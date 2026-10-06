#include "Engine/Core/Window.h"
#include "Engine/Asset/AssetManager.h"
#include "Engine/Assets/Project.h"
#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scene/ScriptRegistry.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace
{
    std::filesystem::path FindProjectRoot()
    {
        auto current = std::filesystem::current_path();
        for (int i = 0; i < 8; ++i)
        {
            for (const auto& entry : std::filesystem::directory_iterator(current))
                if (entry.path().extension() == ".nojobproject") return current;
            if (!current.has_parent_path() || current.parent_path() == current) break;
            current = current.parent_path();
        }
        throw std::runtime_error("NoJobRuntime: no .nojobproject found.");
    }

    std::filesystem::path FindProjectFile(const std::filesystem::path& root)
    {
        for (const auto& entry : std::filesystem::directory_iterator(root))
            if (entry.path().extension() == ".nojobproject") return entry.path();
        return {};
    }

#ifdef _WIN32
    HMODULE g_ProjectScriptsModule = nullptr;
    std::vector<std::string> g_ProjectScriptNames;
    using RegisterProjectScriptFn = void(*)(void(*)(NoJob::ScriptDefinition));

    void RuntimeRegisterProjectScript(NoJob::ScriptDefinition definition)
    {
        g_ProjectScriptNames.push_back(definition.Name);
        NoJob::ScriptRegistry::Register(std::move(definition));
    }

    std::filesystem::path FindNewestProjectScriptsDLL(
        const std::filesystem::path& projectRoot)
    {
        const auto packagedDirectory = projectRoot / "RuntimeData" / "ProjectScripts";
        const auto developmentDirectory = projectRoot / "out" / "ProjectScripts";
        const auto directory = std::filesystem::exists(packagedDirectory)
            ? packagedDirectory : developmentDirectory;
        std::error_code ec;
        std::filesystem::path newest;
        std::filesystem::file_time_type newestTime{};

        if (!std::filesystem::exists(directory, ec))
            return {};

        for (auto it = std::filesystem::recursive_directory_iterator(
                 directory,
                 std::filesystem::directory_options::skip_permission_denied,
                 ec);
             !ec && it != std::filesystem::recursive_directory_iterator();
             ++it)
        {
            if (!it->is_regular_file(ec) || it->path().extension() != ".dll")
                continue;

            const auto stem = it->path().stem().string();
            if (stem.rfind("NoJobProjectScripts_", 0) != 0)
                continue;

            const auto time = std::filesystem::last_write_time(it->path(), ec);
            if (ec) { ec.clear(); continue; }
            if (newest.empty() || time > newestTime)
            {
                newest = it->path();
                newestTime = time;
            }
        }
        return newest;
    }

    bool LoadRuntimeProjectScripts(const std::filesystem::path& projectRoot)
    {
        const auto dll = FindNewestProjectScriptsDLL(projectRoot);
        if (dll.empty())
        {
            std::cout << "NoJobRuntime: no ProjectScripts DLL found; "
                         "continuing without project scripts.\n";
            return true;
        }

        g_ProjectScriptsModule = LoadLibraryW(dll.wstring().c_str());
        if (!g_ProjectScriptsModule)
        {
            std::cerr << "NoJobRuntime: failed to load ProjectScripts DLL: "
                      << dll << " (Win32 " << GetLastError() << ")\n";
            return false;
        }

        // A packaged build intentionally does not contain Assets/Scripts/*.cpp.
        // Discover script names from the serialized Start Scene instead, then
        // resolve their exported NoJobRegister_<ScriptName> symbols from the DLL.
        std::vector<std::string> serializedScripts;
        std::error_code ec;
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(
                 NoJob::AssetManager::GetAssetsDirectory(),
                 std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (ec) break;
            if (!entry.is_regular_file(ec) ||
                entry.path().extension() != ".nojobscene")
                continue;

            std::ifstream sceneFile(entry.path());
            std::string line;
            while (std::getline(sceneFile, line))
            {
                std::istringstream stream(line);
                std::string key;
                stream >> key;

                std::string scriptName;
                if (key == "SCRIPT_V2")
                {
                    bool enabled = true;
                    std::size_t fieldCount = 0;
                    stream >> enabled >> std::quoted(scriptName) >> fieldCount;
                }
                else if (key == "SCRIPT")
                {
                    // Legacy SCRIPT records always represent the built-in Rotator.
                    bool enabled = true;
                    float legacySpeed = 1.0f;
                    stream >> enabled >> legacySpeed;
                    scriptName = "Rotator";
                }
                else
                    continue;

                if (!scriptName.empty() &&
                    std::find(serializedScripts.begin(), serializedScripts.end(),
                              scriptName) == serializedScripts.end())
                    serializedScripts.push_back(scriptName);
            }
        }

        for (const auto& name : serializedScripts)
        {
            const auto symbol = "NoJobRegister_" + name;
            auto fn = reinterpret_cast<RegisterProjectScriptFn>(
                GetProcAddress(g_ProjectScriptsModule, symbol.c_str()));
            if (fn)
                fn(&RuntimeRegisterProjectScript);
            else
                std::cerr << "NoJobRuntime: script export not found: "
                          << symbol << "\n";
        }

        std::cout << "NoJobRuntime: loaded "
                  << g_ProjectScriptNames.size()
                  << " project script(s) from " << dll.filename().string()
                  << "\n";
        return true;
    }

    void UnloadRuntimeProjectScripts()
    {
        for (const auto& name : g_ProjectScriptNames)
            NoJob::ScriptRegistry::Unregister(name);
        g_ProjectScriptNames.clear();
        if (g_ProjectScriptsModule)
        {
            FreeLibrary(g_ProjectScriptsModule);
            g_ProjectScriptsModule = nullptr;
        }
    }
#else
    bool LoadRuntimeProjectScripts(const std::filesystem::path&) { return true; }
    void UnloadRuntimeProjectScripts() {}
#endif
}

