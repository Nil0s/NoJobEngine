#include "Engine/AI/Navigation/NavMeshPathfinder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace NoJob
{
    namespace
    {
        constexpr float Infinity =
            std::numeric_limits<float>::infinity();

        struct OpenNode
        {
            NavPolygonID Polygon =
                InvalidNavPolygonID;

            float FScore = Infinity;
        };

        struct OpenNodeCompare
        {
            bool operator()(
                const OpenNode& lhs,
                const OpenNode& rhs) const
            {
                return lhs.FScore > rhs.FScore;
            }
        };
    }

    NavPath NavMeshPathfinder::FindPath(
        const NavMesh& navMesh,
        NavPolygonID startPolygon,
        NavPolygonID goalPolygon)
    {
        NavPath emptyPath;

        const NavPolygon* start =
            navMesh.GetPolygon(startPolygon);

        const NavPolygon* goal =
            navMesh.GetPolygon(goalPolygon);

        if (!start || !goal)
            return emptyPath;

        if (startPolygon == goalPolygon)
        {
            NavPath path;
            path.Polygons.push_back(startPolygon);
            return path;
        }

        const std::size_t polygonCount =
            navMesh.GetPolygonCount();

        std::vector<float> gScore(
            polygonCount,
            Infinity);

        std::vector<NavPolygonID> cameFrom(
            polygonCount,
            InvalidNavPolygonID);

        std::vector<bool> closed(
            polygonCount,
            false);

        std::priority_queue<
            OpenNode,
            std::vector<OpenNode>,
            OpenNodeCompare>
            openSet;

        gScore[startPolygon] = 0.0f;

        openSet.push(
            {
                startPolygon,
                Heuristic(
                    navMesh,
                    startPolygon,
                    goalPolygon)
            });

        while (!openSet.empty())
        {
            const OpenNode currentNode =
                openSet.top();

            openSet.pop();

            const NavPolygonID currentID =
                currentNode.Polygon;

            if (currentID >= polygonCount)
                continue;

            if (closed[currentID])
                continue;

            if (currentID == goalPolygon)
            {
                return ReconstructPath(
                    startPolygon,
                    goalPolygon,
                    cameFrom);
            }

            closed[currentID] = true;

            const NavPolygon* currentPolygon =
                navMesh.GetPolygon(currentID);

            if (!currentPolygon)
                continue;

            for (const NavEdge& edge :
                currentPolygon->Edges)
            {
                if (!edge.Traversable)
                    continue;

                const NavPolygonID neighborID =
                    edge.NeighborPolygon;

                if (neighborID ==
                    InvalidNavPolygonID)
                {
                    continue;
                }

                if (neighborID >= polygonCount)
                    continue;

                if (closed[neighborID])
                    continue;

                const NavPolygon* neighbor =
                    navMesh.GetPolygon(
                        neighborID);

                if (!neighbor)
                    continue;

                const float transitionCost =
                    CalculateTransitionCost(
                        navMesh,
                        currentID,
                        neighborID,
                        edge);

                if (!std::isfinite(
                    transitionCost))
                {
                    continue;
                }

                const float tentativeGScore =
                    gScore[currentID] +
                    transitionCost;

                if (tentativeGScore >=
                    gScore[neighborID])
                {
                    continue;
                }

                cameFrom[neighborID] =
                    currentID;

                gScore[neighborID] =
                    tentativeGScore;

                const float fScore =
                    tentativeGScore +
                    Heuristic(
                        navMesh,
                        neighborID,
                        goalPolygon);

                openSet.push(
                    {
                        neighborID,
                        fScore
                    });
            }
        }

        return emptyPath;
    }

    float NavMeshPathfinder::Heuristic(
        const NavMesh& navMesh,
        NavPolygonID from,
        NavPolygonID to)
    {
        const NavPolygon* fromPolygon =
            navMesh.GetPolygon(from);

        const NavPolygon* toPolygon =
            navMesh.GetPolygon(to);

        if (!fromPolygon ||
            !toPolygon)
        {
            return 0.0f;
        }

        return glm::distance(
            fromPolygon->Center,
            toPolygon->Center);
    }

    float NavMeshPathfinder::
        CalculateTransitionCost(
            const NavMesh& navMesh,
            NavPolygonID from,
            NavPolygonID to,
            const NavEdge& edge)
    {
        if (!edge.Traversable)
            return Infinity;

        const NavPolygon* fromPolygon =
            navMesh.GetPolygon(from);

        const NavPolygon* toPolygon =
            navMesh.GetPolygon(to);

        if (!fromPolygon ||
            !toPolygon)
        {
            return Infinity;
        }

        if (edge.Cost < 0.0f ||
            toPolygon->Cost < 0.0f)
        {
            return Infinity;
        }

        const float movementCost =
            glm::distance(
                fromPolygon->Center,
                toPolygon->Center);

        return
            movementCost +
            edge.Cost +
            toPolygon->Cost;
    }

    NavPath NavMeshPathfinder::ReconstructPath(
        NavPolygonID startPolygon,
        NavPolygonID goalPolygon,
        const std::vector<NavPolygonID>& cameFrom)
    {
        NavPath path;

        NavPolygonID current =
            goalPolygon;

        while (current !=
            InvalidNavPolygonID)
        {
            path.Polygons.push_back(
                current);

            if (current ==
                startPolygon)
            {
                break;
            }

            if (current >=
                cameFrom.size())
            {
                path.Clear();
                return path;
            }

            current =
                cameFrom[current];
        }

        if (path.Polygons.empty() ||
            path.Polygons.back() !=
            startPolygon)
        {
            path.Clear();
            return path;
        }

        std::reverse(
            path.Polygons.begin(),
            path.Polygons.end());

        return path;
    }
}