#include "Engine/Renderer/Mesh.h"

#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/VertexArray.h"

#include <vector>

namespace NoJob
{
    Mesh::Mesh(
        const std::vector<MeshVertex>& vertices,
        const std::vector<std::uint32_t>& indices)
    {
        // The current VertexBuffer API accepts raw float position data.
        // Flatten MeshVertex into the format expected by VertexBuffer::Create.
        std::vector<float> vertexData;
        vertexData.reserve(vertices.size() * 3);

        for (const MeshVertex& vertex : vertices)
        {
            vertexData.push_back(vertex.Position[0]);
            vertexData.push_back(vertex.Position[1]);
            vertexData.push_back(vertex.Position[2]);
        }

        m_VertexBuffer = VertexBuffer::Create(
            vertexData.data(),
            static_cast<std::uint32_t>(
                vertexData.size() * sizeof(float)));

        m_IndexBuffer = IndexBuffer::Create(
            indices.data(),
            static_cast<std::uint32_t>(indices.size()));

        m_VertexArray = VertexArray::Create();
        m_VertexArray->SetVertexBuffer(m_VertexBuffer);
        m_VertexArray->SetIndexBuffer(m_IndexBuffer);
    }

    std::shared_ptr<Mesh> Mesh::CreateCube()
    {
        const std::vector<MeshVertex> vertices =
        {
            {{ -0.5f, -0.5f,  0.5f }},
            {{  0.5f, -0.5f,  0.5f }},
            {{  0.5f,  0.5f,  0.5f }},
            {{ -0.5f,  0.5f,  0.5f }},

            {{ -0.5f, -0.5f, -0.5f }},
            {{  0.5f, -0.5f, -0.5f }},
            {{  0.5f,  0.5f, -0.5f }},
            {{ -0.5f,  0.5f, -0.5f }}
        };

        const std::vector<std::uint32_t> indices =
        {
            0, 1, 2, 2, 3, 0,
            1, 5, 6, 6, 2, 1,
            5, 4, 7, 7, 6, 5,
            4, 0, 3, 3, 7, 4,
            3, 2, 6, 6, 7, 3,
            4, 5, 1, 1, 0, 4
        };

        return std::make_shared<Mesh>(vertices, indices);
    }
}
