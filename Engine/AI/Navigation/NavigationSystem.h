#pragma once

#include "Engine/AI/Navigation/NavMesh.h"
#include "Engine/AI/Navigation/NavMeshFunnel.h"
#include "Engine/AI/Navigation/NavMeshGenerator.h"

#include <glm/glm.hpp>

namespace NoJob
{
    class Scene;

    class NavigationSystem
    {
    public:
        bool Bake(Scene& scene);

        void Clear();

        NavPolygonID FindPolygon(
            const glm::vec3& position) const;

        NavPointPath CalculatePath(
            const glm::vec3& startPosition,
            const glm::vec3& destination) const;

        bool HasNavMesh() const
        {
            return m_HasNavMesh;
        }

        const NavMesh& GetNavMesh() const
        {
            return m_NavMesh;
        }

        NavMeshGenerationSettings& GetGenerationSettings()
        {
            return m_GenerationSettings;
        }

        const NavMeshGenerationSettings&
            GetGenerationSettings() const
        {
            return m_GenerationSettings;
        }

    private:
        static bool IsPointInsidePolygonXZ(
            const NavMesh& navMesh,
            const NavPolygon& polygon,
            const glm::vec3& position);

    private:
        NavMesh m_NavMesh;
        NavMeshGenerationSettings m_GenerationSettings;

        bool m_HasNavMesh = false;
    };
}