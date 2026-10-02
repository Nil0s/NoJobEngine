#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Platform/OpenGL/OpenGLRendererAPI.h"
#include <stdexcept>

namespace NoJob
{
    std::unique_ptr<RendererAPI> RenderCommand::s_RendererAPI;

    void RenderCommand::Init()
    {
        switch (RendererAPI::GetAPI())
        {
        case GraphicsAPI::OpenGL:
            s_RendererAPI = std::make_unique<OpenGLRendererAPI>();
            break;
        case GraphicsAPI::Vulkan:
            throw std::runtime_error(
                "NoJobEngine: Vulkan backend is not implemented yet.");
        default:
            throw std::runtime_error(
                "NoJobEngine: no graphics API selected.");
        }

        s_RendererAPI->Init();
    }

    void RenderCommand::SetViewport(
        std::uint32_t x,
        std::uint32_t y,
        std::uint32_t width,
        std::uint32_t height)
    {
        s_RendererAPI->SetViewport(x, y, width, height);
    }

    void RenderCommand::SetClearColor(float r, float g, float b, float a)
    {
        s_RendererAPI->SetClearColor(r, g, b, a);
    }

    void RenderCommand::Clear()
    {
        s_RendererAPI->Clear();
    }

    void RenderCommand::DrawIndexed(const VertexArray& vertexArray)
    {
        s_RendererAPI->DrawIndexed(vertexArray);
    }
}
