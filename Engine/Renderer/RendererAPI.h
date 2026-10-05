#pragma once
#include <cstdint>

namespace NoJob
{
    class VertexArray;

    enum class GraphicsAPI
    {
        None = 0,
        OpenGL,
        Vulkan
    };

    class RendererAPI
    {
    public:
        virtual ~RendererAPI() = default;

        virtual void Init() = 0;

        virtual void SetViewport(
            std::uint32_t x,
            std::uint32_t y,
            std::uint32_t width,
            std::uint32_t height) = 0;

        virtual void SetClearColor(
            float r, float g, float b, float a) = 0;

        virtual void Clear() = 0;

        virtual void DrawIndexed(
            const VertexArray& vertexArray) = 0;
        virtual void DrawIndexedRange(
            const VertexArray& vertexArray,
            std::uint32_t indexCount,
            std::uint32_t indexOffset) = 0;

        static GraphicsAPI GetAPI() { return s_API; }

    protected:
        static GraphicsAPI s_API;
    };
}
