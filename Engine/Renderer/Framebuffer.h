#pragma once
#include <cstdint>
#include <memory>

namespace NoJob
{
    struct FramebufferSpecification
    {
        std::uint32_t Width = 1280;
        std::uint32_t Height = 720;

        // Advanced Renderer V2. Scene color is rendered in linear HDR and
        // resolved through the post-processing chain when Unbind() is called.
        bool HDR = true;
        bool Bloom = true;
        bool FXAA = true;
        bool ScreenSpaceAO = true;
        float Exposure = 1.0f;
        float BloomThreshold = 1.0f;
        float BloomStrength = 0.12f;
        float AOIntensity = 0.35f;
    };

    class Framebuffer
    {
    public:
        virtual ~Framebuffer() = default;

        virtual void Bind() = 0;
        virtual void Unbind() = 0;
        virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;

        virtual std::uint32_t GetColorAttachmentRendererID() const = 0;
        virtual const FramebufferSpecification& GetSpecification() const = 0;
        virtual void SetPostProcessSettings(const FramebufferSpecification& specification) = 0;

        static std::shared_ptr<Framebuffer> Create(
            const FramebufferSpecification& specification);
    };
}
