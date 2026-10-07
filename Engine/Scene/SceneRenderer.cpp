#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Core/Log.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture.h"
#include "Engine/Animation/Animation.h"
#include "Engine/Asset/AssetManager.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <stb_image.h>

namespace NoJob
{
    namespace
    {
        struct ShadowResources
        {
            GLuint FBO = 0;
            GLuint Directional = 0;
            GLuint Spot = 0;
            GLuint Point = 0;
            GLuint SkyVAO = 0;
            std::shared_ptr<Shader> DepthShader;
            std::shared_ptr<Shader> SkyShader;

            // Renderer V3 environment resources.
            GLuint Environment = 0;
            GLuint Irradiance = 0;
            GLuint Prefilter = 0;
            GLuint BRDFLUT = 0;
            GLuint CaptureFBO = 0;
            GLuint CaptureRBO = 0;
            std::string LoadedEnvironmentPath;
            std::uint32_t LoadedEnvironmentResolution = 0;
            bool EnvironmentReady = false;

            int AllocatedShadowQuality = -1;
            GLuint GPUQueries[3]{0,0,0};
            std::uint32_t GPUQueryWrite = 0;
            bool GPUQueryIssued[3]{false,false,false};
            glm::mat4 DirectionalMatrix{1.0f};
            glm::mat4 SpotMatrix{1.0f};
            glm::vec3 PointPosition{0.0f};
            float PointFar = 25.0f;
            bool HasDirectional = false;
            bool HasSpot = false;
            bool HasPoint = false;
        };

        ShadowResources& Shadows()
        {
            static ShadowResources resources;
            return resources;
        }

        FramebufferSpecification& RendererSettings()
        {
            static FramebufferSpecification settings;
            return settings;
        }


        RendererStatistics& Statistics()
        {
            static RendererStatistics statistics;
            return statistics;
        }

        std::uint64_t MeshIndexCount(const std::shared_ptr<Mesh>& mesh)
        {
            if (!mesh || !mesh->GetVertexArray() ||
                !mesh->GetVertexArray()->GetIndexBuffer())
                return 0;
            return mesh->GetVertexArray()->GetIndexBuffer()->GetCount();
        }

        void BeginGPUFrameQuery()
        {
            auto& r = Shadows();
            if (r.GPUQueries[0] == 0)
                glGenQueries(3, r.GPUQueries);

            // Read a result only when OpenGL says it is ready. This avoids
            // stalling the CPU just to display profiler information.
            const std::uint32_t read = (r.GPUQueryWrite + 1) % 3;
            if (r.GPUQueryIssued[read])
            {
                GLint available = GL_FALSE;
                glGetQueryObjectiv(
                    r.GPUQueries[read], GL_QUERY_RESULT_AVAILABLE, &available);
                if (available == GL_TRUE)
                {
                    GLuint64 nanoseconds = 0;
                    glGetQueryObjectui64v(
                        r.GPUQueries[read], GL_QUERY_RESULT, &nanoseconds);
                    Statistics().GPUTimeMs =
                        static_cast<float>(nanoseconds) / 1000000.0f;
                    Statistics().GPUTimeValid = true;
                    r.GPUQueryIssued[read] = false;
                }
            }
            glBeginQuery(GL_TIME_ELAPSED, r.GPUQueries[r.GPUQueryWrite]);
        }

        void EndGPUFrameQuery()
        {
            auto& r = Shadows();
            glEndQuery(GL_TIME_ELAPSED);
            r.GPUQueryIssued[r.GPUQueryWrite] = true;
            r.GPUQueryWrite = (r.GPUQueryWrite + 1) % 3;
        }

        glm::vec3 WorldPosition(const glm::mat4& world)
        {
            return glm::vec3(world[3]);
        }

        glm::vec3 WorldForward(const glm::mat4& world)
        {
            glm::vec3 forward = -glm::vec3(world[2]);
            const float length = glm::length(forward);
            return length > 0.0001f ? forward / length
                                    : glm::vec3(0.0f, 0.0f, -1.0f);
        }


        GLuint CompileEnvironmentProgram(
            const char* vertexSource, const char* fragmentSource)
        {
            auto compile=[](GLenum type,const char* source)->GLuint
            {
                GLuint shader=glCreateShader(type);
                glShaderSource(shader,1,&source,nullptr);
                glCompileShader(shader);
                GLint ok=GL_FALSE;
                glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
                if(ok!=GL_TRUE)
                {
                    GLint length=0;
                    glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&length);
                    std::string log(static_cast<std::size_t>(length),'\0');
                    glGetShaderInfoLog(shader,length,nullptr,log.data());
                    glDeleteShader(shader);
                    throw std::runtime_error(
                        "Renderer V3 environment shader compile failed: "+log);
                }
                return shader;
            };
            GLuint vs=compile(GL_VERTEX_SHADER,vertexSource);
            GLuint fs=compile(GL_FRAGMENT_SHADER,fragmentSource);
            GLuint program=glCreateProgram();
            glAttachShader(program,vs);glAttachShader(program,fs);
            glLinkProgram(program);
            glDeleteShader(vs);glDeleteShader(fs);
            GLint ok=GL_FALSE;glGetProgramiv(program,GL_LINK_STATUS,&ok);
            if(ok!=GL_TRUE)
            {
                GLint length=0;glGetProgramiv(program,GL_INFO_LOG_LENGTH,&length);
                std::string log(static_cast<std::size_t>(length),'\0');
                glGetProgramInfoLog(program,length,nullptr,log.data());
                glDeleteProgram(program);
                throw std::runtime_error(
                    "Renderer V3 environment shader link failed: "+log);
            }
            return program;
        }

