#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/BufferLayout.h"

namespace NoJob
{
    Mesh::Mesh(const std::vector<MeshVertex>& vertices,
               const std::vector<std::uint32_t>& indices)
    {
        std::vector<float> data;
        data.reserve(vertices.size() * 8);

        for (const MeshVertex& vertex : vertices)
        {
            data.insert(data.end(),
            {
                vertex.Position.x, vertex.Position.y, vertex.Position.z,
                vertex.Normal.x, vertex.Normal.y, vertex.Normal.z,
                vertex.TexCoord.x, vertex.TexCoord.y
            });
        }

        m_VertexBuffer = VertexBuffer::Create(
            data.data(),
            static_cast<std::uint32_t>(data.size() * sizeof(float)));

        m_IndexBuffer = IndexBuffer::Create(
            indices.data(),
            static_cast<std::uint32_t>(indices.size()));

        m_VertexArray = VertexArray::Create();
        m_VertexArray->SetVertexBuffer(
            m_VertexBuffer,
            BufferLayout{
                { ShaderDataType::Float3, "a_Position" },
                { ShaderDataType::Float3, "a_Normal" },
                { ShaderDataType::Float2, "a_TexCoord" }
            });
        m_VertexArray->SetIndexBuffer(m_IndexBuffer);
    }

    std::shared_ptr<Mesh> Mesh::CreateCube()
    {
        constexpr float h = 0.5f;
        const std::vector<MeshVertex> vertices =
        {
            // Front
            {{-h,-h, h},{ 0, 0, 1},{0,0}}, {{ h,-h, h},{ 0, 0, 1},{1,0}},
            {{ h, h, h},{ 0, 0, 1},{1,1}}, {{-h, h, h},{ 0, 0, 1},{0,1}},
            // Back
            {{ h,-h,-h},{ 0, 0,-1},{0,0}}, {{-h,-h,-h},{ 0, 0,-1},{1,0}},
            {{-h, h,-h},{ 0, 0,-1},{1,1}}, {{ h, h,-h},{ 0, 0,-1},{0,1}},
            // Left
            {{-h,-h,-h},{-1, 0, 0},{0,0}}, {{-h,-h, h},{-1, 0, 0},{1,0}},
            {{-h, h, h},{-1, 0, 0},{1,1}}, {{-h, h,-h},{-1, 0, 0},{0,1}},
            // Right
            {{ h,-h, h},{ 1, 0, 0},{0,0}}, {{ h,-h,-h},{ 1, 0, 0},{1,0}},
            {{ h, h,-h},{ 1, 0, 0},{1,1}}, {{ h, h, h},{ 1, 0, 0},{0,1}},
            // Top
            {{-h, h, h},{ 0, 1, 0},{0,0}}, {{ h, h, h},{ 0, 1, 0},{1,0}},
            {{ h, h,-h},{ 0, 1, 0},{1,1}}, {{-h, h,-h},{ 0, 1, 0},{0,1}},
            // Bottom
            {{-h,-h,-h},{ 0,-1, 0},{0,0}}, {{ h,-h,-h},{ 0,-1, 0},{1,0}},
            {{ h,-h, h},{ 0,-1, 0},{1,1}}, {{-h,-h, h},{ 0,-1, 0},{0,1}}
        };

        std::vector<std::uint32_t> indices;
        indices.reserve(36);
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            const std::uint32_t b = face * 4;
            indices.insert(indices.end(), { b, b+1, b+2, b+2, b+3, b });
        }

        return std::make_shared<Mesh>(vertices, indices);
    }
}
