#include "Engine/AI/Navigation/NavMeshFunnel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace NoJob
{
    namespace
    {
        constexpr float FunnelEpsilon = 0.00001f;
    }

    NavPointPath NavMeshFunnel::BuildPath(
        const NavMesh& navMesh,
        const NavPath& corridor,
        const glm::vec3& startPosition,
        const glm::vec3& goalPosition)
    {
        NavPointPath result;

        if (!corridor.IsValid())
            return result;

        if (corridor.Polygons.size() == 1)
        {
            result.Points.push_back(startPosition);

            if (!NearlyEqual(
                startPosition,
                goalPosition))
            {
                result.Points.push_back(
                    goalPosition);
            }

            return result;
        }

        std::vector<Portal> portals;

        if (!BuildPortals(
            navMesh,
            corridor,
            startPosition,
            goalPosition,
            portals))
        {
            return result;
        }

        if (portals.size() < 2)
            return result;

        result.Points.push_back(
            startPosition);

        glm::vec3 portalApex =
            portals[0].Left;

        glm::vec3 portalLeft =
            portals[0].Left;

        glm::vec3 portalRight =
            portals[0].Right;

        std::size_t apexIndex = 0;
        std::size_t leftIndex = 0;
        std::size_t rightIndex = 0;

        for (std::size_t i = 1;
            i < portals.size();
            ++i)
        {
            const glm::vec3 newLeft =
                portals[i].Left;

            const glm::vec3 newRight =
                portals[i].Right;

            // --------------------------------------------------------
            // Tighten the right side of the funnel.
            // --------------------------------------------------------

            if (SignedAreaXZ(
                portalApex,
                portalRight,
                newRight) <= 0.0f)
            {
                if (NearlyEqual(
                    portalApex,
                    portalRight) ||
                    SignedAreaXZ(
                        portalApex,
                        portalLeft,
                        newRight) > 0.0f)
                {
                    portalRight =
                        newRight;

                    rightIndex = i;
                }
                else
                {
                    if (result.Points.empty() ||
                        !NearlyEqual(
                            result.Points.back(),
                            portalLeft))
                    {
                        result.Points.push_back(
                            portalLeft);
                    }

                    portalApex =
                        portalLeft;

                    apexIndex =
                        leftIndex;

                    portalLeft =
                        portalApex;

                    portalRight =
                        portalApex;

                    leftIndex =
                        apexIndex;

                    rightIndex =
                        apexIndex;

                    i = apexIndex;
                    continue;
                }
            }

            // --------------------------------------------------------
            // Tighten the left side of the funnel.
            // --------------------------------------------------------

            if (SignedAreaXZ(
                portalApex,
                portalLeft,
                newLeft) >= 0.0f)
            {
                if (NearlyEqual(
                    portalApex,
                    portalLeft) ||
                    SignedAreaXZ(
                        portalApex,
                        portalRight,
                        newLeft) < 0.0f)
                {
                    portalLeft =
                        newLeft;

                    leftIndex = i;
                }
                else
                {
                    if (result.Points.empty() ||
                        !NearlyEqual(
                            result.Points.back(),
                            portalRight))
                    {
                        result.Points.push_back(
                            portalRight);
                    }

                    portalApex =
                        portalRight;

                    apexIndex =
                        rightIndex;

                    portalLeft =
                        portalApex;

                    portalRight =
                        portalApex;

                    leftIndex =
                        apexIndex;

                    rightIndex =
                        apexIndex;

                    i = apexIndex;
                    continue;
                }
            }
        }

        if (result.Points.empty() ||
            !NearlyEqual(
                result.Points.back(),
                goalPosition))
        {
            result.Points.push_back(
                goalPosition);
        }

        return result;
    }

    bool NavMeshFunnel::BuildPortals(
        const NavMesh& navMesh,
        const NavPath& corridor,
        const glm::vec3& startPosition,
        const glm::vec3& goalPosition,
        std::vector<Portal>& portals)
    {
        portals.clear();

        if (corridor.Polygons.empty())
            return false;

        // Degenerate start portal.
        portals.push_back(
            {
                startPosition,
                startPosition
            });

        for (std::size_t i = 0;
            i + 1 < corridor.Polygons.size();
            ++i)
        {
            Portal portal;

            if (!FindSharedPortal(
                navMesh,
                corridor.Polygons[i],
                corridor.Polygons[i + 1],
                portal))
            {
                portals.clear();
                return false;
            }

            // Keep waypoints away from the exact portal vertices.
            // This reduces corner grazing on voxelized NavMeshes.
            // Never invert a narrow portal.
            glm::vec3 portalWidth = portal.Right - portal.Left;
            portalWidth.y = 0.0f;
            const float width = glm::length(portalWidth);
            if (width > 0.0001f)
            {
                constexpr float desiredInset = 0.12f;
                const float inset = std::min(desiredInset, width * 0.20f);
                const glm::vec3 along = (portal.Right - portal.Left) / width;
                portal.Left += along * inset;
                portal.Right -= along * inset;
            }

            portals.push_back(
                portal);
        }

        // Degenerate goal portal.
        portals.push_back(
            {
                goalPosition,
                goalPosition
            });

        return true;
    }

    bool NavMeshFunnel::FindSharedPortal(
        const NavMesh& navMesh,
        NavPolygonID fromPolygon,
        NavPolygonID toPolygon,
        Portal& portal)
    {
        const NavPolygon* from =
            navMesh.GetPolygon(
                fromPolygon);

        const NavPolygon* to =
            navMesh.GetPolygon(
                toPolygon);

        if (!from || !to)
            return false;

        for (const NavEdge& edge :
            from->Edges)
        {
            if (!edge.Traversable)
                continue;

            if (edge.NeighborPolygon !=
                toPolygon)
            {
                continue;
            }

            const NavVertex* start =
                navMesh.GetVertex(
                    edge.StartVertex);

            const NavVertex* end =
                navMesh.GetVertex(
                    edge.EndVertex);

            if (!start || !end)
                return false;

            /*
             * Our polygons use consistent winding and adjacency
             * stores the shared edge from the perspective of the
             * current polygon.
             *
             * With the current navigation winding, traversing from
             * one polygon to its neighbour means:
             *
             *     EndVertex   -> left side
             *     StartVertex -> right side
             */
            portal.Left =
                end->Position;

            portal.Right =
                start->Position;

            return true;
        }

        return false;
    }

    float NavMeshFunnel::SignedAreaXZ(
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c)
    {
        const float abX =
            b.x - a.x;

        const float abZ =
            b.z - a.z;

        const float acX =
            c.x - a.x;

        const float acZ =
            c.z - a.z;

        return
            abX * acZ -
            abZ * acX;
    }

    bool NavMeshFunnel::NearlyEqual(
        const glm::vec3& a,
        const glm::vec3& b)
    {
        const glm::vec3 delta =
            a - b;

        return
            glm::dot(delta, delta) <=
            FunnelEpsilon *
            FunnelEpsilon;
    }
}