        void DrawCaptureCube()
        {
            static GLuint vao=0;
            if(!vao) glCreateVertexArrays(1,&vao);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES,0,36);
        }

        void DeleteEnvironmentTextures(ShadowResources& r)
        {
            if(r.Environment) glDeleteTextures(1,&r.Environment);
            if(r.Irradiance) glDeleteTextures(1,&r.Irradiance);
            if(r.Prefilter) glDeleteTextures(1,&r.Prefilter);
            if(r.BRDFLUT) glDeleteTextures(1,&r.BRDFLUT);
            r.Environment=r.Irradiance=r.Prefilter=r.BRDFLUT=0;
            r.EnvironmentReady=false;
        }

        const char* CaptureCubeVS=R"(
            #version 460 core
            out vec3 v_Direction;
            uniform mat4 u_ViewProjection;
            const vec3 P[36]=vec3[](
              vec3(-1,-1,-1),vec3(-1,-1, 1),vec3(-1, 1, 1),vec3(-1,-1,-1),vec3(-1, 1, 1),vec3(-1, 1,-1),
              vec3( 1,-1, 1),vec3( 1,-1,-1),vec3( 1, 1,-1),vec3( 1,-1, 1),vec3( 1, 1,-1),vec3( 1, 1, 1),
              vec3(-1,-1, 1),vec3( 1,-1, 1),vec3( 1, 1, 1),vec3(-1,-1, 1),vec3( 1, 1, 1),vec3(-1, 1, 1),
              vec3( 1,-1,-1),vec3(-1,-1,-1),vec3(-1, 1,-1),vec3( 1,-1,-1),vec3(-1, 1,-1),vec3( 1, 1,-1),
              vec3(-1, 1, 1),vec3( 1, 1, 1),vec3( 1, 1,-1),vec3(-1, 1, 1),vec3( 1, 1,-1),vec3(-1, 1,-1),
              vec3(-1,-1,-1),vec3( 1,-1,-1),vec3( 1,-1, 1),vec3(-1,-1,-1),vec3( 1,-1, 1),vec3(-1,-1, 1));
            void main(){v_Direction=P[gl_VertexID];gl_Position=u_ViewProjection*vec4(v_Direction,1.0);}
        )";

        void EnsureEnvironmentResources()
        {
            auto& r=Shadows();
            const auto& settings=RendererSettings();
            const std::string& configuredPath=settings.EnvironmentHDRIPath;
            const std::filesystem::path environmentPath =
                AssetManager::ResolveProjectPath(configuredPath);
            const std::string path = environmentPath.string();

            if(configuredPath.empty())
            {
                if(r.EnvironmentReady) DeleteEnvironmentTextures(r);
                r.LoadedEnvironmentPath.clear();
                return;
            }
            if(r.EnvironmentReady &&
               r.LoadedEnvironmentPath==path &&
               r.LoadedEnvironmentResolution==settings.EnvironmentResolution)
                return;

            int width=0,height=0,channels=0;
            stbi_set_flip_vertically_on_load(1);
            float* pixels=stbi_loadf(path.c_str(),&width,&height,&channels,3);
            stbi_set_flip_vertically_on_load(0);
            if(!pixels)
            {
                if(r.LoadedEnvironmentPath!=path)
                    std::cerr<<"[Renderer V3] Could not load HDRI: "<<path<<"\n";
                r.LoadedEnvironmentPath=path;
                r.EnvironmentReady=false;
                return;
            }

            GLint previousViewport[4]{};
            GLint previousFramebuffer = 0;
            glGetIntegerv(GL_VIEWPORT, previousViewport);
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);

            DeleteEnvironmentTextures(r);
            if(!r.CaptureFBO) glCreateFramebuffers(1,&r.CaptureFBO);
            if(!r.CaptureRBO) glCreateRenderbuffers(1,&r.CaptureRBO);

            GLuint hdr=0;
            glCreateTextures(GL_TEXTURE_2D,1,&hdr);
            glTextureStorage2D(hdr,1,GL_RGB16F,width,height);
            glTextureSubImage2D(
                hdr,0,0,0,width,height,GL_RGB,GL_FLOAT,pixels);
            glTextureParameteri(hdr,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTextureParameteri(hdr,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTextureParameteri(hdr,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTextureParameteri(hdr,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            stbi_image_free(pixels);

            const std::uint32_t size=
                std::clamp(settings.EnvironmentResolution,128u,1024u);
            int mipLevels=1;
            for(std::uint32_t v=size;v>1;v/=2) ++mipLevels;

            auto makeCube=[](GLuint& id,int resolution,int levels)
            {
                glCreateTextures(GL_TEXTURE_CUBE_MAP,1,&id);
                glTextureStorage2D(
                    id,levels,GL_RGB16F,resolution,resolution);
                glTextureParameteri(
                    id,GL_TEXTURE_MIN_FILTER,
                    levels>1?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
                glTextureParameteri(id,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                glTextureParameteri(id,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
                glTextureParameteri(id,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                glTextureParameteri(id,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
            };
            makeCube(r.Environment,size,mipLevels);
            makeCube(r.Irradiance,32,1);
            makeCube(r.Prefilter,128,5);

            glCreateTextures(GL_TEXTURE_2D,1,&r.BRDFLUT);
            glTextureStorage2D(r.BRDFLUT,1,GL_RG16F,512,512);
            glTextureParameteri(r.BRDFLUT,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTextureParameteri(r.BRDFLUT,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTextureParameteri(r.BRDFLUT,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTextureParameteri(r.BRDFLUT,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

            const glm::mat4 projection=glm::perspective(
                glm::radians(90.0f),1.0f,0.1f,10.0f);
            const glm::mat4 views[]={
                glm::lookAt(glm::vec3(0),glm::vec3( 1,0,0),glm::vec3(0,-1,0)),
                glm::lookAt(glm::vec3(0),glm::vec3(-1,0,0),glm::vec3(0,-1,0)),
                glm::lookAt(glm::vec3(0),glm::vec3(0, 1,0),glm::vec3(0,0, 1)),
                glm::lookAt(glm::vec3(0),glm::vec3(0,-1,0),glm::vec3(0,0,-1)),
                glm::lookAt(glm::vec3(0),glm::vec3(0,0, 1),glm::vec3(0,-1,0)),
                glm::lookAt(glm::vec3(0),glm::vec3(0,0,-1),glm::vec3(0,-1,0))};

            const char* equirectFS=R"(
              #version 460 core
              in vec3 v_Direction;out vec4 o_Color;
              uniform sampler2D u_HDR;
              const vec2 invAtan=vec2(0.159154943,0.318309886);
              void main(){vec3 d=normalize(v_Direction);vec2 uv=vec2(atan(d.z,d.x),asin(d.y));uv*=invAtan;uv+=0.5;o_Color=vec4(texture(u_HDR,uv).rgb,1);}
            )";
            GLuint program=CompileEnvironmentProgram(CaptureCubeVS,equirectFS);
            glUseProgram(program);glBindTextureUnit(0,hdr);
            glUniform1i(glGetUniformLocation(program,"u_HDR"),0);
            glNamedRenderbufferStorage(
                r.CaptureRBO,GL_DEPTH_COMPONENT24,size,size);
            glNamedFramebufferRenderbuffer(
                r.CaptureFBO,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,r.CaptureRBO);
            glBindFramebuffer(GL_FRAMEBUFFER,r.CaptureFBO);
            glViewport(0,0,size,size);
            for(int face=0;face<6;++face)
            {
                glm::mat4 vp=projection*views[face];
                glUniformMatrix4fv(
                    glGetUniformLocation(program,"u_ViewProjection"),
                    1,GL_FALSE,&vp[0][0]);
                glNamedFramebufferTextureLayer(
                    r.CaptureFBO,GL_COLOR_ATTACHMENT0,r.Environment,0,face);
                glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                DrawCaptureCube();
            }
            glGenerateTextureMipmap(r.Environment);
            glDeleteProgram(program);glDeleteTextures(1,&hdr);

            const char* irradianceFS=R"(
              #version 460 core
              in vec3 v_Direction;out vec4 o_Color;uniform samplerCube u_Environment;
              const float PI=3.14159265359;
              void main(){vec3 N=normalize(v_Direction);vec3 up=vec3(0,1,0);vec3 right=normalize(cross(up,N));up=normalize(cross(N,right));vec3 irradiance=vec3(0);float samples=0;
                for(float phi=0;phi<2*PI;phi+=0.12)for(float theta=0;theta<0.5*PI;theta+=0.12){vec3 t=vec3(sin(theta)*cos(phi),sin(theta)*sin(phi),cos(theta));vec3 s=t.x*right+t.y*up+t.z*N;irradiance+=texture(u_Environment,s).rgb*cos(theta)*sin(theta);samples++;}
                irradiance=PI*irradiance/max(samples,1.0);o_Color=vec4(irradiance,1);}
            )";
            program=CompileEnvironmentProgram(CaptureCubeVS,irradianceFS);
            glUseProgram(program);glBindTextureUnit(0,r.Environment);
            glUniform1i(glGetUniformLocation(program,"u_Environment"),0);
            glNamedRenderbufferStorage(r.CaptureRBO,GL_DEPTH_COMPONENT24,32,32);
            glViewport(0,0,32,32);
            for(int face=0;face<6;++face)
            {
                glm::mat4 vp=projection*views[face];
                glUniformMatrix4fv(glGetUniformLocation(program,"u_ViewProjection"),1,GL_FALSE,&vp[0][0]);
                glNamedFramebufferTextureLayer(r.CaptureFBO,GL_COLOR_ATTACHMENT0,r.Irradiance,0,face);
                glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);DrawCaptureCube();
            }
            glDeleteProgram(program);

            const char* prefilterFS=R"(
              #version 460 core
              in vec3 v_Direction;out vec4 o_Color;uniform samplerCube u_Environment;uniform float u_Roughness;
              const float PI=3.14159265359;
              float RadicalInverse(uint bits){bits=(bits<<16u)|(bits>>16u);bits=((bits&0x55555555u)<<1u)|((bits&0xAAAAAAAAu)>>1u);bits=((bits&0x33333333u)<<2u)|((bits&0xCCCCCCCCu)>>2u);bits=((bits&0x0F0F0F0Fu)<<4u)|((bits&0xF0F0F0F0u)>>4u);bits=((bits&0x00FF00FFu)<<8u)|((bits&0xFF00FF00u)>>8u);return float(bits)*2.3283064365386963e-10;}
              vec2 Hammersley(uint i,uint n){return vec2(float(i)/float(n),RadicalInverse(i));}
              vec3 ImportanceGGX(vec2 Xi,vec3 N,float rough){float a=rough*rough;float phi=2*PI*Xi.x;float cosTheta=sqrt((1-Xi.y)/(1+(a*a-1)*Xi.y));float sinTheta=sqrt(max(0,1-cosTheta*cosTheta));vec3 H=vec3(cos(phi)*sinTheta,sin(phi)*sinTheta,cosTheta);vec3 up=abs(N.z)<0.999?vec3(0,0,1):vec3(1,0,0);vec3 tangent=normalize(cross(up,N));vec3 bitangent=cross(N,tangent);return normalize(tangent*H.x+bitangent*H.y+N*H.z);}
              void main(){vec3 N=normalize(v_Direction),R=N,V=R;vec3 pre=vec3(0);float weight=0;const uint COUNT=256u;for(uint i=0u;i<COUNT;i++){vec3 H=ImportanceGGX(Hammersley(i,COUNT),N,u_Roughness);vec3 L=normalize(2*dot(V,H)*H-V);float NL=max(dot(N,L),0);if(NL>0){pre+=textureLod(u_Environment,L,u_Roughness*4.0).rgb*NL;weight+=NL;}}o_Color=vec4(pre/max(weight,0.001),1);}
            )";
            program=CompileEnvironmentProgram(CaptureCubeVS,prefilterFS);
            glUseProgram(program);glBindTextureUnit(0,r.Environment);
            glUniform1i(glGetUniformLocation(program,"u_Environment"),0);
            for(int mip=0;mip<5;++mip)
            {
                int mipSize=128>>mip;
                glNamedRenderbufferStorage(r.CaptureRBO,GL_DEPTH_COMPONENT24,mipSize,mipSize);
                glViewport(0,0,mipSize,mipSize);
                glUniform1f(glGetUniformLocation(program,"u_Roughness"),float(mip)/4.0f);
                for(int face=0;face<6;++face)
                {
                    glm::mat4 vp=projection*views[face];
                    glUniformMatrix4fv(glGetUniformLocation(program,"u_ViewProjection"),1,GL_FALSE,&vp[0][0]);
                    glNamedFramebufferTextureLayer(r.CaptureFBO,GL_COLOR_ATTACHMENT0,r.Prefilter,mip,face);
                    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);DrawCaptureCube();
                }
            }
            glDeleteProgram(program);

            const char* fullscreenVS=R"(
              #version 460 core
              out vec2 v_UV;
              void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);v_UV=p;gl_Position=vec4(p*2.0-1.0,0,1);}
            )";
            const char* brdfFS=R"(
              #version 460 core
              in vec2 v_UV;out vec2 o_BRDF;const float PI=3.14159265359;
              float RadicalInverse(uint bits){bits=(bits<<16u)|(bits>>16u);bits=((bits&0x55555555u)<<1u)|((bits&0xAAAAAAAAu)>>1u);bits=((bits&0x33333333u)<<2u)|((bits&0xCCCCCCCCu)>>2u);bits=((bits&0x0F0F0F0Fu)<<4u)|((bits&0xF0F0F0F0u)>>4u);bits=((bits&0x00FF00FFu)<<8u)|((bits&0xFF00FF00u)>>8u);return float(bits)*2.3283064365386963e-10;}
              vec2 Hammersley(uint i,uint n){return vec2(float(i)/float(n),RadicalInverse(i));}
              vec3 ImportanceGGX(vec2 Xi,vec3 N,float rough){float a=rough*rough;float phi=2*PI*Xi.x;float cosTheta=sqrt((1-Xi.y)/(1+(a*a-1)*Xi.y));float sinTheta=sqrt(max(0,1-cosTheta*cosTheta));vec3 H=vec3(cos(phi)*sinTheta,sin(phi)*sinTheta,cosTheta);return H;}
              float G1(float NV,float rough){float k=(rough*rough)*0.5;return NV/(NV*(1-k)+k);}
              vec2 Integrate(float NV,float rough){vec3 V=vec3(sqrt(max(0,1-NV*NV)),0,NV);float A=0,B=0;const uint COUNT=256u;for(uint i=0u;i<COUNT;i++){vec3 H=ImportanceGGX(Hammersley(i,COUNT),vec3(0,0,1),rough);vec3 L=normalize(2*dot(V,H)*H-V);float NL=max(L.z,0),NH=max(H.z,0),VH=max(dot(V,H),0);if(NL>0){float G=G1(NV,rough)*G1(NL,rough);float vis=(G*VH)/max(NH*NV,0.001);float Fc=pow(1-VH,5);A+=(1-Fc)*vis;B+=Fc*vis;}}return vec2(A,B)/float(COUNT);}
              void main(){o_BRDF=Integrate(v_UV.x,v_UV.y);}
            )";
            program=CompileEnvironmentProgram(fullscreenVS,brdfFS);
            glUseProgram(program);
            glNamedFramebufferTexture(r.CaptureFBO,GL_COLOR_ATTACHMENT0,r.BRDFLUT,0);
            glNamedRenderbufferStorage(r.CaptureRBO,GL_DEPTH_COMPONENT24,512,512);
            glViewport(0,0,512,512);glClear(GL_COLOR_BUFFER_BIT);
            static GLuint fsVao=0;if(!fsVao)glCreateVertexArrays(1,&fsVao);
            glBindVertexArray(fsVao);glDrawArrays(GL_TRIANGLES,0,3);
            glDeleteProgram(program);

            glBindFramebuffer(
                GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
            glViewport(
                previousViewport[0], previousViewport[1],
                previousViewport[2], previousViewport[3]);
            r.LoadedEnvironmentPath=path;
            r.LoadedEnvironmentResolution=size;
            r.EnvironmentReady=true;
            std::cout<<"[Renderer V3] HDRI ready: "<<path<<"\n";
        }

        void EnsureShadowResources()
        {
            auto& s = Shadows();

            if (s.FBO == 0)
                glCreateFramebuffers(1, &s.FBO);

            const int quality = std::clamp(RendererSettings().ShadowQuality, 0, 2);
            if (s.AllocatedShadowQuality != quality)
            {
                if (s.Directional) glDeleteTextures(1, &s.Directional);
                if (s.Spot) glDeleteTextures(1, &s.Spot);
                if (s.Point) glDeleteTextures(1, &s.Point);
                s.Directional = s.Spot = s.Point = 0;

                const int directionalSizes[] = {1024, 2048, 4096};
                const int spotSizes[] = {512, 1024, 2048};
                const int pointSizes[] = {256, 512, 1024};

                auto createDepth2D = [](GLuint& texture, int size)
                {
                    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
                    glTextureStorage2D(
                        texture, 1, GL_DEPTH_COMPONENT32F, size, size);
                    glTextureParameteri(
                        texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTextureParameteri(
                        texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTextureParameteri(
                        texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTextureParameteri(
                        texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    const float border[] = {1,1,1,1};
                    glTextureParameterfv(
                        texture, GL_TEXTURE_BORDER_COLOR, border);
                };

                createDepth2D(s.Directional, directionalSizes[quality]);
                createDepth2D(s.Spot, spotSizes[quality]);

                glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &s.Point);
                glTextureStorage2D(
                    s.Point, 1, GL_DEPTH_COMPONENT32F,
                    pointSizes[quality], pointSizes[quality]);
                glTextureParameteri(
                    s.Point, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteri(
                    s.Point, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteri(
                    s.Point, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTextureParameteri(
                    s.Point, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTextureParameteri(
                    s.Point, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

                s.AllocatedShadowQuality = quality;
            }

            // Shader/VAO creation only happens once; texture resolution can be
            // changed independently at runtime.
            if (s.DepthShader)
                return;

            const std::string depthVS = R"(
                #version 460 core
                layout(location=0) in vec3 a_Position;
                layout(location=2) in vec2 a_TexCoord;
                layout(location=3) in vec4 a_BoneIDs;
                layout(location=4) in vec4 a_BoneWeights;
                uniform mat4 u_Transform;
                uniform int u_UseSkinning;
                uniform mat4 u_Bones[128];
                uniform mat4 u_ViewProjection;
                out vec3 v_WorldPosition;
                out vec2 v_TexCoord;
                void main(){
                    vec4 localPosition=vec4(a_Position,1.0);
                    if(u_UseSkinning==1){
                        ivec4 ids=ivec4(a_BoneIDs);
                        mat4 skin=u_Bones[ids.x]*a_BoneWeights.x+
                                  u_Bones[ids.y]*a_BoneWeights.y+
                                  u_Bones[ids.z]*a_BoneWeights.z+
                                  u_Bones[ids.w]*a_BoneWeights.w;
                        if(dot(a_BoneWeights,vec4(1.0))>0.0) localPosition=skin*localPosition;
                    }
                    vec4 w = u_Transform * localPosition;
                    v_WorldPosition = w.xyz;
                    v_TexCoord = a_TexCoord;
                    gl_Position = u_ViewProjection * w;
                })";
            const std::string depthFS = R"(
                #version 460 core
                in vec3 v_WorldPosition;
                in vec2 v_TexCoord;
                uniform sampler2D u_AlphaTexture;
                uniform int u_UseAlphaTexture;
                uniform int u_AlphaClip;
                uniform float u_AlphaCutoff;
                uniform float u_BaseAlpha;
                uniform int u_RadialDepth;
                uniform vec3 u_LightPosition;
                uniform float u_FarPlane;
                void main(){
                    float alpha=u_BaseAlpha;
                    if(u_UseAlphaTexture==1) alpha*=texture(u_AlphaTexture,v_TexCoord).a;
                    if(u_AlphaClip==1 && alpha<u_AlphaCutoff) discard;
                    if(u_RadialDepth==1)
                        gl_FragDepth = length(v_WorldPosition-u_LightPosition)/u_FarPlane;
                })";
            s.DepthShader = Shader::Create(depthVS, depthFS);

            const std::string skyVS = R"(
                #version 460 core
                const vec2 p[3]=vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));
                out vec2 v_UV;
                void main(){ v_UV=p[gl_VertexID]*0.5+0.5; gl_Position=vec4(p[gl_VertexID],1.0,1.0); })";
            const std::string skyFS = R"(
                #version 460 core
                in vec2 v_UV; out vec4 o_Color;
                uniform mat4 u_InverseViewProjection;
                uniform vec3 u_SunDirection;
                uniform vec3 u_SunColor;
                uniform vec3 u_CameraPosition;
                uniform float u_EnvironmentIntensity;
                uniform float u_EnvironmentRotation;
                uniform int u_HasHDRI;
                uniform samplerCube u_EnvironmentMap;
                vec3 RotateY(vec3 d,float angle){
                    float c=cos(angle),s=sin(angle);
                    return vec3(c*d.x+s*d.z,d.y,-s*d.x+c*d.z);
                }
                void main(){
                    vec2 ndc=v_UV*2.0-1.0;
                    vec4 w=u_InverseViewProjection*vec4(ndc,1.0,1.0);
                    vec3 d=normalize(w.xyz/w.w-u_CameraPosition);
                    d=RotateY(d,u_EnvironmentRotation);
                    float h=clamp(d.y*0.5+0.5,0.0,1.0);
                    vec3 horizon=vec3(0.62,0.72,0.86);
                    vec3 zenith=vec3(0.08,0.22,0.48);
                    vec3 c=mix(horizon,zenith,pow(h,0.7));
                    float sun=pow(max(dot(d,normalize(-u_SunDirection)),0.0),512.0);
                    c+=u_SunColor*sun*3.0;
                    if(u_HasHDRI==1)
                        c=textureLod(u_EnvironmentMap,d,0.0).rgb;
                    o_Color=vec4(c*u_EnvironmentIntensity,1.0);
                })";
            s.SkyShader = Shader::Create(skyVS, skyFS);
            glCreateVertexArrays(1, &s.SkyVAO);
        }

        void UploadSkinning(Entity entity, const std::shared_ptr<Mesh>& mesh,
                            const std::shared_ptr<Shader>& shader)
        {
            constexpr std::size_t MaxBones = 128;
            const bool canSkin = mesh && mesh->HasSkinning() &&
                entity.HasComponent<AnimatorComponent>() &&
                entity.GetComponent<AnimatorComponent>().Animation;
            shader->SetInt("u_UseSkinning", canSkin ? 1 : 0);
            if (!canSkin) return;

            const auto& animator = entity.GetComponent<AnimatorComponent>();
            const auto palette = animator.Animation->EvaluatePose(
                animator.ClipIndex, animator.TimeSeconds);
            const std::size_t count = std::min(palette.size(), MaxBones);
            for (std::size_t i=0;i<count;++i)
                shader->SetMat4("u_Bones[" + std::to_string(i) + "]", palette[i]);
        }

        void RenderDepthScene(Scene& scene, const glm::mat4& lightVP,
                              bool radial, const glm::vec3& lightPos,
                              float farPlane)
        {
            auto& s = Shadows();
            s.DepthShader->Bind();
            s.DepthShader->SetInt("u_RadialDepth", radial ? 1 : 0);
            s.DepthShader->SetFloat3("u_LightPosition", lightPos);
            s.DepthShader->SetFloat("u_FarPlane", farPlane);
            for (Entity entity : scene.GetEntities())
            {
                if (!entity.HasComponent<MeshComponent>() ||
                    !entity.HasComponent<MeshRendererComponent>()) continue;
                const auto& mesh = entity.GetComponent<MeshComponent>();
                const auto& renderer = entity.GetComponent<MeshRendererComponent>();
                if (!mesh.MeshAsset) continue;

                auto drawDepth = [&](const std::shared_ptr<Material>& material,
                                     std::uint32_t count,
                                     std::uint32_t offset)
                {
                    const bool clip = material &&
                        material->SurfaceMode() == MaterialSurfaceMode::AlphaClip;
                    s.DepthShader->SetInt("u_AlphaClip", clip ? 1 : 0);
                    s.DepthShader->SetFloat("u_AlphaCutoff",
                        material ? material->AlphaCutoff() : 0.5f);
                    s.DepthShader->SetFloat("u_BaseAlpha",
                        material ? material->GetColor().a : 1.0f);
                    const bool textured = clip && material->IsUsingTexture();
                    s.DepthShader->SetInt("u_UseAlphaTexture", textured ? 1 : 0);
                    s.DepthShader->SetInt("u_AlphaTexture", 0);
                    if (textured) material->GetTexture()->Bind(0);

                    UploadSkinning(entity, mesh.MeshAsset, s.DepthShader);
                    auto& stats = Statistics();
                    ++stats.ShadowDrawCalls;
                    const std::uint64_t indexCount =
                        count ? count : MeshIndexCount(mesh.MeshAsset);
                    stats.ShadowTriangles += indexCount / 3;
                    if (count)
                        Renderer::SubmitRange(mesh.MeshAsset->GetVertexArray(),
                            s.DepthShader, count, offset,
                            scene.GetWorldTransform(entity), lightVP,
                            glm::vec4(1.0f), 0);
                    else
                        Renderer::Submit(mesh.MeshAsset->GetVertexArray(),
                            s.DepthShader, scene.GetWorldTransform(entity),
                            lightVP, glm::vec4(1.0f), 0);
                };

                const auto& subs = mesh.MeshAsset->GetSubmeshes();
                if (!subs.empty() && !renderer.Materials.empty())
                {
                    for (const auto& sub : subs)
                    {
                        auto mat = renderer.GetMaterial(sub.MaterialIndex);
                        if (!mat) mat = renderer.MaterialAsset;
                        drawDepth(mat, sub.IndexCount, sub.IndexOffset);
                    }
                }
                else drawDepth(renderer.MaterialAsset, 0, 0);
            }
        }

        void BuildShadowMaps(Scene& scene)
        {
            EnsureShadowResources();
            auto& s = Shadows();
            s.HasDirectional = s.HasSpot = s.HasPoint = false;
            if (!RendererSettings().Shadows)
                return;

            const int quality =
                std::clamp(RendererSettings().ShadowQuality, 0, 2);
            const int directionalSizes[] = {1024, 2048, 4096};
            const int spotSizes[] = {512, 1024, 2048};
            const int pointSizes[] = {256, 512, 1024};

            GLint oldViewport[4];
            GLint oldFramebuffer = 0;
            glGetIntegerv(GL_VIEWPORT, oldViewport);
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, s.FBO);
            glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
            glEnable(GL_DEPTH_TEST);
            glCullFace(GL_FRONT);

            for (Entity e : scene.GetEntities())
            {
                const glm::mat4 world = scene.GetWorldTransform(e);
                if (!s.HasDirectional && e.HasComponent<DirectionalLightComponent>())
                {
                    const auto& l=e.GetComponent<DirectionalLightComponent>();
                    if (l.CastShadows)
                    {
                        // A directional light has no position: its shadow camera is
                        // rebuilt every frame from the Sun world rotation and fitted
                        // around the renderable scene. This keeps the shadow projection
                        // coupled to the actual Directional Light instead of behaving
                        // like a fixed camera around the world origin.
                        const glm::vec3 dir = WorldForward(world);

                        glm::vec3 boundsMin( 1.0e30f);
                        glm::vec3 boundsMax(-1.0e30f);
                        bool hasCaster = false;
                        for (Entity caster : scene.GetEntities())
                        {
                            if (!caster.HasComponent<MeshComponent>() ||
                                !caster.HasComponent<MeshRendererComponent>())
                                continue;

                            const glm::mat4 casterWorld = scene.GetWorldTransform(caster);
                            const glm::vec3 p = WorldPosition(casterWorld);
                            // We do not have mesh AABBs in the engine yet, so use the
                            // transform scale as a conservative per-entity radius.
                            const glm::vec3 sx = glm::vec3(casterWorld[0]);
                            const glm::vec3 sy = glm::vec3(casterWorld[1]);
                            const glm::vec3 sz = glm::vec3(casterWorld[2]);
                            const float radius = std::max({glm::length(sx), glm::length(sy), glm::length(sz), 1.0f});
                            boundsMin = glm::min(boundsMin, p - glm::vec3(radius));
                            boundsMax = glm::max(boundsMax, p + glm::vec3(radius));
                            hasCaster = true;
                        }

                        const glm::vec3 center = hasCaster
                            ? (boundsMin + boundsMax) * 0.5f
                            : glm::vec3(0.0f);
                        const float sceneRadius = hasCaster
                            ? std::max(glm::length(boundsMax - boundsMin) * 0.5f, 5.0f)
                            : 10.0f;
                        const float orthoExtent = sceneRadius * 1.25f;
                        const float lightDistance = sceneRadius * 2.5f;
                        const glm::vec3 pos = center - dir * lightDistance;
                        const glm::vec3 up = std::abs(glm::dot(dir, glm::vec3(0,1,0))) > 0.95f
                            ? glm::vec3(0,0,1)
                            : glm::vec3(0,1,0);

                        const glm::mat4 lightView = glm::lookAt(pos, center, up);
                        const glm::mat4 lightProjection = glm::ortho(
                            -orthoExtent, orthoExtent,
                            -orthoExtent, orthoExtent,
                            0.1f, lightDistance + sceneRadius * 2.0f);
                        s.DirectionalMatrix = lightProjection * lightView;
                        glNamedFramebufferTexture(s.FBO,GL_DEPTH_ATTACHMENT,s.Directional,0);
                        glViewport(0,0,directionalSizes[quality],directionalSizes[quality]); glClear(GL_DEPTH_BUFFER_BIT);
                        ++Statistics().ShadowPasses;
                        RenderDepthScene(scene,s.DirectionalMatrix,false,pos,60.f);
                        s.HasDirectional=true;
                    }
                }
                if (!s.HasSpot && e.HasComponent<SpotLightComponent>())
                {
                    const auto& l=e.GetComponent<SpotLightComponent>();
                    if (l.CastShadows)
                    {
                        const glm::vec3 pos=WorldPosition(world), dir=WorldForward(world);
                        glm::vec3 up=std::abs(glm::dot(dir,glm::vec3(0,1,0)))>0.95f?glm::vec3(0,0,1):glm::vec3(0,1,0);
                        s.SpotMatrix=glm::perspective(glm::radians(std::min(l.OuterAngle*2.f,175.f)),1.f,0.05f,std::max(l.Range,0.1f))*glm::lookAt(pos,pos+dir,up);
                        glNamedFramebufferTexture(s.FBO,GL_DEPTH_ATTACHMENT,s.Spot,0);
                        glViewport(0,0,spotSizes[quality],spotSizes[quality]); glClear(GL_DEPTH_BUFFER_BIT);
                        ++Statistics().ShadowPasses;
                        RenderDepthScene(scene,s.SpotMatrix,false,pos,l.Range);
                        s.HasSpot=true;
                    }
                }
                if (!s.HasPoint && e.HasComponent<PointLightComponent>())
                {
                    const auto& l=e.GetComponent<PointLightComponent>();
                    if (l.CastShadows)
                    {
                        s.PointPosition=WorldPosition(world); s.PointFar=std::max(l.Range,0.1f);
                        const glm::mat4 proj=glm::perspective(glm::radians(90.f),1.f,0.05f,s.PointFar);
                        const std::array<glm::vec3,6> dirs={glm::vec3(1,0,0),{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                        const std::array<glm::vec3,6> ups={glm::vec3(0,-1,0),{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
                        glViewport(0,0,pointSizes[quality],pointSizes[quality]);
                        for(int face=0;face<6;++face)
                        {
                            ++Statistics().ShadowPasses;
                            glNamedFramebufferTextureLayer(s.FBO,GL_DEPTH_ATTACHMENT,s.Point,0,face);
                            glClear(GL_DEPTH_BUFFER_BIT);
                            RenderDepthScene(scene,proj*glm::lookAt(s.PointPosition,s.PointPosition+dirs[face],ups[face]),true,s.PointPosition,s.PointFar);
                        }
                        s.HasPoint=true;
                    }
                }
            }
            glCullFace(GL_BACK);
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFramebuffer));
            glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
        }

        void RenderSky(const glm::mat4& viewProjection, const glm::vec3& cameraPosition, Scene& scene)
        {
            auto& s=Shadows();
            glm::vec3 sunDir(0.2f,-1.0f,0.2f), sunColor(1.0f,0.95f,0.85f);
            for(Entity e:scene.GetEntities()) if(e.HasComponent<DirectionalLightComponent>())
            { sunDir=WorldForward(scene.GetWorldTransform(e)); sunColor=e.GetComponent<DirectionalLightComponent>().Color; break; }
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);
            s.SkyShader->Bind();
            s.SkyShader->SetMat4("u_InverseViewProjection",glm::inverse(viewProjection));
            s.SkyShader->SetFloat3("u_SunDirection",sunDir);
            s.SkyShader->SetFloat3("u_SunColor",sunColor);
            s.SkyShader->SetFloat3("u_CameraPosition",cameraPosition);
            const auto& graphics = RendererSettings();
            s.SkyShader->SetFloat(
                "u_EnvironmentIntensity", graphics.EnvironmentIntensity);
            s.SkyShader->SetFloat(
                "u_EnvironmentRotation",
                glm::radians(graphics.EnvironmentRotation));
            s.SkyShader->SetInt(
                "u_HasHDRI", s.EnvironmentReady ? 1 : 0);
            s.SkyShader->SetInt("u_EnvironmentMap", 12);
            if (s.EnvironmentReady)
                glBindTextureUnit(12, s.Environment);
            glBindVertexArray(s.SkyVAO); glDrawArrays(GL_TRIANGLES,0,3); glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
        }

        void UploadLighting(Scene& scene, const std::shared_ptr<Shader>& shader,
                            const glm::vec3& cameraPosition)
        {
            auto& s=Shadows();
            shader->Bind();
            shader->SetFloat3("u_ViewPosition",cameraPosition);
            shader->SetFloat3("u_AmbientColor",{0.035f,0.045f,0.065f});
            const auto& graphics = RendererSettings();
            shader->SetInt("u_UseIBL", graphics.ImageBasedLighting ? 1 : 0);
            shader->SetFloat(
                "u_EnvironmentIntensity", graphics.EnvironmentIntensity);
            shader->SetFloat(
                "u_DiffuseIBLStrength", graphics.DiffuseIBLStrength);
            shader->SetFloat(
                "u_SpecularIBLStrength", graphics.SpecularIBLStrength);
            shader->SetFloat(
                "u_EnvironmentRotation",
                glm::radians(graphics.EnvironmentRotation));
            auto& environment = Shadows();
            shader->SetInt(
                "u_HasHDRI", environment.EnvironmentReady ? 1 : 0);
            shader->SetInt("u_IrradianceMap", 9);
            shader->SetInt("u_PrefilterMap", 10);
            shader->SetInt("u_BRDFLUT", 11);
            if (environment.EnvironmentReady)
            {
                glBindTextureUnit(9, environment.Irradiance);
                glBindTextureUnit(10, environment.Prefilter);
                glBindTextureUnit(11, environment.BRDFLUT);
            }
            bool hasDirectional=false; int pointCount=0,spotCount=0;
            for(Entity e:scene.GetEntities())
            {
                const glm::mat4 world=scene.GetWorldTransform(e);
                if(!hasDirectional && e.HasComponent<DirectionalLightComponent>())
                {
                    const auto& l=e.GetComponent<DirectionalLightComponent>();
                    shader->SetInt("u_HasDirectionalLight",1);
                    shader->SetFloat3("u_DirectionalLight.direction",WorldForward(world));
                    shader->SetFloat3("u_DirectionalLight.color",l.Color);
                    shader->SetFloat("u_DirectionalLight.intensity",std::max(l.Intensity,0.f));
                    shader->SetFloat("u_DirectionalShadowBias",l.ShadowBias);
                    hasDirectional=true;
                }
                if(pointCount<4 && e.HasComponent<PointLightComponent>())
                {
                    const auto& l=e.GetComponent<PointLightComponent>(); const std::string b="u_PointLights["+std::to_string(pointCount)+"]";
                    shader->SetFloat3(b+".position",WorldPosition(world)); shader->SetFloat3(b+".color",l.Color);
                    shader->SetFloat(b+".intensity",std::max(l.Intensity,0.f)); shader->SetFloat(b+".range",std::max(l.Range,0.01f));
                    shader->SetFloat("u_PointShadowBias",l.ShadowBias); ++pointCount;
                }
                if(spotCount<4 && e.HasComponent<SpotLightComponent>())
                {
                    const auto& l=e.GetComponent<SpotLightComponent>(); const std::string b="u_SpotLights["+std::to_string(spotCount)+"]";
                    shader->SetFloat3(b+".position",WorldPosition(world)); shader->SetFloat3(b+".direction",WorldForward(world)); shader->SetFloat3(b+".color",l.Color);
                    shader->SetFloat(b+".intensity",std::max(l.Intensity,0.f)); shader->SetFloat(b+".range",std::max(l.Range,0.01f));
                    shader->SetFloat(b+".innerCos",std::cos(glm::radians(l.InnerAngle))); shader->SetFloat(b+".outerCos",std::cos(glm::radians(l.OuterAngle)));
                    shader->SetFloat("u_SpotShadowBias",l.ShadowBias); ++spotCount;
                }
            }
            shader->SetInt("u_HasDirectionalLight",hasDirectional?1:0); shader->SetInt("u_PointLightCount",pointCount); shader->SetInt("u_SpotLightCount",spotCount);
            shader->SetInt("u_HasDirectionalShadow",s.HasDirectional?1:0); shader->SetInt("u_HasPointShadow",s.HasPoint?1:0); shader->SetInt("u_HasSpotShadow",s.HasSpot?1:0);
            shader->SetMat4("u_DirectionalLightSpace",s.DirectionalMatrix); shader->SetMat4("u_SpotLightSpace",s.SpotMatrix);
            shader->SetFloat3("u_PointShadowPosition",s.PointPosition); shader->SetFloat("u_PointShadowFar",s.PointFar);
            glBindTextureUnit(5,s.Directional); glBindTextureUnit(6,s.Point); glBindTextureUnit(7,s.Spot);
            shader->SetInt("u_DirectionalShadowMap",5); shader->SetInt("u_PointShadowMap",6); shader->SetInt("u_SpotShadowMap",7);
        }
    }


    namespace
    {
        struct ParticleInstanceGPU
        {
            glm::vec3 Position;
            float Size;
            glm::vec4 Color;
        };

        struct ParticleRenderResources
        {
            GLuint VAO = 0;
            GLuint VBO = 0;
            GLuint InstanceVBO = 0;
            std::shared_ptr<Shader> ShaderProgram;
            std::unordered_map<std::string, std::shared_ptr<Texture2D>> Textures;
        };

        ParticleRenderResources& ParticleResources()
        {
            static ParticleRenderResources r;
            return r;
        }

        void EnsureParticleResources()
        {
            auto& r = ParticleResources();
            if (r.VAO != 0) return;

            // XY + UV unit quad. Per-particle position/size/color live in an
            // instanced buffer, reducing an emitter to one draw call.
            const float vertices[] = {
                -0.5f,-0.5f, 0,0,   0.5f,-0.5f, 1,0,   0.5f, 0.5f, 1,1,
                -0.5f,-0.5f, 0,0,   0.5f, 0.5f, 1,1,  -0.5f, 0.5f, 0,1
            };
            glGenVertexArrays(1, &r.VAO);
            glGenBuffers(1, &r.VBO);
            glGenBuffers(1, &r.InstanceVBO);
            glBindVertexArray(r.VAO);

            glBindBuffer(GL_ARRAY_BUFFER, r.VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
                                  reinterpret_cast<void*>(2*sizeof(float)));

            glBindBuffer(GL_ARRAY_BUFFER, r.InstanceVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(ParticleInstanceGPU), nullptr, GL_STREAM_DRAW);
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleInstanceGPU),
                                  reinterpret_cast<void*>(offsetof(ParticleInstanceGPU, Position)));
            glVertexAttribDivisor(2, 1);
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstanceGPU),
                                  reinterpret_cast<void*>(offsetof(ParticleInstanceGPU, Size)));
            glVertexAttribDivisor(3, 1);
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleInstanceGPU),
                                  reinterpret_cast<void*>(offsetof(ParticleInstanceGPU, Color)));
            glVertexAttribDivisor(4, 1);
            glBindVertexArray(0);

            const std::string vs = R"(
                #version 460 core
                layout(location=0) in vec2 a_Position;
                layout(location=1) in vec2 a_UV;
                layout(location=2) in vec3 i_WorldPosition;
                layout(location=3) in float i_Size;
                layout(location=4) in vec4 i_Color;
                uniform mat4 u_ViewProjection;
                uniform vec3 u_CameraRight;
                uniform vec3 u_CameraUp;
                out vec2 v_UV;
                out vec4 v_Color;
                void main() {
                    vec3 world = i_WorldPosition +
                        u_CameraRight * a_Position.x * i_Size +
                        u_CameraUp * a_Position.y * i_Size;
                    gl_Position = u_ViewProjection * vec4(world,1.0);
                    v_UV = a_UV;
                    v_Color = i_Color;
                })";
            const std::string fs = R"(
                #version 460 core
                layout(location=0) out vec4 o_Color;
                in vec2 v_UV;
                in vec4 v_Color;
                uniform sampler2D u_Texture;
                uniform int u_UseTexture;
                void main() {
                    vec4 texel = u_UseTexture != 0 ? texture(u_Texture, v_UV) : vec4(1.0);
                    o_Color = texel * v_Color;
                    if (o_Color.a <= 0.003) discard;
                })";
            r.ShaderProgram = Shader::Create(vs, fs);
        }

        std::shared_ptr<Texture2D> ParticleTexture(const std::string& path)
        {
            if (path.empty()) return {};
            auto& cache = ParticleResources().Textures;
            auto it = cache.find(path);
            if (it != cache.end()) return it->second;
            try
            {
                auto texture = AssetManager::LoadTexture(path);
                cache[path] = texture;
                return texture;
            }
            catch (...) { Log::Warn("Particle texture load failed: " + path); return {}; }
        }

        void RenderParticles(Scene& scene, const glm::mat4& viewProjection,
                             RendererStatistics& stats)
        {
            EnsureParticleResources();
            auto& r = ParticleResources();
            if (!r.ShaderProgram) return;

            const glm::mat4 inverseVP = glm::inverse(viewProjection);
            const glm::vec3 right = glm::normalize(glm::vec3(inverseVP[0]));
            const glm::vec3 up = glm::normalize(glm::vec3(inverseVP[1]));

            glEnable(GL_BLEND);
            glDepthMask(GL_FALSE);
            r.ShaderProgram->Bind();
            r.ShaderProgram->SetMat4("u_ViewProjection", viewProjection);
            r.ShaderProgram->SetFloat3("u_CameraRight", right);
            r.ShaderProgram->SetFloat3("u_CameraUp", up);
            r.ShaderProgram->SetInt("u_Texture", 0);
            glBindVertexArray(r.VAO);

            std::vector<ParticleInstanceGPU> instances;
            for (Entity entity : scene.GetEntities())
            {
                if (!entity.HasComponent<ParticleSystemComponent>()) continue;
                const auto& settings = entity.GetComponent<ParticleSystemComponent>();
                const auto& particles = scene.GetParticles(entity.GetHandle());
                if (particles.empty()) continue;

                instances.clear();
                instances.reserve(particles.size());
                for (const auto& p : particles)
                    instances.push_back({p.Position, p.Size, p.Color});

                if (settings.BlendMode == ParticleBlendMode::Additive)
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                else
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                auto texture = ParticleTexture(settings.TexturePath);
                r.ShaderProgram->SetInt("u_UseTexture", texture ? 1 : 0);
                if (texture) texture->Bind(0);

                glBindBuffer(GL_ARRAY_BUFFER, r.InstanceVBO);
                glBufferData(GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(instances.size() * sizeof(ParticleInstanceGPU)),
                    instances.data(), GL_STREAM_DRAW);
                glDrawArraysInstanced(GL_TRIANGLES, 0, 6,
                    static_cast<GLsizei>(instances.size()));

                ++stats.DrawCalls;
                stats.Triangles += static_cast<std::uint64_t>(instances.size()) * 2;
            }

            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
    }

    void SceneRenderer::Render(Scene& scene, const glm::mat4& viewProjection,
                               const glm::vec3& cameraPosition,
                               bool rebuildShadowMaps)
    {
        const auto cpuStart = std::chrono::steady_clock::now();
        auto& stats = Statistics();
        const float previousGPUTime = stats.GPUTimeMs;
        const bool previousGPUValid = stats.GPUTimeValid;
        stats = {};
        stats.GPUTimeMs = previousGPUTime;
        stats.GPUTimeValid = previousGPUValid;
        BeginGPUFrameQuery();

        EnsureEnvironmentResources();
        if (rebuildShadowMaps)
            BuildShadowMaps(scene);
        RenderSky(viewProjection,cameraPosition,scene);

        for(Entity entity:scene.GetEntities())
        {
            if(!entity.HasComponent<MeshComponent>() ||
               !entity.HasComponent<MeshRendererComponent>()) continue;

            const auto& mesh=entity.GetComponent<MeshComponent>();
            const auto& renderer=entity.GetComponent<MeshRendererComponent>();
            if(!mesh.MeshAsset || !renderer.MaterialAsset) continue;
            const glm::mat4 entityWorld = scene.GetWorldTransform(entity);

            auto drawMaterial=[&](const std::shared_ptr<Material>& material,
                                  std::uint32_t count,std::uint32_t offset)
            {
                if(!material || !material->GetShader()) return;
                const auto& shader=material->GetShader();
                UploadLighting(scene,shader,cameraPosition);
                shader->SetInt("u_SurfaceMode", static_cast<int>(material->SurfaceMode()));
                shader->SetFloat("u_AlphaCutoff", material->AlphaCutoff());

                const bool transparent =
                    material->SurfaceMode() == MaterialSurfaceMode::Transparent;
                if (transparent)
                {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                }
                else
                {
                    glDisable(GL_BLEND);
                    glDepthMask(GL_TRUE);
                }

                shader->SetFloat("u_Metallic",glm::clamp(material->Metallic(),0.f,1.f));
                shader->SetFloat("u_Roughness",glm::clamp(material->Roughness(),0.04f,1.f));
                shader->SetFloat("u_AO",glm::clamp(material->AmbientOcclusion(),0.f,1.f));
                shader->SetFloat("u_NormalStrength",glm::max(material->NormalStrength(),0.f));
                shader->SetFloat3("u_EmissiveColor",material->EmissiveColor());
                shader->SetFloat("u_EmissiveStrength",glm::max(material->EmissiveStrength(),0.f));
                if(material->IsUsingTexture()) material->GetTexture()->Bind(0);
                shader->SetInt("u_Texture",0);

                const auto bindMap=[&](const std::shared_ptr<Texture2D>& tex,
                                      const char* sampler,const char* enabled,int slot)
                {
                    shader->SetInt(enabled,tex?1:0);shader->SetInt(sampler,slot);
                    if(tex)tex->Bind(static_cast<std::uint32_t>(slot));
                };
                bindMap(material->GetNormalTexture(),"u_NormalMap","u_UseNormalMap",1);
                bindMap(material->GetMetallicTexture(),"u_MetallicMap","u_UseMetallicMap",2);
                bindMap(material->GetRoughnessTexture(),"u_RoughnessMap","u_UseRoughnessMap",3);
                bindMap(material->GetAOTexture(),"u_AOMap","u_UseAOMap",4);
                bindMap(material->GetEmissiveTexture(),"u_EmissiveMap","u_UseEmissiveMap",8);

                UploadSkinning(entity, mesh.MeshAsset, shader);
                ++stats.DrawCalls;
                const std::uint64_t indexCount =
                    count ? count : MeshIndexCount(mesh.MeshAsset);
                stats.Triangles += indexCount / 3;
                if(count)
                    Renderer::SubmitRange(mesh.MeshAsset->GetVertexArray(),shader,
                        count,offset,entityWorld,viewProjection,
                        material->GetColor(),material->IsUsingTexture()?1:0);
                else
                    Renderer::Submit(mesh.MeshAsset->GetVertexArray(),shader,
                        entityWorld,viewProjection,
                        material->GetColor(),material->IsUsingTexture()?1:0);

                if (transparent)
                {
                    glDepthMask(GL_TRUE);
                    glDisable(GL_BLEND);
                }
            };

            const auto& subs=mesh.MeshAsset->GetSubmeshes();
            if(!subs.empty() && !renderer.Materials.empty())
            {
                for(const auto& sub:subs)
                {
                    auto mat=renderer.GetMaterial(sub.MaterialIndex);
                    if(!mat)mat=renderer.MaterialAsset;
                    drawMaterial(mat,sub.IndexCount,sub.IndexOffset);
                }
            }
            else drawMaterial(renderer.MaterialAsset,0,0);
        }

        RenderParticles(scene, viewProjection, stats);

        EndGPUFrameQuery();
        const auto cpuEnd = std::chrono::steady_clock::now();
        stats.CPUTimeMs =
            std::chrono::duration<float, std::milli>(
                cpuEnd - cpuStart).count();
    }

    void SceneRenderer::Shutdown()
    {
        // Explicitly release renderer-owned static GL resources before the
        // GLFW window/context disappears. Leaving shared_ptr<Shader> objects
        // in function statics until process teardown can invoke their OpenGL
        // destructors after the context has already been destroyed.
        auto& particles = ParticleResources();
        particles.ShaderProgram.reset();
        particles.Textures.clear();
        if (particles.InstanceVBO != 0)
        {
            glDeleteBuffers(1, &particles.InstanceVBO);
            particles.InstanceVBO = 0;
        }
        if (particles.VBO != 0)
        {
            glDeleteBuffers(1, &particles.VBO);
            particles.VBO = 0;
        }
        if (particles.VAO != 0)
        {
            glDeleteVertexArrays(1, &particles.VAO);
            particles.VAO = 0;
        }
    }

    void SceneRenderer::SetGraphicsSettings(
        const FramebufferSpecification& settings)
    {
        RendererSettings() = settings;
    }

    const FramebufferSpecification& SceneRenderer::GetGraphicsSettings()
    {
        return RendererSettings();
    }

    const RendererStatistics& SceneRenderer::GetStatistics()
    {
        return Statistics();
    }

}
