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

            out vec3 v_WorldPosition;
            out vec3 v_Normal;
            out vec2 v_TexCoord;

            void main()
            {
                vec4 worldPosition =
                    u_Transform * vec4(a_Position, 1.0);

                v_WorldPosition = worldPosition.xyz;
                v_Normal =
                    mat3(transpose(inverse(u_Transform))) * a_Normal;
                v_TexCoord = a_TexCoord;

                gl_Position =
                    u_ViewProjection * worldPosition;
            }
        )";

        const std::string fragmentShaderSource = R"(
            #version 460 core

            layout(location = 0) out vec4 o_Color;

            in vec3 v_WorldPosition;
            in vec3 v_Normal;
            in vec2 v_TexCoord;

            uniform vec4 u_Color;
            uniform sampler2D u_Texture;
            uniform int u_UseTexture;
            uniform vec3 u_ViewPosition;
            uniform vec3 u_AmbientColor;

            struct DirectionalLight
            {
                vec3 direction;
                vec3 color;
                float intensity;
            };

            struct PointLight
            {
                vec3 position;
                vec3 color;
                float intensity;
                float range;
            };

            struct SpotLight
            {
                vec3 position;
                vec3 direction;
                vec3 color;
                float intensity;
                float range;
                float innerCos;
                float outerCos;
            };

            uniform int u_HasDirectionalLight;
            uniform DirectionalLight u_DirectionalLight;

            uniform int u_PointLightCount;
            uniform PointLight u_PointLights[4];

            uniform int u_SpotLightCount;
            uniform SpotLight u_SpotLights[4];

            vec3 EvaluateLight(
                vec3 normal,
                vec3 viewDirection,
                vec3 lightDirection,
                vec3 lightColor,
                float intensity)
            {
                float diffuse =
                    max(dot(normal, lightDirection), 0.0);
                diffuse = diffuse * 0.92 + 0.08;

                vec3 halfDirection =
                    normalize(lightDirection + viewDirection);
                float specular =
                    pow(max(dot(normal, halfDirection), 0.0), 32.0);

                return lightColor * intensity *
                    (diffuse + specular * 0.18);
            }

            void main()
            {
                vec4 baseColor = u_Color;
                if (u_UseTexture == 1)
                    baseColor *= texture(u_Texture, v_TexCoord);

                vec3 normal = normalize(v_Normal);
                vec3 viewDirection =
                    normalize(u_ViewPosition - v_WorldPosition);

                vec3 lighting = u_AmbientColor;

                if (u_HasDirectionalLight == 1)
                {
                    lighting += EvaluateLight(
                        normal,
                        viewDirection,
                        normalize(-u_DirectionalLight.direction),
                        u_DirectionalLight.color,
                        u_DirectionalLight.intensity);
                }

                for (int i = 0; i < u_PointLightCount; ++i)
                {
                    vec3 delta =
                        u_PointLights[i].position - v_WorldPosition;
                    float distanceToLight = length(delta);
                    vec3 lightDirection =
                        delta / max(distanceToLight, 0.0001);

                    float normalizedDistance =
                        distanceToLight /
                        max(u_PointLights[i].range, 0.0001);
                    float attenuation =
                        clamp(1.0 - normalizedDistance, 0.0, 1.0);
                    attenuation *= attenuation;

                    lighting += EvaluateLight(
                        normal,
                        viewDirection,
                        lightDirection,
                        u_PointLights[i].color,
                        u_PointLights[i].intensity * attenuation);
                }

                for (int i = 0; i < u_SpotLightCount; ++i)
                {
                    vec3 delta =
                        u_SpotLights[i].position - v_WorldPosition;
                    float distanceToLight = length(delta);
                    vec3 lightDirection =
                        delta / max(distanceToLight, 0.0001);

                    float theta = dot(
                        normalize(-lightDirection),
                        normalize(u_SpotLights[i].direction));

                    float cone =
                        smoothstep(
                            u_SpotLights[i].outerCos,
                            u_SpotLights[i].innerCos,
                            theta);

                    float normalizedDistance =
                        distanceToLight /
                        max(u_SpotLights[i].range, 0.0001);
                    float attenuation =
                        clamp(1.0 - normalizedDistance, 0.0, 1.0);
                    attenuation *= attenuation;

                    lighting += EvaluateLight(
                        normal,
                        viewDirection,
                        lightDirection,
                        u_SpotLights[i].color,
                        u_SpotLights[i].intensity *
                        attenuation * cone);
                }

                o_Color = vec4(
                    baseColor.rgb * lighting,
                    baseColor.a);
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
        pointLight.Intensity = 2.0f;
        pointLight.Range = 8.0f;
        fillLight.AddComponent<NoJob::PointLightComponent>(pointLight);

        NoJob::PhysicsSystem physics;

        NoJob::FramebufferSpecification framebufferSpecification;
        framebufferSpecification.Width = 1280;
        framebufferSpecification.Height = 720;

        auto framebuffer =
            NoJob::Framebuffer::Create(framebufferSpecification);

        NoJob::FramebufferSpecification cameraPreviewSpecification;
        cameraPreviewSpecification.Width = 320;
        cameraPreviewSpecification.Height = 180;
        auto cameraPreviewFramebuffer =
            NoJob::Framebuffer::Create(cameraPreviewSpecification);

        NoJob::EditorLayer editor;
        editor.Init(window.GetNativeWindow(), &editorScene);
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
