#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

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

        void EnsureShadowResources()
        {
            auto& s = Shadows();
            if (s.FBO != 0)
                return;

            glCreateFramebuffers(1, &s.FBO);

            auto createDepth2D = [](GLuint& texture, int size)
            {
                glCreateTextures(GL_TEXTURE_2D, 1, &texture);
                glTextureStorage2D(texture, 1, GL_DEPTH_COMPONENT32F, size, size);
                glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                const float border[] = {1,1,1,1};
                glTextureParameterfv(texture, GL_TEXTURE_BORDER_COLOR, border);
            };

            createDepth2D(s.Directional, 2048);
            createDepth2D(s.Spot, 1024);

            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &s.Point);
            glTextureStorage2D(s.Point, 1, GL_DEPTH_COMPONENT32F, 512, 512);
            glTextureParameteri(s.Point, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(s.Point, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(s.Point, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(s.Point, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(s.Point, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            const std::string depthVS = R"(
                #version 460 core
                layout(location=0) in vec3 a_Position;
                layout(location=2) in vec2 a_TexCoord;
                uniform mat4 u_Transform;
                uniform mat4 u_ViewProjection;
                out vec3 v_WorldPosition;
                out vec2 v_TexCoord;
                void main(){
                    vec4 w = u_Transform * vec4(a_Position,1.0);
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
                void main(){
                    vec2 ndc=v_UV*2.0-1.0;
                    vec4 w=u_InverseViewProjection*vec4(ndc,1.0,1.0);
                    vec3 d=normalize(w.xyz/w.w-u_CameraPosition);
                    float h=clamp(d.y*0.5+0.5,0.0,1.0);
                    vec3 horizon=vec3(0.62,0.72,0.86);
                    vec3 zenith=vec3(0.08,0.22,0.48);
                    vec3 c=mix(horizon,zenith,pow(h,0.7));
                    float sun=pow(max(dot(d,normalize(-u_SunDirection)),0.0),512.0);
                    c+=u_SunColor*sun*3.0;
                    o_Color=vec4(c,1.0);
                })";
            s.SkyShader = Shader::Create(skyVS, skyFS);
            glCreateVertexArrays(1, &s.SkyVAO);
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
                        glViewport(0,0,2048,2048); glClear(GL_DEPTH_BUFFER_BIT);
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
                        glViewport(0,0,1024,1024); glClear(GL_DEPTH_BUFFER_BIT);
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
                        glViewport(0,0,512,512);
                        for(int face=0;face<6;++face)
                        {
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

    void SceneRenderer::Render(Scene& scene, const glm::mat4& viewProjection,
                               const glm::vec3& cameraPosition)
    {
        BuildShadowMaps(scene);
        RenderSky(viewProjection,cameraPosition,scene);

        for(Entity entity:scene.GetEntities())
        {
            if(!entity.HasComponent<MeshComponent>() ||
               !entity.HasComponent<MeshRendererComponent>()) continue;

            const auto& mesh=entity.GetComponent<MeshComponent>();
            const auto& renderer=entity.GetComponent<MeshRendererComponent>();
            if(!mesh.MeshAsset || !renderer.MaterialAsset) continue;

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

                if(count)
                    Renderer::SubmitRange(mesh.MeshAsset->GetVertexArray(),shader,
                        count,offset,scene.GetWorldTransform(entity),viewProjection,
                        material->GetColor(),material->IsUsingTexture()?1:0);
                else
                    Renderer::Submit(mesh.MeshAsset->GetVertexArray(),shader,
                        scene.GetWorldTransform(entity),viewProjection,
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
    }
}
