#include "Engine/AI/Navigation/NavMeshDebugRenderer.h"

#include "Engine/AI/Navigation/NavMesh.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"

#include <limits>
#include <vector>

namespace NoJob
{
    namespace
    {
        constexpr const char* NavMeshDebugVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;

void main()
{
    gl_Position =
        u_ViewProjection *
        vec4(a_Position, 1.0);
}
)";

        constexpr const char* NavMeshDebugFragmentShader = R"(
#version 460 core

layout(location = 0) out vec4 FragColor;

uniform vec4 u_Color;

void main()
{
    FragColor = u_Color;
}
)";
    }

    NavMeshDebugRenderer::NavMeshDebugRenderer()
    {
        m_Shader =
            Shader::Create(
                NavMeshDebugVertexShader,
                NavMeshDebugFragmentShader);
    }

    void NavMeshDebugRenderer::Build(
        const NavMesh& navMesh)
    {
        Clear();

        std::vector<float> lineVertices;

        //
        // Each NavEdge becomes two position-only vertices:
        //
        // StartVertex ---- EndVertex
        //
        // GL_LINES interprets every pair as one independent line.
        //
        for (std::size_t polygonIndex = 0;
            polygonIndex < navMesh.GetPolygonCount();
            ++polygonIndex)
        {
            const NavPolygon* polygon =
                navMesh.GetPolygon(
                    static_cast<NavPolygonID>(
                        polygonIndex));

            if (!polygon)
            {
                continue;
            }

            for (const NavEdge& edge :
                polygon->Edges)
            {
                const NavVertex* start =
                    navMesh.GetVertex(
                        edge.StartVertex);

                const NavVertex* end =
                    navMesh.GetVertex(
                        edge.EndVertex);

                if (!start || !end)
                {
                    continue;
                }

                lineVertices.push_back(
                    start->Position.x);

                lineVertices.push_back(
                    start->Position.y);

                lineVertices.push_back(
                    start->Position.z);

                lineVertices.push_back(
                    end->Position.x);

                lineVertices.push_back(
                    end->Position.y);

                lineVertices.push_back(
                    end->Position.z);
            }
        }

        if (lineVertices.empty())
        {
            return;
        }

        const std::size_t vertexCount =
            lineVertices.size() / 3;

        if (vertexCount >
            std::numeric_limits<std::uint32_t>::max())
        {
            return;
        }

        const std::size_t byteSize =
            lineVertices.size() *
            sizeof(float);

        if (byteSize >
            std::numeric_limits<std::uint32_t>::max())
        {
            return;
        }

        m_VertexCount =
            static_cast<std::uint32_t>(
                vertexCount);

        m_VertexBuffer =
            VertexBuffer::Create(
                lineVertices.data(),
                static_cast<std::uint32_t>(
                    byteSize));

        m_VertexArray =
            VertexArray::Create();

        m_VertexArray->SetVertexBuffer(
            m_VertexBuffer);
    }

    void NavMeshDebugRenderer::Draw(
        const glm::mat4& viewProjection,
        const glm::vec4& color) const
    {
        if (!m_VertexArray ||
            !m_Shader ||
            m_VertexCount < 2)
        {
            return;
        }

        m_Shader->Bind();

        m_Shader->SetMat4(
            "u_ViewProjection",
            viewProjection);

        m_Shader->SetFloat4(
            "u_Color",
            color);

        RenderCommand::DrawLines(
            *m_VertexArray,
            m_VertexCount);
    }

    void NavMeshDebugRenderer::Clear()
    {
        m_VertexArray.reset();
        m_VertexBuffer.reset();
        m_VertexCount = 0;
    }
}