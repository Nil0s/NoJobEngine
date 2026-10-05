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

            uniform mat4 u_Transform;
            uniform mat4 u_ViewProjection;

            out vec3 v_Normal;
            out vec2 v_TexCoord;

            void main()
            {
                gl_Position =
                    u_ViewProjection *
                    u_Transform *
                    vec4(a_Position, 1.0);

                v_Normal =
                    mat3(transpose(inverse(u_Transform))) * a_Normal;
                v_TexCoord = a_TexCoord;
            }
        )";

        const std::string fragmentShaderSource = R"(
            #version 460 core

            layout(location = 0) out vec4 o_Color;

            in vec3 v_Normal;
            in vec2 v_TexCoord;

            uniform vec4 u_Color;
            uniform sampler2D u_Texture;
            uniform int u_UseTexture;

            void main()
            {
                vec4 baseColor = u_Color;

                if (u_UseTexture == 1)
                    baseColor *= texture(u_Texture, v_TexCoord);

                vec3 normal = normalize(v_Normal);
                vec3 lightDirection =
                    normalize(vec3(0.45, 0.80, 0.35));

                float diffuse =
                    max(dot(normal, lightDirection), 0.0);

                float lighting = 0.25 + diffuse * 0.75;

                o_Color =
                    vec4(baseColor.rgb * lighting, baseColor.a);
            }
        )";

        auto shader = NoJob::Shader::Create(
            vertexShaderSource,
            fragmentShaderSource);

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

        NoJob::PhysicsSystem physics;

        NoJob::FramebufferSpecification framebufferSpecification;
        framebufferSpecification.Width = 1280;
        framebufferSpecification.Height = 720;

        auto framebuffer =
            NoJob::Framebuffer::Create(framebufferSpecification);

        NoJob::EditorLayer editor;
        editor.Init(window.GetNativeWindow(), &editorScene);
        editor.SetSelectedEntity(cube);
        editor.SetDefaultCubeAssets(cubeMesh, cubeMaterial);
        editor.SetViewportTexture(
            framebuffer->GetColorAttachmentRendererID());

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

            editorCamera.OnUpdate(
                window.GetNativeWindow(),
                deltaTime,
                editor.IsViewportHovered());

            framebuffer->Bind();

            NoJob::RenderCommand::SetClearColor(
                0.055f, 0.065f, 0.085f, 1.0f);
            NoJob::RenderCommand::Clear();

            const glm::mat4 viewProjection =
                editorCamera.GetViewProjection();

            NoJob::Renderer::Submit(
                gridVA,
                shader,
                glm::mat4(1.0f),
                viewProjection,
                glm::vec4(0.28f, 0.30f, 0.34f, 1.0f));

            // SceneRenderer now discovers and draws renderable entities.
            NoJob::SceneRenderer::Render(*activeScene, viewProjection);

            framebuffer->Unbind();

            NoJob::RenderCommand::SetViewport(0, 0, 1600, 900);
            NoJob::RenderCommand::SetClearColor(
                0.035f, 0.038f, 0.045f, 1.0f);
            NoJob::RenderCommand::Clear();

            editor.SetEditorCameraMatrices(
                editorCamera.GetViewMatrix(),
                editorCamera.GetProjectionMatrix());

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

            editor.EndFrame();

            window.SwapBuffers();
        }

        editor.Shutdown();

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
