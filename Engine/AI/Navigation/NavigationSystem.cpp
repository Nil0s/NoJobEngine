#include "Engine/AI/Navigation/NavigationSystem.h"

#include "Engine/AI/Navigation/NavMeshGeometryExtractor.h"
#include "Engine/Scene/Scene.h"
#include <utility>

namespace NoJob
{
    bool NavigationSystem::Bake(Scene& scene)
    {
        NavMeshSourceGeometry geometry =
            NavMeshGeometryExtractor::Extract(scene);

        if (geometry.Vertices.empty() ||
            geometry.Indices.size() < 3)
        {
            Clear();
            return false;
        }

        NavMesh generatedNavMesh =
            NavMeshGenerator::Generate(
                geometry,
                m_GenerationSettings);

        if (generatedNavMesh.GetPolygonCount() == 0)
        {
            Clear();
            return false;
        }

        m_NavMesh = std::move(generatedNavMesh);
        m_HasNavMesh = true;

        return true;
    }

    void NavigationSystem::Clear()
    {
        m_NavMesh = NavMesh{};
        m_HasNavMesh = false;
    }
}