#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Platform/OpenGL/OpenGLBuffer.h"
#include <stdexcept>

namespace NoJob
{
    std::shared_ptr<VertexBuffer> VertexBuffer::Create(
        const float* vertices, std::uint32_t size)
    {
        switch (RendererAPI::GetAPI())
        {
        case GraphicsAPI::OpenGL:
            return std::make_shared<OpenGLVertexBuffer>(vertices, size);
        default:
            throw std::runtime_error("NoJobEngine: unsupported graphics API.");
        }
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::Create(
        const std::uint32_t* indices, std::uint32_t count)
    {
        switch (RendererAPI::GetAPI())
        {
        case GraphicsAPI::OpenGL:
            return std::make_shared<OpenGLIndexBuffer>(indices, count);
        default:
            throw std::runtime_error("NoJobEngine: unsupported graphics API.");
        }
    }
}
