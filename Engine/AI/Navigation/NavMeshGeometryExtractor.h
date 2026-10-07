#pragma once

#include "Engine/AI/Navigation/NavMeshGenerator.h"

namespace NoJob
{
    class Scene;

    class NavMeshGeometryExtractor
    {
    public:
        static NavMeshSourceGeometry Extract(Scene& scene);
    };
}