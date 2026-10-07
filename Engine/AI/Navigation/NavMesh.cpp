#include "Engine/AI/Navigation/NavMesh.h"

#include <unordered_set>

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
        for (NavPolygon& polygon : m_Polygons)
        {
            for (NavEdge& edge : polygon.Edges)
            {
                edge.NeighborPolygon = InvalidNavPolygonID;
            }
        }

        for (size_t polygonIndexA = 0;
            polygonIndexA < m_Polygons.size();
            ++polygonIndexA)
        {
            NavPolygon& polygonA = m_Polygons[polygonIndexA];

            for (size_t polygonIndexB = polygonIndexA + 1;
                polygonIndexB < m_Polygons.size();
                ++polygonIndexB)
            {
                NavPolygon& polygonB = m_Polygons[polygonIndexB];

                for (NavEdge& edgeA : polygonA.Edges)
                {
                    for (NavEdge& edgeB : polygonB.Edges)
                    {
                        if (edgeA.StartVertex == edgeB.EndVertex &&
                            edgeA.EndVertex == edgeB.StartVertex)
                        {
                            edgeA.NeighborPolygon =
                                static_cast<NavPolygonID>(polygonIndexB);

                            edgeB.NeighborPolygon =
                                static_cast<NavPolygonID>(polygonIndexA);
                        }
                    }
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