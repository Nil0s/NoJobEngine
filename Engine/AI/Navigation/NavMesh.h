#pragma once

#include <cstdint>
#include <limits>
#include <vector>
#include <utility>

#include <glm/glm.hpp>

namespace NoJob
{
    using NavVertexID = uint32_t;
    using NavPolygonID = uint32_t;

    constexpr NavVertexID InvalidNavVertexID =
        std::numeric_limits<NavVertexID>::max();

    constexpr NavPolygonID InvalidNavPolygonID =
        std::numeric_limits<NavPolygonID>::max();

    struct NavVertex
    {
        glm::vec3 Position{ 0.0f };
    };

    struct NavEdge
    {
        NavVertexID StartVertex = InvalidNavVertexID;
        NavVertexID EndVertex = InvalidNavVertexID;

        NavPolygonID NeighborPolygon = InvalidNavPolygonID;

        bool Traversable = true;
        float Cost = 1.0f;
    };

    struct NavPolygon
    {
        std::vector<NavVertexID> Vertices;
        std::vector<NavEdge> Edges;

        glm::vec3 Center{ 0.0f };

        float Cost = 1.0f;
    };

    class NavMesh
    {
    public:
        NavVertexID AddVertex(const glm::vec3& position);

        NavPolygonID AddPolygon(
            const std::vector<NavVertexID>& vertices,
            float cost = 1.0f);

        void BuildAdjacency();

        void ReplacePolygons(std::vector<NavPolygon> polygons);

        const NavVertex* GetVertex(NavVertexID id) const;
        const NavPolygon* GetPolygon(NavPolygonID id) const;

        size_t GetVertexCount() const { return m_Vertices.size(); }
        size_t GetPolygonCount() const { return m_Polygons.size(); }

    private:
        std::vector<NavVertex> m_Vertices;
        std::vector<NavPolygon> m_Polygons;
    };
}