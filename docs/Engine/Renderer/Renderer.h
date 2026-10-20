#pragma once
#include <glm/glm.hpp>
#include <memory>

namespace NoJob
{
    class VertexArray;
    class Shader;

    class Renderer
    {
    public:
        static void Init();
        static void Shutdown();
        static void BeginFrame();
        static void EndFrame();

        static void Submit(
            const std::shared_ptr<VertexArray>& vertexArray,
            const std::shared_ptr<Shader>& shader,
            const glm::mat4& transform = glm::mat4(1.0f),
            const glm::mat4& viewProjection = glm::mat4(1.0f),
            const glm::vec4& color = glm::vec4(1.0f));

    static void Submit(
        const std::shared_ptr<VertexArray>& vertexArray,
        const std::shared_ptr<Shader>& shader,
        const glm::mat4& transform,
        const glm::mat4& viewProjection,
        const glm::vec4& color,
        int useTexture);

        static void SubmitRange(
            const std::shared_ptr<VertexArray>& vertexArray,
            const std::shared_ptr<Shader>& shader,
            std::uint32_t indexCount, std::uint32_t indexOffset,
            const glm::mat4& transform, const glm::mat4& viewProjection,
            const glm::vec4& color, int useTexture);
    };
}
