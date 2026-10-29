#include "Engine/Core/Window.h"
#include "Engine/Core/ProjectPaths.h"
#include "Editor/Distribution/PackageValidation.h"
#include "Editor/Distribution/UpdateService.h"
#include "Engine/Asset/AssetManager.h"
#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Physics/JoltSpatialQuery.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Texture.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/DefaultSceneShaders.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Scene/SceneSerializer.h"

#include "Engine/AI/Navigation/NavMeshDebugRenderer.h"
#include "Engine/AI/Navigation/NavigationSystem.h"



#include "Editor/EditorCamera.h"
#include "Editor/EditorLayer.h"
#include "Editor/Scripting/ProjectScriptBuildSystem.h"
#include "Editor/Scene/SceneFileDialog.h"
#include "Editor/Scene/DefaultScene.h"
#include "Resources/Version.h"
#include <sstream>

#include <GLFW/glfw3.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>


int main(int argc,char** argv)
{
    try
    {
        // Created first so it is destroyed last during exception unwinding.
        // The worker must be joined before its static std::thread is destroyed.
        struct ScriptBuildWorkerExitGuard final
        {
            ~ScriptBuildWorkerExitGuard() noexcept
            {
                NoJob::ProjectScriptBuildSystem::Shutdown();
            }
        } scriptBuildWorkerExitGuard;

        const auto discoveredRoot=NoJob::Paths::InitializeProjectWorkspace();
        if(discoveredRoot.empty())throw std::runtime_error("Project assets not found. Extract the complete Alpha ZIP to a writable folder.");
        std::filesystem::current_path(discoveredRoot);
        NoJob::AssetManager::Init(discoveredRoot);
        if(argc>1&&std::string(argv[1])=="--validate-package")return NoJob::ValidateAlphaPackage(discoveredRoot);
        const bool smoke=argc>1&&(std::string(argv[1])=="--smoke-test"||std::string(argv[1])=="--smoke-examples"||std::string(argv[1])=="--smoke-scene-workflow");
        unsigned smokeFrames=0;
        NoJob::Window window({NoJob::Paths::IsPortable(discoveredRoot)?"NJE " NJE_PUBLIC_VERSION:"NJE - NoJobEngine",1600,900});
        NoJob::Renderer::Init();

        std::cerr << "[Startup] Creating PBR scene shader...\n";

        auto shader = NoJob::Shader::Create(
            NoJob::DefaultSceneShaders::Vertex,
            NoJob::DefaultSceneShaders::Fragment);

        std::cerr << "[Startup] PBR scene shader OK.\n";

        auto cubeMesh = NoJob::Mesh::CreateCube();

        auto cubeMaterial =
            std::make_shared<NoJob::Material>(
                shader,
                glm::vec4(
                    0.95f,
                    0.35f,
                    0.15f,
                    1.0f));

        auto checkerTexture =
            NoJob::Texture2D::CreateCheckerboard();

        cubeMaterial->SetTexture(checkerTexture);

        NoJob::Scene editorScene;

        std::unique_ptr<NoJob::Scene>
            runtimeScene;

        NoJob::Scene* activeScene =
            &editorScene;

        bool isPlaying = false;
        bool isPaused = false;

        // A new document has no path until its first successful Save As.
        std::filesystem::path currentScenePath;
        NoJob::CreateDefaultEditorScene(editorScene);


        // ------------------------------------------------------------
        // Physics
        // ------------------------------------------------------------

        NoJob::PhysicsSystem physics;

        // Adapter between AI perception and Jolt Physics.
        // PhysicsSystem owns the physical world.
        NoJob::JoltSpatialQuery spatialQuery(physics);

        // ------------------------------------------------------------
        // Main framebuffer
        // ------------------------------------------------------------

        NoJob::FramebufferSpecification
            framebufferSpecification;

        framebufferSpecification.Width = 1280;
        framebufferSpecification.Height = 720;

        framebufferSpecification.HDR = true;
        framebufferSpecification.Exposure = 0.72f;

        framebufferSpecification.Bloom = true;
        framebufferSpecification.BloomThreshold =
            1.35f;

        framebufferSpecification.BloomStrength =
            0.08f;

        framebufferSpecification.ScreenSpaceAO =
            true;

        framebufferSpecification.AOIntensity =
            0.18f;

        framebufferSpecification.FXAA = true;

        auto framebuffer =
            NoJob::Framebuffer::Create(
                framebufferSpecification);


        // ------------------------------------------------------------
        // Camera preview framebuffer
        // ------------------------------------------------------------

        NoJob::FramebufferSpecification
            cameraPreviewSpecification;

        cameraPreviewSpecification.Width = 320;
        cameraPreviewSpecification.Height = 180;

        cameraPreviewSpecification.HDR = true;

        cameraPreviewSpecification.Exposure =
            framebufferSpecification.Exposure;

        cameraPreviewSpecification.Bloom =
            framebufferSpecification.Bloom;

        cameraPreviewSpecification.BloomThreshold =
            framebufferSpecification.BloomThreshold;

        cameraPreviewSpecification.BloomStrength =
            framebufferSpecification.BloomStrength;

        cameraPreviewSpecification.ScreenSpaceAO =
            framebufferSpecification.ScreenSpaceAO;

        cameraPreviewSpecification.AOIntensity =
            framebufferSpecification.AOIntensity;

        cameraPreviewSpecification.FXAA =
            framebufferSpecification.FXAA;

        auto cameraPreviewFramebuffer =
            NoJob::Framebuffer::Create(
                cameraPreviewSpecification);


        // ------------------------------------------------------------
        // Editor
        // ------------------------------------------------------------

        NoJob::EditorLayer editor;

        editor.Init(
            window.GetNativeWindow(),
            &editorScene);

        editor.SetGraphicsSettings(
            framebufferSpecification);



        editor.SetDefaultCubeAssets(
            cubeMesh,
            cubeMaterial);

        editor.SetViewportTexture(
            framebuffer->
            GetColorAttachmentRendererID());

        editor.SetCameraPreviewTexture(
            cameraPreviewFramebuffer->
            GetColorAttachmentRendererID());


        NoJob::EditorCamera editorCamera;
        if(argc>1&&std::string(argv[1])=="--smoke-examples") {
            std::ofstream report(discoveredRoot/"ExamplesValidation.txt");
            for(const auto* name:{"RLObstacleLearningDemo","RLTrackingDemo","RLNavigationDemo","NeuralTrainingDemo","NeuralPongTrackingDemo"}) {
                NoJob::Scene example;
                const auto path=discoveredRoot/"Assets/Scenes"/(std::string(name)+".nojobscene");
                if(!NoJob::SceneSerializer::Load(example,path,cubeMesh,cubeMaterial))throw std::runtime_error("Cannot load example: "+std::string(name));
                for(auto entity:example.GetEntities())if(entity.HasComponent<NoJob::NativeScriptComponent>()) {
                    const auto& script=entity.GetComponent<NoJob::NativeScriptComponent>();
                    if(!NoJob::ScriptRegistry::Find(script.ScriptName))throw std::runtime_error("Missing example script: "+script.ScriptName);
                }
                example.OnRuntimeStart();for(int frame=0;frame<120;++frame)example.OnUpdate(1.f/60.f);example.OnRuntimeStop();
                report<<"PASS: "<<name<<" scene load, script factories and Play/Stop\n";
            }
        }
        auto frameExample=[&] {
            for(auto entity:editorScene.GetEntities())if(entity.HasComponent<NoJob::CameraComponent>()&&entity.GetComponent<NoJob::CameraComponent>().Primary) {
                const auto world=editorScene.GetWorldTransform(entity);
                editorCamera.SetPose(glm::vec3(world[3]),-glm::vec3(world[2]));break;
            }
        };
        auto frameNewScene = [&] {
            editorCamera.SetPose({8.0f, 6.0f, 10.0f}, {-8.0f, -6.0f, -10.0f});
        };
        frameNewScene();
        auto updateSceneTitle = [&] {
            const auto title = std::string("NJE " NJE_PUBLIC_VERSION " - ") + (currentScenePath.empty() ? "Untitled" : currentScenePath.filename().string());
            glfwSetWindowTitle(window.GetNativeWindow(), title.c_str());
        };
        updateSceneTitle();
        auto sceneContent = [&] {
            std::ostringstream out;
            if (!NoJob::SceneSerializer::Serialize(editorScene, out))
                throw std::runtime_error("Cannot inspect scene changes.");
            return out.str();
        };
        std::string savedSceneContent = sceneContent();
        auto saveScene = [&](bool saveAs) {
            auto selected = currentScenePath;
            if (saveAs || selected.empty())
                selected = NoJob::SceneFileDialog::SaveAs(discoveredRoot / "Assets/Scenes");
            if (selected.empty()) return false;
            std::error_code error;
            std::filesystem::create_directories(selected.parent_path(), error);
            if (error || !NoJob::SceneSerializer::Save(editorScene, selected)) {
                NoJob::SceneFileDialog::ShowSaveError();
                return false;
            }
            currentScenePath = selected;
            updateSceneTitle();
            savedSceneContent = sceneContent();
            std::cout << "[Scene] Saved " << selected.string() << '\n';
            return true;
        };
        auto canReplaceScene = [&] {
            if (sceneContent() == savedSceneContent) return true;
            const auto answer = NoJob::SceneFileDialog::ConfirmSceneReplacement();
            if (answer == NoJob::SceneSaveChoice::Cancel) return false;
            return answer == NoJob::SceneSaveChoice::Discard || saveScene(false);
        };



        // ------------------------------------------------------------
        // Navigation
        // ------------------------------------------------------------

        NoJob::NavigationSystem
            navigationSystem;

        NoJob::NavMeshDebugRenderer
            navMeshDebugRenderer;
        // ------------------------------------------------------------


        if (argc > 1 && std::string(argv[1]) == "--smoke-scene-workflow") {
            const auto reportDirectory = discoveredRoot / "Reports/Editor";
            std::filesystem::create_directories(reportDirectory);
            std::ofstream report(reportDirectory / "SceneWorkflowValidation.txt");
            auto require = [&](bool value, const char* text) {
                if (!value) throw std::runtime_error(text);
                report << "PASS: " << text << '\n';
            };
            auto checkTemplate = [&] {
                unsigned cameras = 0, lights = 0;
                require(editorScene.GetEntities().size() == 3, "New scene contains exactly three entities");
                for (auto entity : editorScene.GetEntities()) {
                    if (entity.HasComponent<NoJob::CameraComponent>())
                        cameras += entity.GetComponent<NoJob::CameraComponent>().Primary ? 1 : 0;
                    lights += entity.HasComponent<NoJob::DirectionalLightComponent>() ? 1 : 0;
                    lights += entity.HasComponent<NoJob::PointLightComponent>() ? 1 : 0;
                    require(!entity.HasComponent<NoJob::MeshComponent>() && !entity.HasComponent<NoJob::NativeScriptComponent>(),
                        "Template contains no meshes or gameplay scripts");
                }
                require(cameras == 1 && lights == 2, "One primary camera and two lights");
            };
            checkTemplate();
            require(currentScenePath.empty() && editor.IsGridVisible(), "Startup is untitled with grid enabled");
            const auto emptyContent = sceneContent();
            auto userCube = editorScene.CreateEntity("User cube");
            userCube.AddComponent<NoJob::MeshComponent>(cubeMesh);
            userCube.AddComponent<NoJob::MeshRendererComponent>(cubeMaterial);
            require(sceneContent() != savedSceneContent, "Scene edits differ from the clean baseline");
            const auto saved = reportDirectory / "SceneWorkflowSaved.nojobscene";
            require(NoJob::SceneSerializer::Save(editorScene, saved), "Edited scene saves successfully");
            NoJob::CreateDefaultEditorScene(editorScene);
            editor.ResetSceneDocument(&editorScene);
            editor.ResetSceneOverlays();
            checkTemplate();
            require(sceneContent() == emptyContent && !editor.GetSelectedEntity(), "New restores the same template and clears selection");
            require(NoJob::SceneSerializer::Load(editorScene, saved, cubeMesh, cubeMaterial), "Saved scene reopens after New");
            require(editorScene.GetEntities().size() == 4, "Saved user geometry is preserved");
            NoJob::Scene example;
            require(NoJob::SceneSerializer::Load(example, discoveredRoot / "Assets/Scenes/RLObstacleLearningDemo.nojobscene", cubeMesh, cubeMaterial),
                "Bundled obstacle example remains available");
            NoJob::CreateDefaultEditorScene(editorScene);
            editor.ResetSceneDocument(&editorScene);
            savedSceneContent = sceneContent();
            require(savedSceneContent == emptyContent, "Repeated New produces a clean document");
            report << "PASS: scene workflow diagnostic completed; editor-only grid is not serialized.\n";
        }

        // ------------------------------------------------------------
        // Editor-only grid
        // ------------------------------------------------------------

        std::vector<float>
            gridVertices;

        std::vector<std::uint32_t>
            gridIndices;

        constexpr int gridHalfSize = 10;
        constexpr float lineHalfWidth = 0.012f;

        std::uint32_t baseIndex = 0;

        auto addGridLine =
            [&](float x0,
                float z0,
                float x1,
                float z1)
            {
                const float dx = x1 - x0;
                const float dz = z1 - z0;

                const float length =
                    std::sqrt(
                        dx * dx +
                        dz * dz);

                const float px =
                    -dz / length *
                    lineHalfWidth;

                const float pz =
                    dx / length *
                    lineHalfWidth;

                const float y = 0.0f;

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
                    baseIndex + 0,
                    baseIndex + 1,
                    baseIndex + 2,

                    baseIndex + 2,
                    baseIndex + 3,
                    baseIndex + 0
                };

                gridIndices.insert(
                    gridIndices.end(),
                    std::begin(local),
                    std::end(local));

                baseIndex += 4;
            };


        for (int i = -gridHalfSize;
            i <= gridHalfSize;
            ++i)
        {
            addGridLine(
                static_cast<float>(i),
                -static_cast<float>(
                    gridHalfSize),
                static_cast<float>(i),
                static_cast<float>(
                    gridHalfSize));

            addGridLine(
                -static_cast<float>(
                    gridHalfSize),
                static_cast<float>(i),
                static_cast<float>(
                    gridHalfSize),
                static_cast<float>(i));
        }


        auto gridVB =
            NoJob::VertexBuffer::Create(
                gridVertices.data(),
                static_cast<std::uint32_t>(
                    gridVertices.size()
                    * sizeof(float)));

        auto gridIB =
            NoJob::IndexBuffer::Create(
                gridIndices.data(),
                static_cast<std::uint32_t>(
                    gridIndices.size()));

        auto gridVA =
            NoJob::VertexArray::Create();

        gridVA->SetVertexBuffer(gridVB);
        gridVA->SetIndexBuffer(gridIB);


        // ------------------------------------------------------------
        // Main loop
        // ------------------------------------------------------------

        double lastTime = glfwGetTime();

        NoJob::Updates::Initialize([&] {
            return !isPlaying && saveScene(false);
        });
        struct UpdateExitGuard { ~UpdateExitGuard(){NoJob::Updates::Shutdown();} } updateExitGuard;
        while (!window.ShouldClose())
        {
            const double currentTime =
                glfwGetTime();

            const float deltaTime =
                static_cast<float>(
                    currentTime -
                    lastTime);

            lastTime = currentTime;

            window.PollEvents();
            if(NoJob::Updates::QuitRequested())glfwSetWindowShouldClose(window.GetNativeWindow(),GLFW_TRUE);


            // --------------------------------------------------------
            // Runtime update
            // --------------------------------------------------------

            if (isPlaying &&
                !isPaused &&
                runtimeScene)
            {
                runtimeScene->OnUpdate(
                    deltaTime);

                physics.Update(
                    deltaTime);
            }


            // --------------------------------------------------------
            // Viewport
            // --------------------------------------------------------

            const std::uint32_t viewportWidth =
                editor.GetViewportWidth();

            const std::uint32_t viewportHeight =
                editor.GetViewportHeight();

            const auto& currentSpec =
                framebuffer->GetSpecification();

            if (viewportWidth !=
                currentSpec.Width ||
                viewportHeight !=
                currentSpec.Height)
            {
                framebuffer->Resize(
                    viewportWidth,
                    viewportHeight);

                editor.SetViewportTexture(
                    framebuffer->
                    GetColorAttachmentRendererID());
            }


            editorCamera.SetViewportSize(
                static_cast<float>(
                    viewportWidth),
                static_cast<float>(
                    viewportHeight));

            if (!isPlaying)
            {
                editorCamera.OnUpdate(
                    window.GetNativeWindow(),
                    deltaTime,
                    editor.IsViewportHovered());
            }


            // --------------------------------------------------------
            // Graphics settings
            // --------------------------------------------------------

            const auto& graphicsSettings =
                editor.GetGraphicsSettings();

            framebuffer->
                SetPostProcessSettings(
                    graphicsSettings);

            cameraPreviewFramebuffer->
                SetPostProcessSettings(
                    graphicsSettings);

            NoJob::SceneRenderer::
                SetGraphicsSettings(
                    graphicsSettings);


            // --------------------------------------------------------
            // Main scene render
            // --------------------------------------------------------

            framebuffer->Bind();

            NoJob::RenderCommand::SetClearColor(
                0.055f,
                0.065f,
                0.085f,
                1.0f);

            NoJob::RenderCommand::Clear();


            glm::mat4 renderView =
                editorCamera.GetViewMatrix();

            glm::mat4 renderProjection =
                editorCamera.GetProjectionMatrix();

            glm::mat4 viewProjection =
                renderProjection *
                renderView;

            glm::vec3 renderCameraPosition =
                editorCamera.GetPosition();


            // During Play the primary Scene camera owns the game view.
            if (isPlaying && activeScene)
            {
                for (NoJob::Entity entity :
                activeScene->GetEntities())
                {
                    if (!entity.HasComponent<
                        NoJob::CameraComponent>())
                    {
                        continue;
                    }

                    const auto& camera =
                        entity.GetComponent<
                        NoJob::CameraComponent>();

                    if (!camera.Primary)
                        continue;

                    const glm::mat4 cameraWorld =
                        activeScene->
                        GetWorldTransform(
                            entity);

                    renderView =
                        glm::inverse(
                            cameraWorld);

                    const float aspect =
                        viewportHeight > 0
                        ? static_cast<float>(
                            viewportWidth)
                        /
                        static_cast<float>(
                            viewportHeight)
                        : 1.0f;

                    renderProjection =
                        camera.GetProjection(
                            aspect);

                    viewProjection =
                        renderProjection *
                        renderView;

                    renderCameraPosition =
                        glm::vec3(
                            cameraWorld[3]);

                    break;
                }
            }


            // --------------------------------------------------------
            // Editor grid
            // --------------------------------------------------------

            if (!isPlaying && editor.IsGridVisible())
            {
                shader->Bind();

                shader->SetFloat3(
                    "u_AmbientColor",
                    {
                        1.0f,
                        1.0f,
                        1.0f
                    });

                shader->SetInt(
                    "u_HasDirectionalLight",
                    0);

                shader->SetInt(
                    "u_PointLightCount",
                    0);

                shader->SetInt(
                    "u_SpotLightCount",
                    0);

                shader->SetFloat3(
                    "u_ViewPosition",
                    renderCameraPosition);

                NoJob::Renderer::Submit(
                    gridVA,
                    shader,
                    glm::mat4(1.0f),
                    viewProjection,
                    glm::vec4(
                        0.28f,
                        0.30f,
                        0.34f,
                        1.0f));
            }


            NoJob::SceneRenderer::Render(
                *activeScene,
                viewProjection,
                renderCameraPosition);


            // --------------------------------------------------------
            // Navigation debug rendering
            // --------------------------------------------------------

            if (!isPlaying &&
                editor.IsNavMeshVisible() &&
                navigationSystem.HasNavMesh())
            {
                navMeshDebugRenderer.Draw(
                    viewProjection);
            }


            framebuffer->Unbind();


            // --------------------------------------------------------
            // Selected camera preview
            // --------------------------------------------------------

            if (!isPlaying)
            {
                const NoJob::Entity selected =
                    editor.GetSelectedEntity();

                if (selected &&
                    selected.HasComponent<
                    NoJob::CameraComponent>())
                {
                    const auto& previewCamera =
                        selected.GetComponent<
                        NoJob::CameraComponent>();

                    const glm::mat4 cameraWorld =
                        editorScene.
                        GetWorldTransform(
                            selected);

                    const glm::mat4 previewView =
                        glm::inverse(
                            cameraWorld);

                    const glm::mat4
                        previewProjection =
                        previewCamera.
                        GetProjection(
                            320.0f /
                            180.0f);

                    cameraPreviewFramebuffer->
                        Bind();

                    NoJob::RenderCommand::
                        SetClearColor(
                            0.055f,
                            0.065f,
                            0.085f,
                            1.0f);

                    NoJob::RenderCommand::
                        Clear();

                    // Secondary camera preview reuses
                    // the shadow maps generated by the
                    // main scene render this frame.
                    NoJob::SceneRenderer::Render(
                        editorScene,
                        previewProjection *
                        previewView,
                        glm::vec3(
                            cameraWorld[3]),
                        false);

                    cameraPreviewFramebuffer->
                        Unbind();
                }
            }


            // --------------------------------------------------------
            // Backbuffer / Editor UI
            // --------------------------------------------------------

            NoJob::RenderCommand::SetViewport(
                0,
                0,
                1600,
                900);

            NoJob::RenderCommand::SetClearColor(
                0.035f,
                0.038f,
                0.045f,
                1.0f);

            NoJob::RenderCommand::Clear();


            editor.SetEditorCameraMatrices(
                renderView,
                renderProjection);

            editor.BeginFrame();
            editor.Draw();


            // --------------------------------------------------------
            // Navigation requests
            // --------------------------------------------------------

            if (!isPlaying &&
                editor.
                ConsumeBakeNavMeshRequest())
            {
                if (navigationSystem.Bake(
                    editorScene))
                {
                    navMeshDebugRenderer.Build(
                        navigationSystem.
                        GetNavMesh());

                    std::cout
                        << "[Navigation] NavMesh baked: "
                        << navigationSystem.
                        GetNavMesh().
                        GetPolygonCount()
                        << " polygons.\n";
                }
                else
                {
                    navMeshDebugRenderer.Clear();
                    frameExample();

                    std::cout
                        << "[Navigation] "
                        "NavMesh bake produced "
                        "no walkable polygons.\n";
                }
            }


            if (!isPlaying &&
                editor.
                ConsumeClearNavMeshRequest())
            {
                navigationSystem.Clear();
                navMeshDebugRenderer.Clear();

                std::cout
                    << "[Navigation] "
                    "NavMesh cleared.\n";
            }


            // --------------------------------------------------------
            // Play
            // --------------------------------------------------------

            if (editor.ConsumePlayRequest() &&
                !isPlaying)
            {
                std::cerr << "[Play] Copying editor scene...\n";
                runtimeScene =
                    editorScene.Copy();
                std::cerr << "[Play] Scene copy complete.\n";

                // The runtime scene does not own navigation.
                // It uses the NavMesh already baked by the
                // editor NavigationSystem.
                runtimeScene->
                    SetNavigationSystem(
                        &navigationSystem);

                std::cerr << "[Play] Starting runtime scripts...\n";
                runtimeScene->OnRuntimeStart();
                std::cerr << "[Play] Runtime scripts started.\n";

                std::cerr << "[Play] Starting physics...\n";
                physics.Start(*runtimeScene);
                std::cerr << "[Play] Physics started.\n";

                // Connect AI perception to the active physics world.
                runtimeScene->SetSpatialQuery(&spatialQuery);

                activeScene = runtimeScene.get();

                isPlaying = true;
                isPaused = false;

                editor.SetScene(
                    activeScene);

                editor.SetRuntimeState(
                    true,
                    false);
                std::cerr << "[Play] Play mode initialized.\n";
            }


            // --------------------------------------------------------
            // Pause
            // --------------------------------------------------------

            if (editor.ConsumePauseRequest() &&
                isPlaying)
            {
                isPaused = !isPaused;

                editor.SetRuntimeState(
                    true,
                    isPaused);
            }


            // --------------------------------------------------------
            // Stop
            // --------------------------------------------------------

            if (editor.ConsumeStopRequest() &&
                isPlaying)
            {
                // Disconnect the spatial query before stopping physics.
                runtimeScene->SetSpatialQuery(nullptr);

                runtimeScene->OnRuntimeStop();

                physics.Stop();

                runtimeScene.reset();

                activeScene =
                    &editorScene;

                isPlaying = false;
                isPaused = false;

                editor.SetScene(
                    activeScene);

                editor.SetRuntimeState(
                    false,
                    false);
            }


            // New documents use the same camera/lights template as startup.
            if (!isPlaying && editor.ConsumeNewSceneRequest() && canReplaceScene()) {
                NoJob::CreateDefaultEditorScene(editorScene);
                currentScenePath.clear();
                updateSceneTitle();
                savedSceneContent = sceneContent();
                activeScene = &editorScene;
                editor.ResetSceneDocument(activeScene);
                editor.ResetSceneOverlays();
                navigationSystem.Clear();
                navMeshDebugRenderer.Clear();
                frameNewScene();
                std::cout << "[Scene] New untitled scene created.\n";
            }
            if (!isPlaying && editor.ConsumeSaveSceneRequest()) saveScene(false);
            if (!isPlaying && editor.ConsumeSaveSceneAsRequest()) saveScene(true);


            // --------------------------------------------------------
            // Load Scene
            // --------------------------------------------------------

            if (!isPlaying &&
                editor.
                ConsumeLoadSceneRequest())
            {
                auto selected=editor.ConsumeExampleScenePath();
                if(selected.empty())selected=NoJob::SceneFileDialog::Open(NoJob::AssetManager::GetProjectRoot()/"Assets/Scenes");

                NoJob::Scene loadedScene;
                if (!selected.empty() && canReplaceScene() &&
                    NoJob::SceneSerializer::Load(
                        loadedScene,
                        selected,
                        cubeMesh,
                        cubeMaterial))
                {
                    editorScene.RestoreFrom(loadedScene);
                    currentScenePath = selected;
                    updateSceneTitle();

                    activeScene =
                        &editorScene;

                    editor.ResetSceneDocument(activeScene);
                    savedSceneContent = sceneContent();
                    frameExample();

                    // The baked NavMesh belonged to
                    // the previous scene.
                    navigationSystem.Clear();
                    navMeshDebugRenderer.Clear();

                    std::cout
                        << "[Scene] Opened "
                        << currentScenePath.string()
                        << "\n";
                }
            }


            // --------------------------------------------------------
            // Graphics validation scene
            // --------------------------------------------------------

            if (!isPlaying &&
                editor.
                ConsumeGraphicsTestSceneRequest() && canReplaceScene())
            {
                currentScenePath.clear();
                updateSceneTitle();
                editor.ResetSceneDocument(&editorScene);
                // This replaces the scene, therefore
                // any previously baked NavMesh is stale.
                navigationSystem.Clear();
                navMeshDebugRenderer.Clear();

                for (auto e :
                    editorScene.GetEntities())
                {
                    editorScene.DestroyEntity(e);
                }


                auto makeCube =
                    [&](const char* name,
                        glm::vec3 pos,
                        glm::vec3 scale,
                        glm::vec4 color,
                        float metal,
                        float rough,
                        float emissive)
                    {
                        auto mat =
                            std::make_shared<
                            NoJob::Material>(
                                shader,
                                color);

                        mat->Metallic() = metal;
                        mat->Roughness() = rough;

                        mat->AmbientOcclusion() =
                            1.0f;

                        mat->EmissiveColor() =
                            glm::vec3(color);

                        mat->EmissiveStrength() =
                            emissive;

                        auto e =
                            editorScene.
                            CreateEntity(name);

                        e.AddComponent<
                            NoJob::MeshComponent>(
                                cubeMesh);

                        e.AddComponent<
                            NoJob::
                            MeshRendererComponent>(
                                mat);

                        auto& tr =
                            e.GetComponent<
                            NoJob::
                            TransformComponent>();

                        tr.Position = pos;
                        tr.Scale = scale;

                        return e;
                    };


                makeCube(
                    "PBR Dielectric",
                    { -4.5f, 0.0f, 0.0f },
                    { 1, 1, 1 },
                    {
                        0.72f,
                        0.18f,
                        0.08f,
                        1
                    },
                    0.0f,
                    0.35f,
                    0.0f);

                makeCube(
                    "PBR Metal",
                    { -1.5f, 0.0f, 0.0f },
                    { 1, 1, 1 },
                    {
                        0.75f,
                        0.72f,
                        0.62f,
                        1
                    },
                    1.0f,
                    0.16f,
                    0.0f);

                makeCube(
                    "Rough Surface",
                    { 1.5f, 0.0f, 0.0f },
                    { 1, 1, 1 },
                    {
                        0.12f,
                        0.35f,
                        0.75f,
                        1
                    },
                    0.25f,
                    0.92f,
                    0.0f);

                makeCube(
                    "HDR Emissive",
                    { 4.5f, 0.0f, 0.0f },
                    { 1, 1, 1 },
                    {
                        0.15f,
                        0.8f,
                        0.32f,
                        1
                    },
                    0.0f,
                    0.4f,
                    8.0f);

                makeCube(
                    "Shadow Receiver",
                    { 0.0f, -1.6f, 0.0f },
                    { 7.0f, 0.25f, 3.0f },
                    {
                        0.18f,
                        0.20f,
                        0.23f,
                        1
                    },
                    0.0f,
                    0.75f,
                    0.0f);


                auto cam =
                    editorScene.CreateEntity(
                        "Graphics Test Camera");

                auto& ct =
                    cam.GetComponent<
                    NoJob::
                    TransformComponent>();

                ct.Position =
                {
                    0.0f,
                    3.5f,
                    11.5f
                };

                ct.Rotation =
                {
                    glm::radians(-14.0f),
                    0.0f,
                    0.0f
                };

                cam.AddComponent<
                    NoJob::CameraComponent>();


                auto sunE =
                    editorScene.CreateEntity(
                        "Directional Shadow Test");

                sunE.GetComponent<
                    NoJob::
                    TransformComponent>().
                    Rotation =
                {
                    glm::radians(-52.0f),
                    glm::radians(-28.0f),
                    0.0f
                };

                NoJob::
                    DirectionalLightComponent dl;

                dl.Intensity = 1.15f;
                dl.CastShadows = true;

                sunE.AddComponent<
                    NoJob::
                    DirectionalLightComponent>(
                        dl);


                auto pointE =
                    editorScene.CreateEntity(
                        "Point Shadow Test");

                pointE.GetComponent<
                    NoJob::
                    TransformComponent>().
                    Position =
                {
                    -3.0f,
                    2.8f,
                    2.0f
                };

                NoJob::PointLightComponent pl;

                pl.Intensity = 1.4f;
                pl.Range = 7.0f;
                pl.CastShadows = true;

                pointE.AddComponent<
                    NoJob::PointLightComponent>(
                        pl);


                auto spotE =
                    editorScene.CreateEntity(
                        "Spot Shadow Test");

                auto& st =
                    spotE.GetComponent<
                    NoJob::
                    TransformComponent>();

                st.Position =
                {
                    3.0f,
                    4.0f,
                    3.0f
                };

                st.Rotation =
                {
                    glm::radians(-55.0f),
                    glm::radians(18.0f),
                    0.0f
                };

                NoJob::SpotLightComponent sl;

                sl.Intensity = 4.0f;
                sl.Range = 12.0f;
                sl.CastShadows = true;

                spotE.AddComponent<
                    NoJob::SpotLightComponent>(
                        sl);


                activeScene =
                    &editorScene;

                editor.SetScene(
                    activeScene);

                const auto graphicsScenePath =
                    NoJob::AssetManager::
                    GetProjectRoot()
                    /
                    "Assets/Scenes/"
                    "GraphicsValidation.nojobscene";

                std::filesystem::
                    create_directories(
                        graphicsScenePath.
                        parent_path());

                NoJob::SceneSerializer::Save(
                    editorScene,
                    graphicsScenePath);

                std::cout
                    << "[Graphics] Validation "
                    "scene generated and saved.\n";
            }


            editor.EndFrame();

            window.SwapBuffers();
            if(smoke&&++smokeFrames>=120)glfwSetWindowShouldClose(window.GetNativeWindow(),GLFW_TRUE);
        }


        // ------------------------------------------------------------
        // Shutdown
        // ------------------------------------------------------------
        if (runtimeScene)
        {
            runtimeScene->SetSpatialQuery(nullptr);
            runtimeScene->OnRuntimeStop();

            physics.Stop();

            runtimeScene.reset();
            activeScene = &editorScene;
            editor.SetScene(activeScene);
            editor.SetRuntimeState(false, false);
        }
        editor.Shutdown();

        navMeshDebugRenderer.Clear();
        navigationSystem.Clear();

        cameraPreviewFramebuffer.reset();
        framebuffer.reset();

        gridVA.reset();
        gridIB.reset();
        gridVB.reset();

        cubeMaterial.reset();
        cubeMesh.reset();
        shader.reset();

        NoJob::SceneRenderer::Shutdown();
        NoJob::Renderer::Shutdown();

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Fatal] Unhandled std::exception: " << e.what() << '\n';
#ifdef _WIN32
        MessageBoxA(nullptr,e.what(),"NJE startup error",MB_OK|MB_ICONERROR);
#endif
        NoJob::ProjectScriptBuildSystem::Shutdown();
        return 1;
    }
    catch (...)
    {
        std::cerr << "[Fatal] Unhandled non-std exception during editor execution.\n";
        NoJob::ProjectScriptBuildSystem::Shutdown();
        return 1;
    }
}