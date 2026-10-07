#include "Engine/AI/Navigation/NavMeshGenerator.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float GeometryEpsilon = 1e-6f;
}

namespace NoJob
{
    NavMesh NavMeshGenerator::Generate(
        const NavMeshSourceGeometry& geometry,
        const NavMeshGenerationSettings& settings)
    {
        NavMesh navMesh;

        std::vector<NavVertexID> vertexMap(
            geometry.Vertices.size(),
            InvalidNavVertexID);

        const float weldTolerance =
            std::max(
                settings.VertexWeldTolerance,
                0.0f);

        const float weldToleranceSquared =
            weldTolerance * weldTolerance;

        auto getOrCreateNavVertex =
            [&](uint32_t sourceIndex) -> NavVertexID
            {
                NavVertexID& mappedVertex =
                    vertexMap[sourceIndex];

                // This exact source vertex was already processed.
                if (mappedVertex != InvalidNavVertexID)
                {
                    return mappedVertex;
                }

                const glm::vec3& sourcePosition =
                    geometry.Vertices[sourceIndex];

                //
                // Different source indices may represent the same
                // physical point. This happens with UV/normal seams,
                // separate meshes and adjacent scene entities.
                //
                for (NavVertexID candidateID = 0;
                    candidateID < navMesh.GetVertexCount();
                    ++candidateID)
                {
                    const NavVertex* candidate =
                        navMesh.GetVertex(candidateID);

                    if (!candidate)
                    {
                        continue;
                    }

                    const glm::vec3 difference =
                        candidate->Position -
                        sourcePosition;

                    const float distanceSquared =
                        glm::dot(
                            difference,
                            difference);

                    if (distanceSquared <=
                        weldToleranceSquared)
                    {
                        mappedVertex = candidateID;
                        return mappedVertex;
                    }
                }

                mappedVertex =
                    navMesh.AddVertex(sourcePosition);

                return mappedVertex;
            };

        for (size_t i = 0;
            i + 2 < geometry.Indices.size();
            i += 3)
        {
            const uint32_t indexA =
                geometry.Indices[i];

            const uint32_t indexB =
                geometry.Indices[i + 1];

            const uint32_t indexC =
                geometry.Indices[i + 2];

            if (indexA >= geometry.Vertices.size() ||
                indexB >= geometry.Vertices.size() ||
                indexC >= geometry.Vertices.size())
            {
                continue;
            }

            const glm::vec3& a =
                geometry.Vertices[indexA];

            const glm::vec3& b =
                geometry.Vertices[indexB];

            const glm::vec3& c =
                geometry.Vertices[indexC];

            const glm::vec3 ab = b - a;
            const glm::vec3 ac = c - a;

            const glm::vec3 crossProduct =
                glm::cross(ab, ac);

            const float crossLength =
                glm::length(crossProduct);

            // Reject degenerate or almost-degenerate triangles.
            if (crossLength <= GeometryEpsilon)
            {
                continue;
            }

            const glm::vec3 normal =
                crossProduct / crossLength;

            const float upDot =
                glm::clamp(
                    glm::dot(
                        normal,
                        glm::vec3(0.0f, 1.0f, 0.0f)),
                    -1.0f,
                    1.0f);

            const float slopeAngle =
                glm::degrees(std::acos(upDot));

            if (slopeAngle > settings.MaxSlopeAngle)
            {
                continue;
            }

            const NavVertexID navA =
                getOrCreateNavVertex(indexA);

            const NavVertexID navB =
                getOrCreateNavVertex(indexB);

            const NavVertexID navC =
                getOrCreateNavVertex(indexC);

            if (navA == InvalidNavVertexID ||
                navB == InvalidNavVertexID ||
                navC == InvalidNavVertexID)
            {
                continue;
            }

            navMesh.AddPolygon(
                { navA, navB, navC });
        }

        navMesh.BuildAdjacency();

        MergeConvexPolygons(
            navMesh,
            std::max(
                settings.MergePlanarityTolerance,
                0.0f));

        return navMesh;
    }

