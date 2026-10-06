#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include "Engine/Renderer/Framebuffer.h"

namespace NoJob
{
    struct RendererStatistics
    {
        std::uint32_t DrawCalls = 0;
        std::uint32_t ShadowDrawCalls = 0;
        std::uint32_t ShadowPasses = 0;
        std::uint64_t Triangles = 0;
        std::uint64_t ShadowTriangles = 0;
        float CPUTimeMs = 0.0f;
        float GPUTimeMs = 0.0f;
        bool GPUTimeValid = false;
    };

    class Scene;

    class SceneRenderer
    {
    public:
        static void Render(
            Scene& scene,
            const glm::mat4& viewProjection,
            const glm::vec3& cameraPosition = glm::vec3(0.0f),
            bool rebuildShadowMaps = true);

        static void SetGraphicsSettings(
            const FramebufferSpecification& settings);
        static const FramebufferSpecification& GetGraphicsSettings();
        static const RendererStatistics& GetStatistics();
    };
}
