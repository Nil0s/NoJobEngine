#pragma once

#include <glm/glm.hpp>

namespace NoJob
{
    class Scene;

    class SceneRenderer
    {
    public:
        static void Render(
            Scene& scene,
            const glm::mat4& viewProjection);
    };
}
