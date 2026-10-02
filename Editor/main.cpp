#include "Engine/Core/Window.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Editor/EditorLayer.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

int main()
{
    try
    {
        NoJob::Window window({ "NoJobEngine", 1600, 900 });
        NoJob::Renderer::Init();

        NoJob::Scene scene;
        NoJob::Entity triangle = scene.CreateEntity("Triangle");

        NoJob::FramebufferSpecification framebufferSpecification;
        framebufferSpecification.Width = 1280;
        framebufferSpecification.Height = 720;

        auto framebuffer =
            NoJob::Framebuffer::Create(framebufferSpecification);

        NoJob::EditorLayer editor;
        editor.Init(window.GetNativeWindow(), &scene);
        editor.SetSelectedEntity(triangle);
        editor.SetViewportTexture(
            framebuffer->GetColorAttachmentRendererID());

        const float vertices[] =
        {
            -0.55f, -0.45f, 0.0f,
             0.55f, -0.45f, 0.0f,
             0.00f,  0.55f, 0.0f
        };

        const std::uint32_t indices[] = { 0, 1, 2 };

        auto vertexBuffer =
            NoJob::VertexBuffer::Create(vertices, sizeof(vertices));

        auto indexBuffer =
            NoJob::IndexBuffer::Create(indices, 3);

        auto vertexArray = NoJob::VertexArray::Create();
        vertexArray->SetVertexBuffer(vertexBuffer);
        vertexArray->SetIndexBuffer(indexBuffer);

        const std::string vertexShaderSource = R"(
            #version 460 core

            layout(location = 0) in vec3 a_Position;

            uniform mat4 u_Transform;

            void main()
            {
                gl_Position =
                    u_Transform * vec4(a_Position, 1.0);
            }
        )";

        const std::string fragmentShaderSource = R"(
            #version 460 core

            layout(location = 0) out vec4 o_Color;

            void main()
            {
                o_Color = vec4(0.95, 0.35, 0.15, 1.0);
            }
        )";

        auto shader = NoJob::Shader::Create(
            vertexShaderSource,
            fragmentShaderSource);

        while (!window.ShouldClose())
        {
            window.PollEvents();

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

            // Render the scene into the offscreen framebuffer.
            framebuffer->Bind();

            NoJob::RenderCommand::SetClearColor(
                0.08f, 0.09f, 0.11f, 1.0f);
            NoJob::RenderCommand::Clear();

            const auto& transform =
                triangle.GetComponent<NoJob::TransformComponent>();

            NoJob::Renderer::Submit(
                vertexArray,
                shader,
                transform.GetTransform());

            framebuffer->Unbind();

            // Clear the main application window, then draw ImGui.
            NoJob::RenderCommand::SetViewport(0, 0, 1600, 900);
            NoJob::RenderCommand::SetClearColor(
                0.035f, 0.038f, 0.045f, 1.0f);
            NoJob::RenderCommand::Clear();

            editor.BeginFrame();
            editor.Draw();
            editor.EndFrame();

            window.SwapBuffers();
        }

        editor.Shutdown();

        framebuffer.reset();
        shader.reset();
        vertexArray.reset();
        indexBuffer.reset();
        vertexBuffer.reset();

        NoJob::Renderer::Shutdown();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
