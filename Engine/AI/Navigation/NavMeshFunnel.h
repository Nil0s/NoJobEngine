#pragma once

#include "Engine/AI/Navigation/NavMesh.h"
#include "Engine/AI/Navigation/NavMeshPathfinder.h"

#include <vector>

#include <glm/glm.hpp>

namespace NoJob
{
    struct NavPointPath
    {
        std::vector<glm::vec3> Points;

        bool IsValid() const
        {
            return !Points.empty();
        }

        void Clear()
        {
            Points.clear();
        }
    };

    class NavMeshFunnel
    {
    public:
        static NavPointPath BuildPath(
            const NavMesh& navMesh,
            const NavPath& corridor,
            const glm::vec3& startPosition,
            const glm::vec3& goalPosition);

    private:
        struct Portal
        {
            glm::vec3 Left{ 0.0f };
            glm::vec3 Right{ 0.0f };
        };

        static bool BuildPortals(
            const NavMesh& navMesh,
            const NavPath& corridor,
            const glm::vec3& startPosition,
            const glm::vec3& goalPosition,
            std::vector<Portal>& portals);

        static bool FindSharedPortal(
            const NavMesh& navMesh,
            NavPolygonID fromPolygon,
            NavPolygonID toPolygon,
            Portal& portal);

        static float SignedAreaXZ(
            const glm::vec3& a,
            const glm::vec3& b,
            const glm::vec3& c);

        static bool NearlyEqual(
            const glm::vec3& a,
            const glm::vec3& b);
    };
}