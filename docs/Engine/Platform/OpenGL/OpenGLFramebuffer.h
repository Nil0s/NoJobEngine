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
        void PresentToDefault(std::uint32_t width, std::uint32_t height) override;

        std::uint32_t GetColorAttachmentRendererID() const override
        {
            // The editor always displays the post-processed LDR result.
            return m_DisplayAttachment ? m_DisplayAttachment : m_ColorAttachment;
        }

        const FramebufferSpecification& GetSpecification() const override
        {
            return m_Specification;
        }

        void SetPostProcessSettings(const FramebufferSpecification& specification) override
        {
            // Width/height are owned by Resize(); post settings can change live.
            m_Specification.HDR = specification.HDR;
            m_Specification.Bloom = specification.Bloom;
            m_Specification.FXAA = specification.FXAA;
            m_Specification.ScreenSpaceAO = specification.ScreenSpaceAO;
            m_Specification.Exposure = specification.Exposure;
            m_Specification.BloomThreshold = specification.BloomThreshold;
            m_Specification.BloomStrength = specification.BloomStrength;
            m_Specification.AOIntensity = specification.AOIntensity;
        }

    private:
        void Invalidate();
        void EnsurePostProcessResources();
        void DestroyAttachments();
        void RunPostProcess();

        FramebufferSpecification m_Specification;
        std::uint32_t m_RendererID = 0;
        std::uint32_t m_ColorAttachment = 0;   // linear HDR scene
        std::uint32_t m_DepthAttachment = 0;
        std::uint32_t m_PostProcessFBO = 0;
        std::uint32_t m_DisplayAttachment = 0; // final LDR image for ImGui
        std::uint32_t m_PostProgram = 0;
        std::uint32_t m_FullscreenVAO = 0;
    };
}