    bool NavMeshGenerator::IsPolygonPlanar(
        const NavMesh& navMesh,
        const std::vector<NavVertexID>& vertices,
        float tolerance)
    {
        if (vertices.size() < 3)
        {
            return false;
        }

        const NavVertex* first =
            navMesh.GetVertex(vertices[0]);

        if (!first)
        {
            return false;
        }

        const glm::vec3 planePoint =
            first->Position;

        glm::vec3 planeNormal{ 0.0f };

        //
        // Find the first non-degenerate triangle in the polygon.
        // Its normal defines the reference plane.
        //
        for (size_t i = 1;
            i + 1 < vertices.size();
            ++i)
        {
            const NavVertex* second =
                navMesh.GetVertex(vertices[i]);

            const NavVertex* third =
                navMesh.GetVertex(vertices[i + 1]);

            if (!second || !third)
            {
                return false;
            }

            const glm::vec3 edgeA =
                second->Position -
                planePoint;

            const glm::vec3 edgeB =
                third->Position -
                planePoint;

            const glm::vec3 crossProduct =
                glm::cross(edgeA, edgeB);

            const float normalLength =
                glm::length(crossProduct);

            if (normalLength > GeometryEpsilon)
            {
                planeNormal =
                    crossProduct / normalLength;

                break;
            }
        }

        if (glm::length(planeNormal) <=
            GeometryEpsilon)
        {
            return false;
        }

        tolerance =
            std::max(tolerance, 0.0f);

        for (NavVertexID vertexID : vertices)
        {
            const NavVertex* vertex =
                navMesh.GetVertex(vertexID);

            if (!vertex)
            {
                return false;
            }

            const float distanceToPlane =
                std::abs(
                    glm::dot(
                        vertex->Position -
                        planePoint,
                        planeNormal));

            if (distanceToPlane > tolerance)
            {
                return false;
            }
        }

        return true;
    }

    bool NavMeshGenerator::IsPolygonConvex(
        const NavMesh& navMesh,
        const std::vector<NavVertexID>& vertices)
    {
        if (vertices.size() < 3)
        {
            return false;
        }

        //
        // First calculate a usable normal for the polygon.
        //
        glm::vec3 polygonNormal{ 0.0f };

        for (size_t i = 0;
            i < vertices.size();
            ++i)
        {
            const NavVertex* current =
                navMesh.GetVertex(vertices[i]);

            const NavVertex* next =
                navMesh.GetVertex(
                    vertices[(i + 1) %
                    vertices.size()]);

            if (!current || !next)
            {
                return false;
            }

            //
            // Newell-style accumulation.
            // This works for polygons that are not necessarily
            // aligned with the XZ plane.
            //
            polygonNormal.x +=
                (current->Position.y -
                    next->Position.y) *
                (current->Position.z +
                    next->Position.z);

            polygonNormal.y +=
                (current->Position.z -
                    next->Position.z) *
                (current->Position.x +
                    next->Position.x);

            polygonNormal.z +=
                (current->Position.x -
                    next->Position.x) *
                (current->Position.y +
                    next->Position.y);
        }

        const float normalLength =
            glm::length(polygonNormal);

        if (normalLength <= GeometryEpsilon)
        {
            return false;
        }

        polygonNormal /= normalLength;

        float referenceSign = 0.0f;

        for (size_t i = 0;
            i < vertices.size();
            ++i)
        {
            const NavVertexID previousID =
                vertices[
                    (i + vertices.size() - 1) %
                        vertices.size()];

            const NavVertexID currentID =
                vertices[i];

            const NavVertexID nextID =
                vertices[
                    (i + 1) %
                        vertices.size()];

            const NavVertex* previous =
                navMesh.GetVertex(previousID);

            const NavVertex* current =
                navMesh.GetVertex(currentID);

            const NavVertex* next =
                navMesh.GetVertex(nextID);

            if (!previous ||
                !current ||
                !next)
            {
                return false;
            }

            const glm::vec3 edgeA =
                current->Position -
                previous->Position;

            const glm::vec3 edgeB =
                next->Position -
                current->Position;

            const glm::vec3 turnCross =
                glm::cross(edgeA, edgeB);

            const float turn =
                glm::dot(
                    turnCross,
                    polygonNormal);

            if (std::abs(turn) <=
                GeometryEpsilon)
            {
                continue;
            }

            const float currentSign =
                turn > 0.0f ? 1.0f : -1.0f;

            if (referenceSign == 0.0f)
            {
                referenceSign = currentSign;
            }
            else if (currentSign !=
                referenceSign)
            {
                return false;
            }
        }

        return referenceSign != 0.0f;
    }

    bool NavMeshGenerator::BuildMergedPolygon(
        const NavPolygon& polygonA,
        const NavPolygon& polygonB,
        std::vector<NavVertexID>& mergedVertices)
    {
        mergedVertices.clear();

        size_t sharedEdgeA =
            polygonA.Edges.size();

        size_t sharedEdgeB =
            polygonB.Edges.size();

        for (size_t edgeIndexA = 0;
            edgeIndexA < polygonA.Edges.size();
            ++edgeIndexA)
        {
            const NavEdge& edgeA =
                polygonA.Edges[edgeIndexA];

            for (size_t edgeIndexB = 0;
                edgeIndexB < polygonB.Edges.size();
                ++edgeIndexB)
            {
                const NavEdge& edgeB =
                    polygonB.Edges[edgeIndexB];

                if (edgeA.StartVertex ==
                    edgeB.EndVertex &&
                    edgeA.EndVertex ==
                    edgeB.StartVertex)
                {
                    sharedEdgeA = edgeIndexA;
                    sharedEdgeB = edgeIndexB;
                    break;
                }
            }

            if (sharedEdgeA !=
                polygonA.Edges.size())
            {
                break;
            }
        }

        if (sharedEdgeA ==
            polygonA.Edges.size())
        {
            return false;
        }

        const size_t countA =
            polygonA.Vertices.size();

        const size_t countB =
            polygonB.Vertices.size();

        if (countA < 3 || countB < 3)
        {
            return false;
        }

        //
        // Walk around polygon A using the boundary path
        // opposite to the shared edge.
        //
        for (size_t offset = 0;
            offset < countA;
            ++offset)
        {
            const size_t index =
                (sharedEdgeA + 1 + offset) %
                countA;

            mergedVertices.push_back(
                polygonA.Vertices[index]);
        }

        //
        // Polygon B shares two vertices with polygon A.
        // Add only the vertices belonging to its outer path.
        //
        for (size_t offset = 0;
            offset < countB - 2;
            ++offset)
        {
            const size_t index =
                (sharedEdgeB + 2 + offset) %
                countB;

            mergedVertices.push_back(
                polygonB.Vertices[index]);
        }

        return mergedVertices.size() >= 3;
    }

