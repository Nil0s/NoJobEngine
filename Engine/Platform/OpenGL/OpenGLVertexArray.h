#pragma once
#include "Engine/Renderer/VertexArray.h"
#include <cstdint>

namespace NoJob
{
    class OpenGLVertexArray final : public VertexArray
    {
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray() override;

        void Bind() const override;
        void Unbind() const override;

        void SetVertexBuffer(
            const std::shared_ptr<VertexBuffer>& vertexBuffer) override;

        void SetIndexBuffer(
            const std::shared_ptr<IndexBuffer>& indexBuffer) override;

        const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const override
        {
            return m_IndexBuffer;
        }

    private:
        std::uint32_t m_RendererID = 0;
        std::shared_ptr<VertexBuffer> m_VertexBuffer;
        std::shared_ptr<IndexBuffer> m_IndexBuffer;
    };
}
