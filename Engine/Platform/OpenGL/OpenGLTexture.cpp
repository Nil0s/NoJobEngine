#include "Engine/Platform/OpenGL/OpenGLTexture.h"
#include <glad/gl.h>
#include <vector>

namespace NoJob
{
    OpenGLTexture2D::OpenGLTexture2D(
        std::uint32_t width, std::uint32_t height)
    {
        std::vector<std::uint8_t> pixels(width * height * 4);
        constexpr std::uint32_t cellSize = 32;

        for (std::uint32_t y = 0; y < height; ++y)
        {
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const bool light =
                    ((x / cellSize) + (y / cellSize)) % 2 == 0;
                const std::uint8_t c = light ? 230 : 55;
                const std::size_t i = (y * width + x) * 4;

                pixels[i + 0] = c;
                pixels[i + 1] = c;
                pixels[i + 2] = c;
                pixels[i + 3] = 255;
            }
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, GL_RGBA8, width, height);
        glTextureSubImage2D(
            m_RendererID, 0, 0, 0, width, height,
            GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        glTextureParameteri(
            m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture2D::Bind(std::uint32_t slot) const
    {
        glBindTextureUnit(slot, m_RendererID);
    }
}
