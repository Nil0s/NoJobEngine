#pragma once
#include "Engine/Renderer/RendererAPI.h"

namespace NoJob
{
    class OpenGLRendererAPI final : public RendererAPI
    {
    public:
        void Init() override;

        void SetViewport(
            std::uint32_t x,
            std::uint32_t y,
            std::uint32_t width,
            std::uint32_t height) override;

        void SetClearColor(float r, float g, float b, float a) override;
        void Clear() override;
        void DrawIndexed(const VertexArray& vertexArray) override;
        void DrawIndexedRange(const VertexArray& vertexArray,
                              std::uint32_t indexCount,
                              std::uint32_t indexOffset) override;
    };
}
