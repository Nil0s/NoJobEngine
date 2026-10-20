#pragma once

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

namespace NoJob
{
    class NavMesh;
    class Shader;
    class VertexArray;
    class VertexBuffer;

    class NavMeshDebugRenderer
    {
    public:
        NavMeshDebugRenderer();

        void Build(const NavMesh& navMesh);

        void Draw(
            const glm::mat4& viewProjection,
            const glm::vec4& color =
            glm::vec4(
                0.15f,
                0.85f,
                1.0f,
                1.0f)) const;

        void Clear();

        std::uint32_t GetVertexCount() const
        {
            return m_VertexCount;
        }

    private:
        std::shared_ptr<VertexArray> m_VertexArray;
        std::shared_ptr<VertexBuffer> m_VertexBuffer;
        std::shared_ptr<Shader> m_Shader;

        std::uint32_t m_VertexCount = 0;
    };
}