#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Platform/OpenGL/OpenGLFramebuffer.h"
#include <stdexcept>

namespace NoJob
{
    std::shared_ptr<Framebuffer> Framebuffer::Create(
        const FramebufferSpecification& specification)
    {
        switch (RendererAPI::GetAPI())
        {
        case GraphicsAPI::OpenGL:
            return std::make_shared<OpenGLFramebuffer>(specification);
        default:
            throw std::runtime_error(
                "NoJobEngine: unsupported framebuffer graphics API.");
        }
    }
}
