#pragma once
#include <cstdint>
#include <memory>

namespace NoJob
{
    struct FramebufferSpecification
    {
        std::uint32_t Width = 1280;
        std::uint32_t Height = 720;
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

        static std::shared_ptr<Framebuffer> Create(
            const FramebufferSpecification& specification);
    };
}
