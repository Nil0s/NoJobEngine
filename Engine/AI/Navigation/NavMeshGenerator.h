#pragma once

#include "Engine/AI/Navigation/NavMesh.h"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace NoJob
{
    struct NavMeshGenerationSettings
    {
        float MaxSlopeAngle = 45.0f;

        // Maximum world-space distance at which two source vertices
        // are considered the same navigation vertex.
        float VertexWeldTolerance = 0.001f;

        // Maximum distance a vertex may have from the polygon plane
        // for polygons to be merged.
        float MergePlanarityTolerance = 0.001f;
    };

    struct NavMeshSourceGeometry
    {
        std::vector<glm::vec3> Vertices;
        std::vector<uint32_t> Indices;
    };

    class NavMeshGenerator
    {
    public:
        static NavMesh Generate(
            const NavMeshSourceGeometry& geometry,
            const NavMeshGenerationSettings& settings = {});

    private:
        static bool IsPolygonPlanar(
            const NavMesh& navMesh,
            const std::vector<NavVertexID>& vertices,
            float tolerance);

        static bool IsPolygonConvex(
            const NavMesh& navMesh,
            const std::vector<NavVertexID>& vertices);

        static bool BuildMergedPolygon(
            const NavPolygon& polygonA,
            const NavPolygon& polygonB,
            std::vector<NavVertexID>& mergedVertices);

        static void MergeConvexPolygons(
            NavMesh& navMesh,
            float planarityTolerance);
    };
}