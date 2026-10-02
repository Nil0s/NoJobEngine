#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace NoJob
{
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    struct MeshVertex
    {
        float Position[3];
    };

    class Mesh
    {
    public:
        Mesh(
            const std::vector<MeshVertex>& vertices,
            const std::vector<std::uint32_t>& indices);

        const std::shared_ptr<VertexArray>& GetVertexArray() const
        {
            return m_VertexArray;
        }

        static std::shared_ptr<Mesh> CreateCube();

    private:
        std::shared_ptr<VertexArray> m_VertexArray;
        std::shared_ptr<VertexBuffer> m_VertexBuffer;
        std::shared_ptr<IndexBuffer> m_IndexBuffer;
    };
}
