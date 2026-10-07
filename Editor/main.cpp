#include "Engine/Core/Window.h"
#include "Engine/Asset/AssetManager.h"
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
#include "Engine/Renderer/DefaultSceneShaders.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Scene/SceneSerializer.h"

#include "Engine/AI/Navigation/NavMeshDebugRenderer.h"
#include "Engine/AI/Navigation/NavigationSystem.h"

#include "Editor/EditorCamera.h"
#include "Editor/EditorLayer.h"
#include "Editor/Scene/SceneFileDialog.h"

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

        std::filesystem::path currentScenePath =
            NoJob::AssetManager::GetProjectRoot()
            / "Assets/Scenes/CurrentScene.nojobscene";


        // ------------------------------------------------------------
        // Default cube
        // ------------------------------------------------------------

        NoJob::Entity cube =
            editorScene.CreateEntity("Cube");

        cube.AddComponent<NoJob::MeshComponent>(
            cubeMesh);

        cube.AddComponent<
            NoJob::MeshRendererComponent>(
                cubeMaterial);

        cube.GetComponent<
            NoJob::TransformComponent>().Position =
        { 0.0f, 2.0f, 0.0f };

        cube.AddComponent<
            NoJob::RigidbodyComponent>();

        cube.AddComponent<
            NoJob::BoxColliderComponent>();


        // ------------------------------------------------------------
        // Ground
        // ------------------------------------------------------------

        auto groundMaterial =
            std::make_shared<NoJob::Material>(
                shader,
                glm::vec4(
                    0.28f,
                    0.32f,
                    0.38f,
                    1.0f));

        groundMaterial->SetTexture(
            checkerTexture);

        NoJob::Entity ground =
            editorScene.CreateEntity("Ground");

        ground.AddComponent<
            NoJob::MeshComponent>(
                cubeMesh);

        ground.AddComponent<
            NoJob::MeshRendererComponent>(
                groundMaterial);

        auto& groundTransform =
            ground.GetComponent<
            NoJob::TransformComponent>();

        groundTransform.Position =
        { 0.0f, -1.5f, 0.0f };

        groundTransform.Scale =
        { 6.0f, 0.5f, 6.0f };

        NoJob::RigidbodyComponent groundBody;

        groundBody.Type =
            NoJob::RigidbodyType::Static;

        groundBody.UseGravity = false;

        ground.AddComponent<
            NoJob::RigidbodyComponent>(
                groundBody);

        ground.AddComponent<
            NoJob::BoxColliderComponent>();


        // ------------------------------------------------------------
        // Main camera
        // ------------------------------------------------------------

        NoJob::Entity mainCamera =
            editorScene.CreateEntity(
                "Main Camera");

        auto& cameraTransform =
            mainCamera.GetComponent<
            NoJob::TransformComponent>();

        cameraTransform.Position =
        { 0.0f, 2.5f, 7.0f };

        cameraTransform.Rotation =
        {
            glm::radians(-10.0f),
            0.0f,
            0.0f
        };

        mainCamera.AddComponent<
            NoJob::CameraComponent>();


        // ------------------------------------------------------------
        // Lights
        // ------------------------------------------------------------

        NoJob::Entity sun =
            editorScene.CreateEntity("Sun");

        auto& sunTransform =
            sun.GetComponent<
            NoJob::TransformComponent>();

        sunTransform.Rotation =
        {
            glm::radians(-50.0f),
            glm::radians(-30.0f),
            0.0f
        };

        NoJob::DirectionalLightComponent
            sunLight;

        sunLight.Intensity = 1.1f;

        sun.AddComponent<
            NoJob::DirectionalLightComponent>(
                sunLight);

        NoJob::Entity fillLight =
            editorScene.CreateEntity(
                "Point Light");

        fillLight.GetComponent<
            NoJob::TransformComponent>().Position =
        { 2.5f, 2.5f, 2.0f };

        NoJob::PointLightComponent
            pointLight;

        pointLight.Intensity = 0.75f;
        pointLight.Range = 8.0f;

        fillLight.AddComponent<
            NoJob::PointLightComponent>(
                pointLight);


        // ------------------------------------------------------------
        // Physics
        // ------------------------------------------------------------

        NoJob::PhysicsSystem physics;


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

        editor.SetSelectedEntity(cube);

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


        // ------------------------------------------------------------
        // Navigation
        // ------------------------------------------------------------

        NoJob::NavigationSystem
            navigationSystem;

        NoJob::NavMeshDebugRenderer
            navMeshDebugRenderer;


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

            if (!isPlaying)
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
                runtimeScene =
                    editorScene.Copy();

                runtimeScene->
                    OnRuntimeStart();

                physics.Start(
                    *runtimeScene);

                activeScene =
                    runtimeScene.get();

                isPlaying = true;
                isPaused = false;

                editor.SetScene(
                    activeScene);

                editor.SetRuntimeState(
                    true,
                    false);
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
                physics.Stop();

                runtimeScene->
                    OnRuntimeStop();

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


            // --------------------------------------------------------
            // Save Scene
            // --------------------------------------------------------

            if (!isPlaying &&
                editor.
                ConsumeSaveSceneRequest())
            {
                std::filesystem::
                    create_directories(
                        currentScenePath.
                        parent_path());

                if (NoJob::SceneSerializer::Save(
                    editorScene,
                    currentScenePath))
                {
                    std::cout
                        << "[Scene] Saved "
                        << currentScenePath.string()
                        << "\n";
                }
            }


            // --------------------------------------------------------
            // Save Scene As
            // --------------------------------------------------------

            if (!isPlaying &&
                editor.
                ConsumeSaveSceneAsRequest())
            {
                const auto selected =
                    NoJob::SceneFileDialog::SaveAs(
                        NoJob::AssetManager::
                        GetProjectRoot()
                        /
                        "Assets/Scenes");

                if (!selected.empty())
                {
                    std::filesystem::
                        create_directories(
                            selected.parent_path());

                    if (NoJob::SceneSerializer::Save(
                        editorScene,
                        selected))
                    {
                        currentScenePath =
                            selected;

                        std::cout
                            << "[Scene] Saved As "
                            << currentScenePath.
                            string()
                            << "\n";
                    }
                }
            }


            // --------------------------------------------------------
            // Load Scene
            // --------------------------------------------------------

            if (!isPlaying &&
                editor.
                ConsumeLoadSceneRequest())
            {
                const auto selected =
                    NoJob::SceneFileDialog::Open(
                        NoJob::AssetManager::
                        GetProjectRoot()
                        /
                        "Assets/Scenes");

                if (!selected.empty() &&
                    NoJob::SceneSerializer::Load(
                        editorScene,
                        selected,
                        cubeMesh,
                        cubeMaterial))
                {
                    currentScenePath =
                        selected;

                    activeScene =
                        &editorScene;

                    editor.SetScene(
                        activeScene);

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
                ConsumeGraphicsTestSceneRequest())
            {
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
        }


        // ------------------------------------------------------------
        // Shutdown
        // ------------------------------------------------------------

        editor.Shutdown();

        navMeshDebugRenderer.Clear();
        navigationSystem.Clear();

        cameraPreviewFramebuffer.reset();
        framebuffer.reset();

        gridVA.reset();
        gridIB.reset();
        gridVB.reset();

        groundMaterial.reset();
        cubeMaterial.reset();
        cubeMesh.reset();
        shader.reset();

        NoJob::SceneRenderer::Shutdown();
        NoJob::Renderer::Shutdown();

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << e.what()
            << '\n';

        return 1;
    }
}