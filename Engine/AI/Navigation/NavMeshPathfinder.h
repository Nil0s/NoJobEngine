#pragma once

#include "Engine/AI/Navigation/NavMesh.h"

#include <vector>

#include <glm/glm.hpp>

namespace NoJob
{
    struct NavPath
    {
        std::vector<NavPolygonID> Polygons;

        bool IsValid() const
        {
            return !Polygons.empty();
        }

        void Clear()
        {
            Polygons.clear();
        }
    };

    class NavMeshPathfinder
    {
    public:
        static NavPath FindPath(
            const NavMesh& navMesh,
            NavPolygonID startPolygon,
            NavPolygonID goalPolygon);

    private:
        static float Heuristic(
            const NavMesh& navMesh,
            NavPolygonID from,
            NavPolygonID to);

        static float CalculateTransitionCost(
            const NavMesh& navMesh,
            NavPolygonID from,
            NavPolygonID to,
            const NavEdge& edge);

        static NavPath ReconstructPath(
            NavPolygonID startPolygon,
            NavPolygonID goalPolygon,
            const std::vector<NavPolygonID>& cameFrom);
    };
}