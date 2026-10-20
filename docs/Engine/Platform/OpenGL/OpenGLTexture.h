#pragma once
#include "Engine/Renderer/Texture.h"

#include <string>

namespace NoJob
{
    class OpenGLTexture2D final : public Texture2D
    {
    public:
        OpenGLTexture2D(std::uint32_t width, std::uint32_t height);
        explicit OpenGLTexture2D(const std::string& path);
        ~OpenGLTexture2D() override;

        void Bind(std::uint32_t slot = 0) const override;
        std::uint32_t GetRendererID() const override { return m_RendererID; }
        const std::string& GetPath() const override { return m_Path; }

    private:
        std::uint32_t m_RendererID = 0;
        std::string m_Path = "Checkerboard";
    };
}
