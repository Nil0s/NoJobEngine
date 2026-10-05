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
            const glm::mat4& viewProjection,
            const glm::vec3& cameraPosition = glm::vec3(0.0f));
    };
}
