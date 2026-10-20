#pragma once
#include "Engine/Renderer/Buffer.h"

namespace NoJob
{
    class OpenGLVertexBuffer final : public VertexBuffer
    {
    public:
        OpenGLVertexBuffer(const float* vertices, std::uint32_t size);
        ~OpenGLVertexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

    private:
        std::uint32_t m_RendererID = 0;
    };

    class OpenGLIndexBuffer final : public IndexBuffer
    {
    public:
        OpenGLIndexBuffer(
            const std::uint32_t* indices,
            std::uint32_t count);
        ~OpenGLIndexBuffer() override;

        void Bind() const override;
        void Unbind() const override;
        std::uint32_t GetCount() const override { return m_Count; }

    private:
        std::uint32_t m_RendererID = 0;
        std::uint32_t m_Count = 0;
    };
}
