#pragma once
#include <cstdint>
#include <memory>

namespace NoJob
{
    class RendererAPI;
    class VertexArray;

    class RenderCommand
    {
    public:
        static void Init();

        static void SetViewport(
            std::uint32_t x,
            std::uint32_t y,
            std::uint32_t width,
            std::uint32_t height);

        static void SetClearColor(float r, float g, float b, float a);
        static void Clear();
        static void DrawIndexed(const VertexArray& vertexArray);

    private:
        static std::unique_ptr<RendererAPI> s_RendererAPI;
    };
}
