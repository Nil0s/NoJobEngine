#pragma once

#include "Engine/AI/Navigation/NavMesh.h"
#include "Engine/AI/Navigation/NavMeshGenerator.h"

namespace NoJob
{
    class Scene;

    class NavigationSystem
    {
    public:
        bool Bake(Scene& scene);

        void Clear();

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

        const NavMeshGenerationSettings& GetGenerationSettings() const
        {
            return m_GenerationSettings;
        }

    private:
        NavMesh m_NavMesh;
        NavMeshGenerationSettings m_GenerationSettings;

        bool m_HasNavMesh = false;
    };
}