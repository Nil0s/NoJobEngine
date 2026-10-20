#include "Engine/AI/Navigation/NavMesh.h"

#include <unordered_set>
#include <unordered_map>

namespace NoJob
{
    NavVertexID NavMesh::AddVertex(const glm::vec3& position)
    {
        if (m_Vertices.size() >= InvalidNavVertexID)
        {
            return InvalidNavVertexID;
        }

        const NavVertexID id =
            static_cast<NavVertexID>(m_Vertices.size());

        m_Vertices.push_back({ position });

        return id;
    }

    NavPolygonID NavMesh::AddPolygon(
        const std::vector<NavVertexID>& vertices,
        float cost)
    {
        // A polygon needs at least three vertices.
        if (vertices.size() < 3)
        {
            return InvalidNavPolygonID;
        }

        // Negative traversal penalties are not supported.
        // Cost 0 means that the polygon adds no terrain penalty.
        if (cost < 0.0f)
        {
            return InvalidNavPolygonID;
        }

        // The polygon ID is its position inside m_Polygons.
        // Keep the maximum uint32_t value reserved as the invalid sentinel.
        if (m_Polygons.size() >= InvalidNavPolygonID)
        {
            return InvalidNavPolygonID;
        }

        // Every referenced vertex must already exist and each vertex
        // can only appear once in the polygon.
        std::unordered_set<NavVertexID> uniqueVertices;

        for (NavVertexID vertexID : vertices)
        {
            if (vertexID >= m_Vertices.size())
            {
                return InvalidNavPolygonID;
            }

            if (!uniqueVertices.insert(vertexID).second)
            {
                return InvalidNavPolygonID;
            }
        }

        NavPolygon polygon;
        polygon.Vertices = vertices;
        polygon.Cost = cost;

        polygon.Edges.reserve(vertices.size());

        glm::vec3 center{ 0.0f };

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const NavVertexID startVertex = vertices[i];
            const NavVertexID endVertex =
                vertices[(i + 1) % vertices.size()];

            NavEdge edge;
            edge.StartVertex = startVertex;
            edge.EndVertex = endVertex;

            polygon.Edges.push_back(edge);

            center += m_Vertices[startVertex].Position;
        }

        polygon.Center =
            center / static_cast<float>(vertices.size());

        const NavPolygonID id =
            static_cast<NavPolygonID>(m_Polygons.size());

        m_Polygons.push_back(std::move(polygon));

        return id;
    }

    void NavMesh::BuildAdjacency()
    {
        // O(E) edge lookup instead of comparing every polygon pair.
        std::unordered_map<std::uint64_t, std::pair<NavPolygonID, size_t>> edges;
        for (NavPolygonID id = 0; id < m_Polygons.size(); ++id)
        {
            auto& polygon = m_Polygons[id];
            for (size_t i = 0; i < polygon.Edges.size(); ++i)
            {
                auto& edge = polygon.Edges[i];
                edge.NeighborPolygon = InvalidNavPolygonID;
                const std::uint64_t reverse =
                    (std::uint64_t(edge.EndVertex) << 32) | edge.StartVertex;
                auto it = edges.find(reverse);
                if (it != edges.end())
                {
                    edge.NeighborPolygon = it->second.first;
                    m_Polygons[it->second.first].Edges[it->second.second].NeighborPolygon = id;
                }
                else
                {
                    const std::uint64_t key =
                        (std::uint64_t(edge.StartVertex) << 32) | edge.EndVertex;
                    edges.emplace(key, std::make_pair(id, i));
                }
            }
        }
    }

    const NavVertex* NavMesh::GetVertex(NavVertexID id) const
    {
        if (id >= m_Vertices.size())
        {
            return nullptr;
        }

        return &m_Vertices[id];
    }

    const NavPolygon* NavMesh::GetPolygon(NavPolygonID id) const
    {
        if (id >= m_Polygons.size())
        {
            return nullptr;
        }

        return &m_Polygons[id];
    }
    void NavMesh::ReplacePolygons(std::vector<NavPolygon> polygons)
    {
        m_Polygons = std::move(polygons);
        BuildAdjacency();
    }
}