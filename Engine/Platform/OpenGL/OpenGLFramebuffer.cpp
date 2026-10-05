#include "Engine/Platform/OpenGL/OpenGLFramebuffer.h"

#include <glad/gl.h>
#include <stdexcept>
#include <string>

namespace NoJob
{
    namespace
    {
        GLuint CompileStage(GLenum type, const char* source)
        {
            const GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);
            GLint ok = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                GLint length = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                std::string log(static_cast<size_t>(length), '\0');
                glGetShaderInfoLog(shader, length, nullptr, log.data());
                glDeleteShader(shader);
                throw std::runtime_error("NoJobEngine post-process shader compilation failed: " + log);
            }
            return shader;
        }

        GLuint CreatePostProgram()
        {
            const char* vs = R"(
                #version 460 core
                const vec2 P[3]=vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));
                out vec2 v_UV;
                void main(){v_UV=P[gl_VertexID]*0.5+0.5;gl_Position=vec4(P[gl_VertexID],0.0,1.0);}
            )";
            const char* fs = R"(
                #version 460 core
                in vec2 v_UV; layout(location=0) out vec4 o_Color;
                uniform sampler2D u_HDR;
                uniform sampler2D u_Depth;
                uniform vec2 u_InvResolution;
                uniform float u_Exposure,u_BloomThreshold,u_BloomStrength,u_AOIntensity;
                uniform int u_Bloom,u_FXAA,u_AO;

                vec3 SampleHDR(vec2 uv){return texture(u_HDR,clamp(uv,vec2(0.0),vec2(1.0))).rgb;}
                float Luma(vec3 c){return dot(c,vec3(0.299,0.587,0.114));}
                vec3 ACES(vec3 x){
                    const float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
                    return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);
                }
                vec3 FXAA(vec2 uv){
                    vec3 rgbM=SampleHDR(uv);
                    float lumaM=Luma(rgbM);
                    float lumaN=Luma(SampleHDR(uv+vec2(0.0,-u_InvResolution.y)));
                    float lumaS=Luma(SampleHDR(uv+vec2(0.0, u_InvResolution.y)));
                    float lumaW=Luma(SampleHDR(uv+vec2(-u_InvResolution.x,0.0)));
                    float lumaE=Luma(SampleHDR(uv+vec2( u_InvResolution.x,0.0)));
                    float mn=min(lumaM,min(min(lumaN,lumaS),min(lumaW,lumaE)));
                    float mx=max(lumaM,max(max(lumaN,lumaS),max(lumaW,lumaE)));
                    if(mx-mn < max(0.0312,mx*0.125)) return rgbM;
                    vec2 dir=vec2(-(lumaN-lumaS),lumaW-lumaE);
                    dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+0.0001),vec2(-8.0),vec2(8.0))*u_InvResolution;
                    return (SampleHDR(uv+dir*(1.0/3.0-0.5))+SampleHDR(uv+dir*(2.0/3.0-0.5)))*0.5;
                }
                float ScreenAO(vec2 uv){
                    float center=texture(u_Depth,uv).r;
                    if(center>=0.99999) return 1.0;
                    float occ=0.0;
                    const vec2 dirs[8]=vec2[8](vec2(1,0),vec2(-1,0),vec2(0,1),vec2(0,-1),
                                               vec2(1,1),vec2(-1,1),vec2(1,-1),vec2(-1,-1));
                    for(int i=0;i<8;i++){
                        float d=texture(u_Depth,uv+dirs[i]*u_InvResolution*3.0).r;
                        occ += smoothstep(0.0004,0.012,center-d);
                    }
                    return 1.0-(occ/8.0)*u_AOIntensity;
                }
                vec3 Bloom(vec2 uv){
                    vec3 b=vec3(0.0); float w=0.0;
                    for(int x=-4;x<=4;x++) for(int y=-4;y<=4;y++){
                        vec2 o=vec2(x,y)*u_InvResolution*2.0;
                        vec3 c=SampleHDR(uv+o);
                        float bright=max(Luma(c)-u_BloomThreshold,0.0);
                        float ww=exp(-float(x*x+y*y)/10.0);
                        b+=c*(bright/(bright+1.0))*ww; w+=ww;
                    }
                    return b/max(w,0.0001);
                }
                void main(){
                    vec3 hdr=(u_FXAA==1)?FXAA(v_UV):SampleHDR(v_UV);
                    if(u_AO==1) hdr*=ScreenAO(v_UV);
                    if(u_Bloom==1) hdr+=Bloom(v_UV)*u_BloomStrength;
                    vec3 mapped=ACES(hdr*max(u_Exposure,0.001));
                    mapped=pow(mapped,vec3(1.0/2.2));
                    o_Color=vec4(mapped,1.0);
                }
            )";
            GLuint v=CompileStage(GL_VERTEX_SHADER,vs), f=CompileStage(GL_FRAGMENT_SHADER,fs);
            GLuint program=glCreateProgram();
            glAttachShader(program,v); glAttachShader(program,f); glLinkProgram(program);
            glDeleteShader(v); glDeleteShader(f);
            GLint ok=GL_FALSE; glGetProgramiv(program,GL_LINK_STATUS,&ok);
            if(!ok){glDeleteProgram(program);throw std::runtime_error("NoJobEngine post-process shader link failed.");}
            return program;
        }
    }

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& specification)
        : m_Specification(specification)
    {
        Invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer()
    {
        DestroyAttachments();
        if(m_PostProgram) glDeleteProgram(m_PostProgram);
        if(m_FullscreenVAO) glDeleteVertexArrays(1,&m_FullscreenVAO);
    }

    void OpenGLFramebuffer::DestroyAttachments()
    {
        if(m_RendererID) glDeleteFramebuffers(1,&m_RendererID);
        if(m_PostProcessFBO) glDeleteFramebuffers(1,&m_PostProcessFBO);
        if(m_ColorAttachment) glDeleteTextures(1,&m_ColorAttachment);
        if(m_DisplayAttachment) glDeleteTextures(1,&m_DisplayAttachment);
        if(m_DepthAttachment) glDeleteTextures(1,&m_DepthAttachment);
        m_RendererID=m_PostProcessFBO=m_ColorAttachment=m_DisplayAttachment=m_DepthAttachment=0;
    }

    void OpenGLFramebuffer::EnsurePostProcessResources()
    {
        if(!m_PostProgram) m_PostProgram=CreatePostProgram();
        if(!m_FullscreenVAO) glCreateVertexArrays(1,&m_FullscreenVAO);
    }

    void OpenGLFramebuffer::Invalidate()
    {
        DestroyAttachments();
        EnsurePostProcessResources();

        const GLsizei w=static_cast<GLsizei>(m_Specification.Width);
        const GLsizei h=static_cast<GLsizei>(m_Specification.Height);

        glCreateFramebuffers(1,&m_RendererID);
        glCreateTextures(GL_TEXTURE_2D,1,&m_ColorAttachment);
        glTextureStorage2D(m_ColorAttachment,1,m_Specification.HDR?GL_RGBA16F:GL_RGBA8,w,h);
        glTextureParameteri(m_ColorAttachment,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTextureParameteri(m_ColorAttachment,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTextureParameteri(m_ColorAttachment,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_ColorAttachment,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(m_RendererID,GL_COLOR_ATTACHMENT0,m_ColorAttachment,0);

        glCreateTextures(GL_TEXTURE_2D,1,&m_DepthAttachment);
        glTextureStorage2D(m_DepthAttachment,1,GL_DEPTH24_STENCIL8,w,h);
        glTextureParameteri(m_DepthAttachment,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTextureParameteri(m_DepthAttachment,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTextureParameteri(m_DepthAttachment,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_DepthAttachment,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(m_RendererID,GL_DEPTH_STENCIL_ATTACHMENT,m_DepthAttachment,0);

        glCreateFramebuffers(1,&m_PostProcessFBO);
        glCreateTextures(GL_TEXTURE_2D,1,&m_DisplayAttachment);
        glTextureStorage2D(m_DisplayAttachment,1,GL_RGBA8,w,h);
        glTextureParameteri(m_DisplayAttachment,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTextureParameteri(m_DisplayAttachment,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTextureParameteri(m_DisplayAttachment,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_DisplayAttachment,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(m_PostProcessFBO,GL_COLOR_ATTACHMENT0,m_DisplayAttachment,0);

        if(glCheckNamedFramebufferStatus(m_RendererID,GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE ||
           glCheckNamedFramebufferStatus(m_PostProcessFBO,GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("NoJobEngine: advanced framebuffer is incomplete.");
    }

    void OpenGLFramebuffer::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER,m_RendererID);
        glViewport(0,0,(GLsizei)m_Specification.Width,(GLsizei)m_Specification.Height);
    }

    void OpenGLFramebuffer::RunPostProcess()
    {
        // Post-processing is executed in the middle of the editor render loop.
        // Preserve every GL state we touch; ImGui's multi-viewport Win32 backend
        // may switch platform windows/contexts later in glfwPollEvents().
        GLint previousDrawFBO = 0;
        GLint previousReadFBO = 0;
        GLint previousProgram = 0;
        GLint previousVAO = 0;
        GLint previousViewport[4] = { 0, 0, 0, 0 };
        const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFBO);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFBO);
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
        glGetIntegerv(GL_VIEWPORT, previousViewport);

        glBindFramebuffer(GL_FRAMEBUFFER,m_PostProcessFBO);
        glViewport(0,0,(GLsizei)m_Specification.Width,(GLsizei)m_Specification.Height);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(m_PostProgram);
        glBindTextureUnit(0,m_ColorAttachment);
        glBindTextureUnit(1,m_DepthAttachment);
        glUniform1i(glGetUniformLocation(m_PostProgram,"u_HDR"),0);
        glUniform1i(glGetUniformLocation(m_PostProgram,"u_Depth"),1);
        glUniform2f(glGetUniformLocation(m_PostProgram,"u_InvResolution"),
                    1.0f/(float)m_Specification.Width,1.0f/(float)m_Specification.Height);
        glUniform1f(glGetUniformLocation(m_PostProgram,"u_Exposure"),m_Specification.Exposure);
        glUniform1f(glGetUniformLocation(m_PostProgram,"u_BloomThreshold"),m_Specification.BloomThreshold);
        glUniform1f(glGetUniformLocation(m_PostProgram,"u_BloomStrength"),m_Specification.BloomStrength);
        glUniform1f(glGetUniformLocation(m_PostProgram,"u_AOIntensity"),m_Specification.AOIntensity);
        glUniform1i(glGetUniformLocation(m_PostProgram,"u_Bloom"),m_Specification.Bloom?1:0);
        glUniform1i(glGetUniformLocation(m_PostProgram,"u_FXAA"),m_Specification.FXAA?1:0);
        glUniform1i(glGetUniformLocation(m_PostProgram,"u_AO"),m_Specification.ScreenSpaceAO?1:0);
        glBindVertexArray(m_FullscreenVAO);
        glDrawArrays(GL_TRIANGLES,0,3);
        glBindVertexArray(static_cast<GLuint>(previousVAO));
        glUseProgram(static_cast<GLuint>(previousProgram));
        if(depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(previousDrawFBO));
        glBindFramebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(previousReadFBO));
        glViewport(previousViewport[0],previousViewport[1],previousViewport[2],previousViewport[3]);
    }

    void OpenGLFramebuffer::Unbind()
    {
        // Resolve while the GLFW/OpenGL context that rendered this framebuffer
        // is still current, then return to the default framebuffer expected by
        // the editor/ImGui platform backend.
        RunPostProcess();
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }

    void OpenGLFramebuffer::Resize(std::uint32_t width,std::uint32_t height)
    {
        // Docking/minimize can briefly report invalid sizes. Never recreate GL
        // resources from those transient values.
        if(width==0||height==0||width>16384||height>16384) return;
        if(width==m_Specification.Width&&height==m_Specification.Height) return;
        m_Specification.Width=width; m_Specification.Height=height; Invalidate();
    }
}
