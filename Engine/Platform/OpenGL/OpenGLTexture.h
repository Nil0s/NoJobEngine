#pragma once
#include "Engine/Renderer/Texture.h"

namespace NoJob
{
    class OpenGLTexture2D final : public Texture2D
    {
    public:
        OpenGLTexture2D(std::uint32_t width, std::uint32_t height);
        ~OpenGLTexture2D() override;

        void Bind(std::uint32_t slot = 0) const override;
        std::uint32_t GetRendererID() const override { return m_RendererID; }

    private:
        std::uint32_t m_RendererID = 0;
    };
}
