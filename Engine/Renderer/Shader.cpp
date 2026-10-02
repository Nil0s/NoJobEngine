#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Platform/OpenGL/OpenGLShader.h"
#include <stdexcept>

namespace NoJob
{
    std::shared_ptr<Shader> Shader::Create(
        const std::string& vertexSource,
        const std::string& fragmentSource)
    {
        switch (RendererAPI::GetAPI())
        {
        case GraphicsAPI::OpenGL:
            return std::make_shared<OpenGLShader>(
                vertexSource, fragmentSource);
        default:
            throw std::runtime_error("NoJobEngine: unsupported graphics API.");
        }
    }
}
