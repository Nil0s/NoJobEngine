#include "Engine/AI/Navigation/NavigationSystem.h"

#include "Engine/AI/Navigation/NavMeshGeometryExtractor.h"
#include "Engine/AI/Navigation/NavMeshVoxelizer.h"
#include "Engine/AI/Navigation/NavMeshPathfinder.h"
#include "Engine/Scene/Scene.h"

#include <cmath>
#include <utility>

namespace NoJob
{
    namespace
    {
        constexpr float PointInsideEpsilon =
            0.00001f;
    }

    bool NavigationSystem::Bake(Scene& scene)
    {
        NavMeshSourceGeometry geometry = NavMeshVoxelizer::Rasterize(
            scene, NavMeshGeometryExtractor::Extract(scene),
            0.35f, 0.75f, 1.8f, m_GenerationSettings.MaxSlopeAngle);

        if (geometry.Vertices.empty() ||
            geometry.Indices.size() < 3)
        {
            Clear();
            return false;
        }

        NavMeshGenerationSettings voxelSettings = m_GenerationSettings;
        voxelSettings.MergePolygons = false;
        voxelSettings.VertexWeldTolerance = 0.0f;
        NavMesh generatedNavMesh = NavMeshGenerator::Generate(geometry, voxelSettings);

        if (generatedNavMesh.GetPolygonCount() == 0)
        {
            Clear();
            return false;
        }

        m_NavMesh =
            std::move(generatedNavMesh);

        m_HasNavMesh = true;
        ++m_NavMeshVersion;
        return true;
    }

    void NavigationSystem::Clear()
    {
        m_NavMesh = NavMesh{};
        m_HasNavMesh = false;
        ++m_NavMeshVersion;
    }

    NavPolygonID NavigationSystem::FindPolygon(
        const glm::vec3& position) const
    {
        if (!m_HasNavMesh)
            return InvalidNavPolygonID;

        for (std::size_t polygonIndex = 0;
            polygonIndex < m_NavMesh.GetPolygonCount();
            ++polygonIndex)
        {
            const NavPolygonID polygonID =
                static_cast<NavPolygonID>(
                    polygonIndex);

            const NavPolygon* polygon =
                m_NavMesh.GetPolygon(
                    polygonID);

            if (!polygon)
                continue;

            if (IsPointInsidePolygonXZ(
                m_NavMesh,
                *polygon,
                position))
            {
                return polygonID;
            }
        }

        return InvalidNavPolygonID;
    }

    NavPointPath NavigationSystem::CalculatePath(
        const glm::vec3& startPosition,
        const glm::vec3& destination) const
    {
        NavPointPath emptyPath;

        if (!m_HasNavMesh)
            return emptyPath;

        const NavPolygonID startPolygon =
            FindPolygon(startPosition);

        const NavPolygonID goalPolygon =
            FindPolygon(destination);

        if (startPolygon ==
            InvalidNavPolygonID ||
            goalPolygon ==
            InvalidNavPolygonID)
        {
            return emptyPath;
        }

        const NavPath corridor =
            NavMeshPathfinder::FindPath(
                m_NavMesh,
                startPolygon,
                goalPolygon);

        if (!corridor.IsValid())
            return emptyPath;

        return NavMeshFunnel::BuildPath(
            m_NavMesh,
            corridor,
            startPosition,
            destination);
    }

    bool NavigationSystem::IsPointInsidePolygonXZ(
        const NavMesh& navMesh,
        const NavPolygon& polygon,
        const glm::vec3& position)
    {
        if (polygon.Vertices.size() < 3)
            return false;

        float referenceSign = 0.0f;

        for (std::size_t i = 0;
            i < polygon.Vertices.size();
            ++i)
        {
            const NavVertexID startID =
                polygon.Vertices[i];

            const NavVertexID endID =
                polygon.Vertices[
                    (i + 1) %
                        polygon.Vertices.size()];

            const NavVertex* start =
                navMesh.GetVertex(startID);

            const NavVertex* end =
                navMesh.GetVertex(endID);

            if (!start || !end)
                return false;

            const float edgeX =
                end->Position.x -
                start->Position.x;

            const float edgeZ =
                end->Position.z -
                start->Position.z;

            const float pointX =
                position.x -
                start->Position.x;

            const float pointZ =
                position.z -
                start->Position.z;

            const float cross =
                edgeX * pointZ -
                edgeZ * pointX;

            if (std::abs(cross) <=
                PointInsideEpsilon)
            {
                continue;
            }

            if (referenceSign == 0.0f)
            {
                referenceSign = cross;
                continue;
            }

            if ((referenceSign > 0.0f &&
                cross < -PointInsideEpsilon) ||
                (referenceSign < 0.0f &&
                    cross > PointInsideEpsilon))
            {
                return false;
            }
        }

        return true;
    }
}