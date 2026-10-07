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
#include "Engine/Renderer/DefaultSceneShaders.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scene/ScriptRegistry.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
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
        const auto buildConfig = projectRoot / "NoJobBuildConfig.nojobbuild";
        if (std::filesystem::exists(buildConfig)) { std::ifstream in(buildConfig); std::string h,n,s; int v=0; if((in>>h>>v)&&h=="NOJOB_BUILD_CONFIG"&&(in>>std::quoted(n))&&(in>>std::quoted(s))&&!s.empty()) project.StartScene=s; }
        NoJob::AssetManager::Init(projectRoot);

        NoJob::Window window({ project.Name.empty() ? "NoJob Runtime" : project.Name, 1280, 720 });
        NoJob::Renderer::Init();


        auto shader = NoJob::Shader::Create(NoJob::DefaultSceneShaders::Vertex, NoJob::DefaultSceneShaders::Fragment);
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
