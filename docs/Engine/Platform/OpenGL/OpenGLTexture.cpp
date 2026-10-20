#include "Engine/Platform/OpenGL/OpenGLTexture.h"

#include <glad/gl.h>
#include <stb_image.h>

#include <stdexcept>
#include <vector>

namespace NoJob
{
    namespace
    {
        void ConfigureTexture(std::uint32_t id)
        {
            glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
    }

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

        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
        : m_Path(path)
    {
        stbi_set_flip_vertically_on_load(1);

        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_uc* data = stbi_load(
            path.c_str(),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha);

        if (!data || width <= 0 || height <= 0)
        {
            const std::string reason =
                stbi_failure_reason() ? stbi_failure_reason() : "unknown error";
            stbi_image_free(data);
            throw std::runtime_error(
                "NoJobEngine: could not load texture '" +
                path + "': " + reason);
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);

        int levels = 1;
        int size = width > height ? width : height;
        while (size > 1)
        {
            size /= 2;
            ++levels;
        }

        glTextureStorage2D(
            m_RendererID,
            levels,
            GL_RGBA8,
            width,
            height);

        glTextureSubImage2D(
            m_RendererID,
            0,
            0,
            0,
            width,
            height,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            data);

        ConfigureTexture(m_RendererID);
        glGenerateTextureMipmap(m_RendererID);

        stbi_image_free(data);
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
