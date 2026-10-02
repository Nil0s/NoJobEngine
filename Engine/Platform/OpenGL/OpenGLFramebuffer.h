#pragma once
#include "Engine/Renderer/Framebuffer.h"

namespace NoJob
{
    class OpenGLFramebuffer final : public Framebuffer
    {
    public:
        explicit OpenGLFramebuffer(
            const FramebufferSpecification& specification);
        ~OpenGLFramebuffer() override;

        void Bind() override;
        void Unbind() override;
        void Resize(std::uint32_t width, std::uint32_t height) override;

        std::uint32_t GetColorAttachmentRendererID() const override
        {
            return m_ColorAttachment;
        }

        const FramebufferSpecification& GetSpecification() const override
        {
            return m_Specification;
        }

    private:
        void Invalidate();

        FramebufferSpecification m_Specification;
        std::uint32_t m_RendererID = 0;
        std::uint32_t m_ColorAttachment = 0;
        std::uint32_t m_DepthAttachment = 0;
    };
}
