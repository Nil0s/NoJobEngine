#include "Engine/Renderer/Texture.h"
#include "Engine/Platform/OpenGL/OpenGLTexture.h"

namespace NoJob
{
    std::shared_ptr<Texture2D> Texture2D::CreateCheckerboard(
        std::uint32_t width, std::uint32_t height)
    {
        return std::make_shared<OpenGLTexture2D>(width, height);
    }

    std::shared_ptr<Texture2D> Texture2D::Create(
        const std::string& path)
    {
        return std::make_shared<OpenGLTexture2D>(path);
    }
}
