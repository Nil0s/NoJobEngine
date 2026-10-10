#pragma once
#include "Engine/AI/Navigation/NavMeshGenerator.h"
namespace NoJob {
class Scene;
class NavMeshVoxelizer {
public:
    static NavMeshSourceGeometry Rasterize(Scene& scene, const NavMeshSourceGeometry& source,
                                           float cellSize, float agentRadius, float agentHeight,
                                           float maxSlopeAngle);
};
}
