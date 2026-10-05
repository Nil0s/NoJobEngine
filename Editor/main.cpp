#include "Engine/Core/Window.h"
#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Texture.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Scene/SceneSerializer.h"

#include "Editor/EditorCamera.h"
#include "Editor/EditorLayer.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

int main()
{
    try
    {
        NoJob::Window window({ "NoJobEngine", 1600, 900 });
        NoJob::Renderer::Init();

        const std::string vertexShaderSource = R"(
            #version 460 core

            layout(location = 0) in vec3 a_Position;
            layout(location = 1) in vec3 a_Normal;
            layout(location = 2) in vec2 a_TexCoord;
            layout(location = 3) in vec4 a_BoneIDs;
            layout(location = 4) in vec4 a_BoneWeights;

            uniform mat4 u_Transform;
            uniform int u_UseSkinning;
            uniform mat4 u_Bones[128];
            uniform mat4 u_ViewProjection;

            out vec3 v_WorldPosition;
            out vec3 v_Normal;
            out vec2 v_TexCoord;

            void main()
            {
                vec4 localPosition = vec4(a_Position, 1.0);
                vec3 localNormal = a_Normal;
                if (u_UseSkinning == 1 && dot(a_BoneWeights, vec4(1.0)) > 0.0)
                {
                    ivec4 ids = ivec4(a_BoneIDs);
                    mat4 skin =
                        u_Bones[ids.x] * a_BoneWeights.x +
                        u_Bones[ids.y] * a_BoneWeights.y +
                        u_Bones[ids.z] * a_BoneWeights.z +
                        u_Bones[ids.w] * a_BoneWeights.w;
                    localPosition = skin * localPosition;
                    localNormal = normalize(mat3(skin) * a_Normal);
                }

                vec4 worldPosition = u_Transform * localPosition;

                v_WorldPosition = worldPosition.xyz;
                v_Normal =
                    mat3(transpose(inverse(u_Transform))) * localNormal;
                v_TexCoord = a_TexCoord;

                gl_Position =
                    u_ViewProjection * worldPosition;
            }
        )";

        const std::string fragmentShaderSource = R"(
            #version 460 core
            layout(location=0) out vec4 o_Color;
            in vec3 v_WorldPosition; in vec3 v_Normal; in vec2 v_TexCoord;

            uniform vec4 u_Color;
            uniform sampler2D u_Texture;
            uniform int u_UseTexture;
            uniform int u_SurfaceMode;
            uniform float u_AlphaCutoff;
            uniform sampler2D u_NormalMap; uniform int u_UseNormalMap; uniform float u_NormalStrength;
            uniform sampler2D u_MetallicMap; uniform int u_UseMetallicMap;
            uniform sampler2D u_RoughnessMap; uniform int u_UseRoughnessMap;
            uniform sampler2D u_AOMap; uniform int u_UseAOMap;
            uniform sampler2D u_EmissiveMap; uniform int u_UseEmissiveMap;
            uniform vec3 u_EmissiveColor; uniform float u_EmissiveStrength;
            uniform vec3 u_ViewPosition; uniform vec3 u_AmbientColor;
            uniform float u_Metallic; uniform float u_Roughness; uniform float u_AO;

            struct DirectionalLight { vec3 direction; vec3 color; float intensity; };
            struct PointLight { vec3 position; vec3 color; float intensity; float range; };
            struct SpotLight { vec3 position; vec3 direction; vec3 color; float intensity; float range; float innerCos; float outerCos; };
            uniform int u_HasDirectionalLight; uniform DirectionalLight u_DirectionalLight;
            uniform int u_PointLightCount; uniform PointLight u_PointLights[4];
            uniform int u_SpotLightCount; uniform SpotLight u_SpotLights[4];

            uniform sampler2D u_DirectionalShadowMap;
            uniform samplerCube u_PointShadowMap;
            uniform sampler2D u_SpotShadowMap;
            uniform int u_HasDirectionalShadow, u_HasPointShadow, u_HasSpotShadow;
            uniform mat4 u_DirectionalLightSpace, u_SpotLightSpace;
            uniform vec3 u_PointShadowPosition;
            uniform float u_PointShadowFar, u_DirectionalShadowBias, u_PointShadowBias, u_SpotShadowBias;

            const float PI=3.14159265359;
            float DistributionGGX(vec3 N,vec3 H,float r){float a=r*r,a2=a*a,NH=max(dot(N,H),0.0),NH2=NH*NH;float d=(NH2*(a2-1.0)+1.0);return a2/max(PI*d*d,0.000001);}
            float GeometrySchlickGGX(float NV,float r){float k=((r+1.0)*(r+1.0))/8.0;return NV/(NV*(1.0-k)+k);}
            float GeometrySmith(vec3 N,vec3 V,vec3 L,float r){return GeometrySchlickGGX(max(dot(N,V),0.0),r)*GeometrySchlickGGX(max(dot(N,L),0.0),r);}
            vec3 FresnelSchlick(float c,vec3 F0){return F0+(1.0-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);}
            vec3 FresnelSchlickRoughness(float c,vec3 F0,float r){return F0+(max(vec3(1.0-r),F0)-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);}

            vec3 SurfaceNormal(){
                vec3 N=normalize(v_Normal);
                if(u_UseNormalMap==0) return N;
                vec3 mapN=texture(u_NormalMap,v_TexCoord).xyz*2.0-1.0;
                mapN.xy*=u_NormalStrength;
                vec3 Q1=dFdx(v_WorldPosition), Q2=dFdy(v_WorldPosition);
                vec2 st1=dFdx(v_TexCoord), st2=dFdy(v_TexCoord);

                // Reconstruct the full cotangent frame from position/UV
                // derivatives. The previous B=-cross(N,T) assumed one fixed
                // UV handedness, which breaks on mirrored FBX/Mixamo UV
                // islands and can turn correctly textured areas almost black.
                float det=st1.x*st2.y-st1.y*st2.x;
                if(abs(det)<0.000001) return N;
                vec3 T=(Q1*st2.y-Q2*st1.y)/det;
                vec3 B=(-Q1*st2.x+Q2*st1.x)/det;
                T=normalize(T-N*dot(N,T));
                B=normalize(B-N*dot(N,B));
                return normalize(mat3(T,B,N)*mapN);
            }
            float Shadow2D(sampler2D map,mat4 lightSpace,float bias){
                vec4 lp=lightSpace*vec4(v_WorldPosition,1.0); vec3 p=lp.xyz/lp.w; p=p*0.5+0.5;
                if(p.z>1.0||p.x<0.0||p.x>1.0||p.y<0.0||p.y>1.0) return 0.0;
                float shadow=0.0; vec2 texel=1.0/vec2(textureSize(map,0));
                for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++) shadow += p.z-bias>texture(map,p.xy+vec2(x,y)*texel).r?1.0:0.0;
                return shadow/9.0;
            }
            float PointShadow(float bias){
                vec3 d=v_WorldPosition-u_PointShadowPosition; float current=length(d);
                float closest=texture(u_PointShadowMap,d).r*u_PointShadowFar;
                return current-bias>closest?1.0:0.0;
            }
            vec3 BRDF(vec3 albedo,vec3 N,vec3 V,vec3 L,vec3 radiance,float metallic,float roughness){
                vec3 H=normalize(V+L); vec3 F0=mix(vec3(0.04),albedo,metallic);
                vec3 F=FresnelSchlick(max(dot(H,V),0.0),F0);
                float NDF=DistributionGGX(N,H,roughness),G=GeometrySmith(N,V,L,roughness);
                vec3 spec=(NDF*G*F)/max(4.0*max(dot(N,V),0.0)*max(dot(N,L),0.0),0.001);
                vec3 kD=(vec3(1.0)-F)*(1.0-metallic); float NL=max(dot(N,L),0.0);
                return (kD*albedo/PI+spec)*radiance*NL;
            }
            vec3 ProceduralEnvironment(vec3 d){
                float h=clamp(d.y*0.5+0.5,0.0,1.0);
                vec3 horizon=vec3(0.62,0.72,0.86), zenith=vec3(0.08,0.22,0.48);
                vec3 sky=mix(horizon,zenith,pow(h,0.7));
                if(d.y<0.0) sky=mix(vec3(0.025,0.03,0.035),horizon,clamp(d.y+1.0,0.0,1.0)*0.18);
                return sky;
            }
            vec3 ACES(vec3 x){
                const float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
                return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);
            }
            void main(){
                vec4 base=u_Color; if(u_UseTexture==1) base*=texture(u_Texture,v_TexCoord);
                if(u_SurfaceMode==1 && base.a<u_AlphaCutoff) discard;
                vec3 albedo=max(base.rgb,vec3(0.0));
                float metallic=clamp(u_Metallic*(u_UseMetallicMap==1?texture(u_MetallicMap,v_TexCoord).r:1.0),0.0,1.0);
                float roughness=clamp(u_Roughness*(u_UseRoughnessMap==1?texture(u_RoughnessMap,v_TexCoord).r:1.0),0.04,1.0);
                float ao=clamp(u_AO*(u_UseAOMap==1?texture(u_AOMap,v_TexCoord).r:1.0),0.0,1.0);
                vec3 N=SurfaceNormal(), V=normalize(u_ViewPosition-v_WorldPosition);
                vec3 Lo=vec3(0.0);

                if(u_HasDirectionalLight==1){vec3 L=normalize(-u_DirectionalLight.direction);float sh=u_HasDirectionalShadow==1?Shadow2D(u_DirectionalShadowMap,u_DirectionalLightSpace,u_DirectionalShadowBias):0.0;Lo+=BRDF(albedo,N,V,L,u_DirectionalLight.color*u_DirectionalLight.intensity*(1.0-sh),metallic,roughness);}
                for(int i=0;i<u_PointLightCount;i++){vec3 delta=u_PointLights[i].position-v_WorldPosition;float dist=length(delta);vec3 L=delta/max(dist,0.0001);float a=clamp(1.0-dist/max(u_PointLights[i].range,0.001),0.0,1.0);a*=a;float sh=(i==0&&u_HasPointShadow==1)?PointShadow(u_PointShadowBias):0.0;Lo+=BRDF(albedo,N,V,L,u_PointLights[i].color*u_PointLights[i].intensity*a*(1.0-sh),metallic,roughness);}
                for(int i=0;i<u_SpotLightCount;i++){vec3 delta=u_SpotLights[i].position-v_WorldPosition;float dist=length(delta);vec3 L=delta/max(dist,0.0001);float theta=dot(normalize(-L),normalize(u_SpotLights[i].direction));float cone=smoothstep(u_SpotLights[i].outerCos,u_SpotLights[i].innerCos,theta);float a=clamp(1.0-dist/max(u_SpotLights[i].range,0.001),0.0,1.0);a*=a;float sh=(i==0&&u_HasSpotShadow==1)?Shadow2D(u_SpotShadowMap,u_SpotLightSpace,u_SpotShadowBias):0.0;Lo+=BRDF(albedo,N,V,L,u_SpotLights[i].color*u_SpotLights[i].intensity*a*cone*(1.0-sh),metallic,roughness);}

                // Procedural image-based lighting: diffuse sky irradiance + roughness-aware specular environment.
                vec3 F0=mix(vec3(0.04),albedo,metallic);
                float NV=max(dot(N,V),0.0);
                vec3 F=FresnelSchlickRoughness(NV,F0,roughness);
                vec3 kD=(vec3(1.0)-F)*(1.0-metallic);
                vec3 diffuseIBL=ProceduralEnvironment(N)*albedo;
                vec3 R=reflect(-V,N);
                vec3 specEnv=ProceduralEnvironment(normalize(mix(R,N,roughness*roughness)));
                vec3 specIBL=specEnv*F*(1.0-roughness*0.45);
                vec3 ambient=(kD*diffuseIBL*0.18+specIBL*0.32+u_AmbientColor*albedo*0.25)*ao;

                vec3 emissive=u_EmissiveColor*u_EmissiveStrength;
                if(u_UseEmissiveMap==1) emissive*=texture(u_EmissiveMap,v_TexCoord).rgb;

                // Keep scene output linear/HDR. Tone mapping, bloom, AO and
                // anti-aliasing are handled by the framebuffer post-process chain.
                vec3 color=ambient+Lo+emissive;
                o_Color=vec4(color,base.a);
            }
        )";

        std::cerr << "[Startup] Creating PBR scene shader...\n";
        auto shader = NoJob::Shader::Create(
            vertexShaderSource,
            fragmentShaderSource);
        std::cerr << "[Startup] PBR scene shader OK.\n";

        auto cubeMesh = NoJob::Mesh::CreateCube();

        auto cubeMaterial = std::make_shared<NoJob::Material>(
            shader,
            glm::vec4(0.95f, 0.35f, 0.15f, 1.0f));
        auto checkerTexture =
            NoJob::Texture2D::CreateCheckerboard();
        cubeMaterial->SetTexture(checkerTexture);

        NoJob::Scene editorScene;
        std::unique_ptr<NoJob::Scene> runtimeScene;
        NoJob::Scene* activeScene = &editorScene;
        bool isPlaying = false;
        bool isPaused = false;

        NoJob::Entity cube = editorScene.CreateEntity("Cube");
        cube.AddComponent<NoJob::MeshComponent>(cubeMesh);
        cube.AddComponent<NoJob::MeshRendererComponent>(cubeMaterial);
        cube.GetComponent<NoJob::TransformComponent>().Position =
            { 0.0f, 2.0f, 0.0f };
        cube.AddComponent<NoJob::RigidbodyComponent>();
        cube.AddComponent<NoJob::BoxColliderComponent>();

        auto groundMaterial = std::make_shared<NoJob::Material>(
            shader,
            glm::vec4(0.28f, 0.32f, 0.38f, 1.0f));
        groundMaterial->SetTexture(checkerTexture);

        NoJob::Entity ground = editorScene.CreateEntity("Ground");
        ground.AddComponent<NoJob::MeshComponent>(cubeMesh);
        ground.AddComponent<NoJob::MeshRendererComponent>(groundMaterial);
        auto& groundTransform =
            ground.GetComponent<NoJob::TransformComponent>();
        groundTransform.Position = { 0.0f, -1.5f, 0.0f };
        groundTransform.Scale = { 6.0f, 0.5f, 6.0f };

        NoJob::RigidbodyComponent groundBody;
        groundBody.Type = NoJob::RigidbodyType::Static;
        groundBody.UseGravity = false;
        ground.AddComponent<NoJob::RigidbodyComponent>(groundBody);
        ground.AddComponent<NoJob::BoxColliderComponent>();

        NoJob::Entity mainCamera =
            editorScene.CreateEntity("Main Camera");
        auto& cameraTransform =
            mainCamera.GetComponent<NoJob::TransformComponent>();
        cameraTransform.Position = { 0.0f, 2.5f, 7.0f };
        cameraTransform.Rotation =
            { glm::radians(-10.0f), 0.0f, 0.0f };
        mainCamera.AddComponent<NoJob::CameraComponent>();

        NoJob::Entity sun = editorScene.CreateEntity("Sun");
        auto& sunTransform =
            sun.GetComponent<NoJob::TransformComponent>();
        sunTransform.Rotation =
            { glm::radians(-50.0f), glm::radians(-30.0f), 0.0f };
        NoJob::DirectionalLightComponent sunLight;
        sunLight.Intensity = 1.1f;
        sun.AddComponent<NoJob::DirectionalLightComponent>(sunLight);

        NoJob::Entity fillLight =
            editorScene.CreateEntity("Point Light");
        fillLight.GetComponent<NoJob::TransformComponent>().Position =
            { 2.5f, 2.5f, 2.0f };
        NoJob::PointLightComponent pointLight;
        pointLight.Intensity = 0.75f;
        pointLight.Range = 8.0f;
        fillLight.AddComponent<NoJob::PointLightComponent>(pointLight);

        NoJob::PhysicsSystem physics;

        NoJob::FramebufferSpecification framebufferSpecification;
        framebufferSpecification.Width = 1280;
        framebufferSpecification.Height = 720;
        framebufferSpecification.HDR = true;
        framebufferSpecification.Exposure = 0.72f;
        framebufferSpecification.Bloom = true;
        framebufferSpecification.BloomThreshold = 1.35f;
        framebufferSpecification.BloomStrength = 0.08f;
        framebufferSpecification.ScreenSpaceAO = true;
        framebufferSpecification.AOIntensity = 0.18f;
        framebufferSpecification.FXAA = true;

        auto framebuffer =
            NoJob::Framebuffer::Create(framebufferSpecification);

        NoJob::FramebufferSpecification cameraPreviewSpecification;
        cameraPreviewSpecification.Width = 320;
        cameraPreviewSpecification.Height = 180;
        cameraPreviewSpecification.HDR = true;
        cameraPreviewSpecification.Exposure = framebufferSpecification.Exposure;
        cameraPreviewSpecification.Bloom = framebufferSpecification.Bloom;
        cameraPreviewSpecification.BloomThreshold = framebufferSpecification.BloomThreshold;
        cameraPreviewSpecification.BloomStrength = framebufferSpecification.BloomStrength;
        cameraPreviewSpecification.ScreenSpaceAO = framebufferSpecification.ScreenSpaceAO;
        cameraPreviewSpecification.AOIntensity = framebufferSpecification.AOIntensity;
        cameraPreviewSpecification.FXAA = framebufferSpecification.FXAA;
        auto cameraPreviewFramebuffer =
            NoJob::Framebuffer::Create(cameraPreviewSpecification);

        NoJob::EditorLayer editor;
        editor.Init(window.GetNativeWindow(), &editorScene);
        editor.SetGraphicsSettings(framebufferSpecification);
        editor.SetSelectedEntity(cube);
        editor.SetDefaultCubeAssets(cubeMesh, cubeMaterial);
        editor.SetViewportTexture(
            framebuffer->GetColorAttachmentRendererID());
        editor.SetCameraPreviewTexture(
            cameraPreviewFramebuffer->GetColorAttachmentRendererID());

        NoJob::EditorCamera editorCamera;

        // Editor-only grid. It is not a Scene entity.
        std::vector<float> gridVertices;
        std::vector<std::uint32_t> gridIndices;

        constexpr int gridHalfSize = 10;
        constexpr float lineHalfWidth = 0.012f;
        std::uint32_t baseIndex = 0;

        auto addGridLine = [&](float x0, float z0, float x1, float z1)
        {
            const float dx = x1 - x0;
            const float dz = z1 - z0;
            const float length = std::sqrt(dx * dx + dz * dz);

            const float px = -dz / length * lineHalfWidth;
            const float pz = dx / length * lineHalfWidth;

            const float y = -1.0f;

            const float quad[] =
            {
                x0 + px, y, z0 + pz,
                x0 - px, y, z0 - pz,
                x1 - px, y, z1 - pz,
                x1 + px, y, z1 + pz
            };

            gridVertices.insert(
                gridVertices.end(),
                std::begin(quad),
                std::end(quad));

            const std::uint32_t local[] =
            {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
            };

            gridIndices.insert(
                gridIndices.end(),
                std::begin(local),
                std::end(local));

            baseIndex += 4;
        };

        for (int i = -gridHalfSize; i <= gridHalfSize; ++i)
        {
            addGridLine(
                static_cast<float>(i),
                -static_cast<float>(gridHalfSize),
                static_cast<float>(i),
                static_cast<float>(gridHalfSize));

            addGridLine(
                -static_cast<float>(gridHalfSize),
                static_cast<float>(i),
                static_cast<float>(gridHalfSize),
                static_cast<float>(i));
        }

        auto gridVB =
            NoJob::VertexBuffer::Create(
                gridVertices.data(),
                static_cast<std::uint32_t>(
                    gridVertices.size() * sizeof(float)));

        auto gridIB =
            NoJob::IndexBuffer::Create(
                gridIndices.data(),
                static_cast<std::uint32_t>(gridIndices.size()));

        auto gridVA = NoJob::VertexArray::Create();
        gridVA->SetVertexBuffer(gridVB);
        gridVA->SetIndexBuffer(gridIB);

        double lastTime = glfwGetTime();

        while (!window.ShouldClose())
        {
            const double currentTime = glfwGetTime();
            const float deltaTime =
                static_cast<float>(currentTime - lastTime);
            lastTime = currentTime;

            window.PollEvents();

            if (isPlaying && !isPaused && runtimeScene)
            {
                runtimeScene->OnUpdate(deltaTime);
                physics.Update(deltaTime);
            }

            const std::uint32_t viewportWidth =
                editor.GetViewportWidth();

            const std::uint32_t viewportHeight =
                editor.GetViewportHeight();

            const auto& currentSpec =
                framebuffer->GetSpecification();

            if (viewportWidth != currentSpec.Width
                || viewportHeight != currentSpec.Height)
            {
                framebuffer->Resize(
                    viewportWidth,
                    viewportHeight);

                editor.SetViewportTexture(
                    framebuffer->GetColorAttachmentRendererID());
            }

            editorCamera.SetViewportSize(
                static_cast<float>(viewportWidth),
                static_cast<float>(viewportHeight));

            if (!isPlaying)
            {
                editorCamera.OnUpdate(
                    window.GetNativeWindow(),
                    deltaTime,
                    editor.IsViewportHovered());
            }

            // Apply editor graphics settings live to both render targets.
            const auto& graphicsSettings = editor.GetGraphicsSettings();
            framebuffer->SetPostProcessSettings(graphicsSettings);
            cameraPreviewFramebuffer->SetPostProcessSettings(graphicsSettings);

            framebuffer->Bind();

            NoJob::RenderCommand::SetClearColor(
                0.055f, 0.065f, 0.085f, 1.0f);
            NoJob::RenderCommand::Clear();

            glm::mat4 renderView =
                editorCamera.GetViewMatrix();
            glm::mat4 renderProjection =
                editorCamera.GetProjectionMatrix();
            glm::mat4 viewProjection =
                renderProjection * renderView;
            glm::vec3 renderCameraPosition =
                editorCamera.GetPosition();

            // During Play the primary Scene camera owns the game view.
            if (isPlaying && activeScene)
            {
                for (NoJob::Entity entity : activeScene->GetEntities())
                {
                    if (!entity.HasComponent<NoJob::CameraComponent>())
                        continue;

                    const auto& camera =
                        entity.GetComponent<NoJob::CameraComponent>();
                    if (!camera.Primary)
                        continue;

                    const glm::mat4 cameraWorld =
                        activeScene->GetWorldTransform(entity);
                    renderView =
                        glm::inverse(cameraWorld);
                    const float aspect =
                        viewportHeight > 0
                            ? static_cast<float>(viewportWidth) /
                              static_cast<float>(viewportHeight)
                            : 1.0f;

                    renderProjection =
                        camera.GetProjection(aspect);
                    viewProjection =
                        renderProjection * renderView;
                    renderCameraPosition =
                        glm::vec3(cameraWorld[3]);
                    break;
                }
            }

            if (!isPlaying)
            {
                shader->Bind();
                shader->SetFloat3(
                    "u_AmbientColor", { 1.0f, 1.0f, 1.0f });
                shader->SetInt("u_HasDirectionalLight", 0);
                shader->SetInt("u_PointLightCount", 0);
                shader->SetInt("u_SpotLightCount", 0);
                shader->SetFloat3(
                    "u_ViewPosition", renderCameraPosition);

                NoJob::Renderer::Submit(
                    gridVA,
                    shader,
                    glm::mat4(1.0f),
                    viewProjection,
                    glm::vec4(0.28f, 0.30f, 0.34f, 1.0f));
            }

            NoJob::SceneRenderer::Render(
                *activeScene,
                viewProjection,
                renderCameraPosition);

            framebuffer->Unbind();

            // Unity-style preview for the currently selected Camera entity.
            if (!isPlaying)
            {
                const NoJob::Entity selected = editor.GetSelectedEntity();
                if (selected &&
                    selected.HasComponent<NoJob::CameraComponent>())
                {
                    const auto& previewCamera =
                        selected.GetComponent<NoJob::CameraComponent>();
                    const glm::mat4 cameraWorld =
                        editorScene.GetWorldTransform(selected);
                    const glm::mat4 previewView =
                        glm::inverse(cameraWorld);
                    const glm::mat4 previewProjection =
                        previewCamera.GetProjection(320.0f / 180.0f);

                    cameraPreviewFramebuffer->Bind();
                    NoJob::RenderCommand::SetClearColor(
                        0.055f, 0.065f, 0.085f, 1.0f);
                    NoJob::RenderCommand::Clear();

                    NoJob::SceneRenderer::Render(
                        editorScene,
                        previewProjection * previewView,
                        glm::vec3(cameraWorld[3]));

                    cameraPreviewFramebuffer->Unbind();
                }
            }

            NoJob::RenderCommand::SetViewport(0, 0, 1600, 900);
            NoJob::RenderCommand::SetClearColor(
                0.035f, 0.038f, 0.045f, 1.0f);
            NoJob::RenderCommand::Clear();

            editor.SetEditorCameraMatrices(
                renderView,
                renderProjection);

            editor.BeginFrame();
            editor.Draw();

            if (editor.ConsumePlayRequest() && !isPlaying)
            {
                runtimeScene = editorScene.Copy();
                runtimeScene->OnRuntimeStart();
                physics.Start(*runtimeScene);
                activeScene = runtimeScene.get();
                isPlaying = true;
                isPaused = false;

                editor.SetScene(activeScene);
                editor.SetRuntimeState(true, false);
            }

            if (editor.ConsumePauseRequest() && isPlaying)
            {
                isPaused = !isPaused;
                editor.SetRuntimeState(true, isPaused);
            }

            if (editor.ConsumeStopRequest() && isPlaying)
            {
                physics.Stop();
                runtimeScene->OnRuntimeStop();
                runtimeScene.reset();
                activeScene = &editorScene;
                isPlaying = false;
                isPaused = false;

                editor.SetScene(activeScene);
                editor.SetRuntimeState(false, false);
            }

            if (!isPlaying && editor.ConsumeSaveSceneRequest())
            {
                std::filesystem::create_directories("Assets/Scenes");
                NoJob::SceneSerializer::Save(editorScene, "Assets/Scenes/CurrentScene.nojobscene");
                std::cout << "[Scene] Saved Assets/Scenes/CurrentScene.nojobscene\n";
            }

            if (!isPlaying && editor.ConsumeLoadSceneRequest())
            {
                if (NoJob::SceneSerializer::Load(editorScene, "Assets/Scenes/CurrentScene.nojobscene", cubeMesh, cubeMaterial))
                {
                    activeScene = &editorScene;
                    editor.SetScene(activeScene);
                    std::cout << "[Scene] Loaded Assets/Scenes/CurrentScene.nojobscene\n";
                }
            }

            if (!isPlaying && editor.ConsumeGraphicsTestSceneRequest())
            {
                // Deterministic validation scene: each station isolates a major
                // graphics feature so regressions are visible in one viewport.
                for (auto e : editorScene.GetEntities())
                    editorScene.DestroyEntity(e);

                auto makeCube=[&](const char* name,glm::vec3 pos,glm::vec3 scale,glm::vec4 color,float metal,float rough,float emissive)
                {
                    auto mat=std::make_shared<NoJob::Material>(shader,color);
                    mat->Metallic()=metal; mat->Roughness()=rough; mat->AmbientOcclusion()=1.0f;
                    mat->EmissiveColor()=glm::vec3(color); mat->EmissiveStrength()=emissive;
                    auto e=editorScene.CreateEntity(name);
                    e.AddComponent<NoJob::MeshComponent>(cubeMesh);
                    e.AddComponent<NoJob::MeshRendererComponent>(mat);
                    auto& tr=e.GetComponent<NoJob::TransformComponent>(); tr.Position=pos; tr.Scale=scale;
                    return e;
                };

                makeCube("PBR Dielectric",{-4.5f,0.0f,0.0f},{1,1,1},{0.72f,0.18f,0.08f,1},0.0f,0.35f,0.0f);
                makeCube("PBR Metal",{-1.5f,0.0f,0.0f},{1,1,1},{0.75f,0.72f,0.62f,1},1.0f,0.16f,0.0f);
                makeCube("Rough Surface",{1.5f,0.0f,0.0f},{1,1,1},{0.12f,0.35f,0.75f,1},0.25f,0.92f,0.0f);
                makeCube("HDR Emissive",{4.5f,0.0f,0.0f},{1,1,1},{0.15f,0.8f,0.32f,1},0.0f,0.4f,8.0f);
                makeCube("Shadow Receiver",{0.0f,-1.6f,0.0f},{7.0f,0.25f,3.0f},{0.18f,0.20f,0.23f,1},0.0f,0.75f,0.0f);

                auto cam=editorScene.CreateEntity("Graphics Test Camera");
                auto& ct=cam.GetComponent<NoJob::TransformComponent>();ct.Position={0.0f,3.5f,11.5f};ct.Rotation={glm::radians(-14.0f),0.0f,0.0f};
                cam.AddComponent<NoJob::CameraComponent>();

                auto sunE=editorScene.CreateEntity("Directional Shadow Test");
                sunE.GetComponent<NoJob::TransformComponent>().Rotation={glm::radians(-52.0f),glm::radians(-28.0f),0.0f};
                NoJob::DirectionalLightComponent dl;dl.Intensity=1.15f;dl.CastShadows=true;sunE.AddComponent<NoJob::DirectionalLightComponent>(dl);

                auto pointE=editorScene.CreateEntity("Point Shadow Test");
                pointE.GetComponent<NoJob::TransformComponent>().Position={-3.0f,2.8f,2.0f};
                NoJob::PointLightComponent pl;pl.Intensity=1.4f;pl.Range=7.0f;pl.CastShadows=true;pointE.AddComponent<NoJob::PointLightComponent>(pl);

                auto spotE=editorScene.CreateEntity("Spot Shadow Test");
                auto& st=spotE.GetComponent<NoJob::TransformComponent>();st.Position={3.0f,4.0f,3.0f};st.Rotation={glm::radians(-55.0f),glm::radians(18.0f),0.0f};
                NoJob::SpotLightComponent sl;sl.Intensity=4.0f;sl.Range=12.0f;sl.CastShadows=true;spotE.AddComponent<NoJob::SpotLightComponent>(sl);

                activeScene=&editorScene; editor.SetScene(activeScene);
                NoJob::SceneSerializer::Save(editorScene,"Assets/Scenes/GraphicsValidation.nojobscene");
                std::cout<<"[Graphics] Validation scene generated and saved.\n";
            }

            editor.EndFrame();

            window.SwapBuffers();
        }

        editor.Shutdown();

        cameraPreviewFramebuffer.reset();
        framebuffer.reset();

        gridVA.reset();
        gridIB.reset();
        gridVB.reset();

        groundMaterial.reset();
        cubeMaterial.reset();
        cubeMesh.reset();
        shader.reset();

        NoJob::Renderer::Shutdown();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