    void NavMeshGenerator::MergeConvexPolygons(
        NavMesh& navMesh,
        float planarityTolerance)
    {
        bool merged = true;

        while (merged)
        {
            merged = false;

            for (size_t polygonIndexA = 0;
                polygonIndexA <
                navMesh.GetPolygonCount();
                ++polygonIndexA)
            {
                const NavPolygon* polygonA =
                    navMesh.GetPolygon(
                        static_cast<NavPolygonID>(
                            polygonIndexA));

                if (!polygonA)
                {
                    continue;
                }

                for (size_t polygonIndexB =
                    polygonIndexA + 1;
                    polygonIndexB <
                    navMesh.GetPolygonCount();
                    ++polygonIndexB)
                {
                    const NavPolygon* polygonB =
                        navMesh.GetPolygon(
                            static_cast<NavPolygonID>(
                                polygonIndexB));

                    if (!polygonB)
                    {
                        continue;
                    }

                    std::vector<NavVertexID>
                        mergedVertices;

                    if (!BuildMergedPolygon(
                        *polygonA,
                        *polygonB,
                        mergedVertices))
                    {
                        continue;
                    }

                    //
                    // A merged navigation polygon must represent
                    // one planar surface. Convexity alone is not
                    // enough for a valid navigation polygon.
                    //
                    if (!IsPolygonPlanar(
                        navMesh,
                        mergedVertices,
                        planarityTolerance))
                    {
                        continue;
                    }

                    if (!IsPolygonConvex(
                        navMesh,
                        mergedVertices))
                    {
                        continue;
                    }

                    std::vector<NavPolygon>
                        newPolygons;

                    newPolygons.reserve(
                        navMesh.GetPolygonCount() - 1);

                    for (size_t polygonIndex = 0;
                        polygonIndex <
                        navMesh.GetPolygonCount();
                        ++polygonIndex)
                    {
                        if (polygonIndex ==
                            polygonIndexA ||
                            polygonIndex ==
                            polygonIndexB)
                        {
                            continue;
                        }

                        const NavPolygon* polygon =
                            navMesh.GetPolygon(
                                static_cast<
                                NavPolygonID>(
                                    polygonIndex));

                        if (polygon)
                        {
                            newPolygons.push_back(
                                *polygon);
                        }
                    }

                    NavPolygon mergedPolygon;

                    mergedPolygon.Vertices =
                        std::move(mergedVertices);

                    //
                    // For now we only merge polygons generated
                    // with the same default navigation cost.
                    //
                    mergedPolygon.Cost =
                        polygonA->Cost;

                    mergedPolygon.Edges.reserve(
                        mergedPolygon.Vertices.size());

                    glm::vec3 center{ 0.0f };

                    for (size_t vertexIndex = 0;
                        vertexIndex <
                        mergedPolygon.Vertices.size();
                        ++vertexIndex)
                    {
                        const NavVertexID startVertex =
                            mergedPolygon.Vertices[
                                vertexIndex];

                        const NavVertexID endVertex =
                            mergedPolygon.Vertices[
                                (vertexIndex + 1) %
                                    mergedPolygon.Vertices.size()];

                        NavEdge edge;

                        edge.StartVertex =
                            startVertex;

                        edge.EndVertex =
                            endVertex;

                        mergedPolygon.Edges.push_back(
                            edge);

                        const NavVertex* vertex =
                            navMesh.GetVertex(
                                startVertex);

                        if (vertex)
                        {
                            center +=
                                vertex->Position;
                        }
                    }

                    mergedPolygon.Center =
                        center /
                        static_cast<float>(
                            mergedPolygon.Vertices.size());

                    newPolygons.push_back(
                        std::move(mergedPolygon));

                    navMesh.ReplacePolygons(
                        std::move(newPolygons));

                    merged = true;

                    break;
                }

                if (merged)
                {
                    break;
                }
            }
        }
    }
}