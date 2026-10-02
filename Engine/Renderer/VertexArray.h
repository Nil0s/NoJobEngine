#pragma once
#include <memory>

namespace NoJob
{
    class VertexBuffer;
    class IndexBuffer;

    class VertexArray
    {
    public:
        virtual ~VertexArray() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void SetVertexBuffer(
            const std::shared_ptr<VertexBuffer>& vertexBuffer) = 0;

        virtual void SetIndexBuffer(
            const std::shared_ptr<IndexBuffer>& indexBuffer) = 0;

        virtual const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const = 0;

        static std::shared_ptr<VertexArray> Create();
    };
}
