#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Platform/OpenGL/OpenGLVertexArray.h"
#include <stdexcept>

namespace NoJob
{
    std::shared_ptr<VertexArray> VertexArray::Create()
    {
        switch (RendererAPI::GetAPI())
        {
        case GraphicsAPI::OpenGL:
            return std::make_shared<OpenGLVertexArray>();
        default:
            throw std::runtime_error("NoJobEngine: unsupported graphics API.");
        }
    }
}
