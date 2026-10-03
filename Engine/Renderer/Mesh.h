#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace NoJob
{
    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    struct MeshVertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
        glm::vec2 TexCoord{ 0.0f };
    };

    class Mesh
    {
    public:
        Mesh(const std::vector<MeshVertex>& vertices,
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
