#pragma once
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
            const std::shared_ptr<Shader>& shader);
    };
}