int main()
{
    try
    {
        const auto projectRoot = FindProjectRoot();
        NoJob::ProjectConfig project;
        if (!NoJob::Project::Load(project, FindProjectFile(projectRoot)))
            throw std::runtime_error("NoJobRuntime: failed to load project configuration.");
        NoJob::AssetManager::Init(projectRoot);

        NoJob::Window window({ project.Name.empty() ? "NoJob Runtime" : project.Name, 1280, 720 });
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
            uniform int u_UseIBL;
            uniform float u_EnvironmentIntensity;
            uniform float u_DiffuseIBLStrength;
            uniform float u_SpecularIBLStrength;
            uniform float u_EnvironmentRotation;
            uniform int u_HasHDRI;
            uniform samplerCube u_IrradianceMap;
            uniform samplerCube u_PrefilterMap;
            uniform sampler2D u_BRDFLUT;
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
            vec3 RotateEnvironment(vec3 d){
                float c=cos(u_EnvironmentRotation),s=sin(u_EnvironmentRotation);
                return vec3(c*d.x+s*d.z,d.y,-s*d.x+c*d.z);
            }
            vec3 ProceduralEnvironment(vec3 d){
                d=RotateEnvironment(normalize(d));
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
                vec3 R=RotateEnvironment(reflect(-V,N));
                vec3 diffuseIBL;
                vec3 specIBL;
                if(u_HasHDRI==1){
                    vec3 irradiance=texture(u_IrradianceMap,RotateEnvironment(N)).rgb;
                    diffuseIBL=irradiance*albedo;
                    const float MAX_REFLECTION_LOD=4.0;
                    vec3 prefiltered=textureLod(
                        u_PrefilterMap,R,roughness*MAX_REFLECTION_LOD).rgb;
                    vec2 brdf=texture(u_BRDFLUT,vec2(NV,roughness)).rg;
                    specIBL=prefiltered*(F*brdf.x+brdf.y);
                }else{
                    diffuseIBL=ProceduralEnvironment(N)*albedo;
                    vec3 specEnv=ProceduralEnvironment(
                        normalize(mix(reflect(-V,N),N,roughness*roughness)));
                    specIBL=specEnv*F*(1.0-roughness*0.45);
                }
                vec3 environment =
                    (kD*diffuseIBL*u_DiffuseIBLStrength+
                     specIBL*u_SpecularIBLStrength)*
                    u_EnvironmentIntensity;
                if(u_UseIBL==0) environment=vec3(0.0);
                vec3 ambient=(environment+u_AmbientColor*albedo*0.25)*ao;

                vec3 emissive=u_EmissiveColor*u_EmissiveStrength;
                if(u_UseEmissiveMap==1) emissive*=texture(u_EmissiveMap,v_TexCoord).rgb;

                // Keep scene output linear/HDR. Tone mapping, bloom, AO and
                // anti-aliasing are handled by the framebuffer post-process chain.
                vec3 color=ambient+Lo+emissive;
                o_Color=vec4(color,base.a);
            }
        )";


        auto shader = NoJob::Shader::Create(vertexShaderSource, fragmentShaderSource);
        auto defaultMesh = NoJob::Mesh::CreateCube();
        auto defaultMaterial = std::make_shared<NoJob::Material>(
            shader, glm::vec4(0.95f, 0.35f, 0.15f, 1.0f));
        defaultMaterial->SetTexture(NoJob::Texture2D::CreateCheckerboard());

        NoJob::Scene scene;
        const auto startScene = projectRoot / project.StartScene;
        if (!NoJob::SceneSerializer::Load(scene, startScene, defaultMesh, defaultMaterial))
            throw std::runtime_error("NoJobRuntime: failed to load Start Scene: " + startScene.string());

        if (!LoadRuntimeProjectScripts(projectRoot))
            throw std::runtime_error("NoJobRuntime: ProjectScripts could not be loaded.");

        NoJob::PhysicsSystem physics;
        scene.OnRuntimeStart();
        physics.Start(scene);

        NoJob::FramebufferSpecification graphics;
        graphics.Width = 1280;
        graphics.Height = 720;
        graphics.HDR = true;
        graphics.Exposure = 0.72f;
        graphics.Bloom = true;
        graphics.BloomThreshold = 1.35f;
        graphics.BloomStrength = 0.08f;
        graphics.ScreenSpaceAO = true;
        graphics.AOIntensity = 0.18f;
        graphics.FXAA = true;
        auto framebuffer = NoJob::Framebuffer::Create(graphics);
        NoJob::SceneRenderer::SetGraphicsSettings(graphics);

        double lastTime = glfwGetTime();
        while (!window.ShouldClose())
        {
            window.PollEvents();
            const double now = glfwGetTime();
            const float dt = static_cast<float>(now - lastTime);
            lastTime = now;

            scene.OnUpdate(dt);
            physics.Update(dt);

            int width = 0, height = 0;
            glfwGetFramebufferSize(window.GetNativeWindow(), &width, &height);
            if (width <= 0 || height <= 0) { window.SwapBuffers(); continue; }
            framebuffer->Resize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));

            glm::mat4 view(1.0f), projection(1.0f);
            glm::vec3 cameraPosition(0.0f);
            bool foundCamera = false;
            for (NoJob::Entity entity : scene.GetEntities())
            {
                if (!entity.HasComponent<NoJob::CameraComponent>()) continue;
                const auto& camera = entity.GetComponent<NoJob::CameraComponent>();
                if (!camera.Primary) continue;
                const glm::mat4 world = scene.GetWorldTransform(entity);
                view = glm::inverse(world);
                projection = camera.GetProjection(static_cast<float>(width) / static_cast<float>(height));
                cameraPosition = glm::vec3(world[3]);
                foundCamera = true;
                break;
            }
            if (!foundCamera)
                throw std::runtime_error("NoJobRuntime: Start Scene has no Primary Camera.");

            framebuffer->Bind();
            NoJob::RenderCommand::SetClearColor(0.02f, 0.025f, 0.035f, 1.0f);
            NoJob::RenderCommand::Clear();
            NoJob::SceneRenderer::Render(scene, projection * view, cameraPosition);
            framebuffer->Unbind();
            framebuffer->PresentToDefault(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
            window.SwapBuffers();
        }

        physics.Stop();
        scene.OnRuntimeStop();
        UnloadRuntimeProjectScripts();
        framebuffer.reset();
        defaultMaterial.reset();
        defaultMesh.reset();
        shader.reset();
        NoJob::SceneRenderer::Shutdown();
        NoJob::Renderer::Shutdown();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
