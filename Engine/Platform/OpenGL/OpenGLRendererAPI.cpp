#include "Engine/Platform/OpenGL/OpenGLRendererAPI.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/Buffer.h"
#include <glad/gl.h>

namespace NoJob
{
    void OpenGLRendererAPI::Init()
    {
        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRendererAPI::SetViewport(
        std::uint32_t x,
        std::uint32_t y,
        std::uint32_t width,
        std::uint32_t height)
    {
        glViewport(
            static_cast<GLint>(x),
            static_cast<GLint>(y),
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height));
    }

    void OpenGLRendererAPI::SetClearColor(
        float r, float g, float b, float a)
    {
        glClearColor(r, g, b, a);
    }

    void OpenGLRendererAPI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::DrawIndexed(
        const VertexArray& vertexArray)
    {
        const auto& indexBuffer = vertexArray.GetIndexBuffer();
        if (!indexBuffer)
            return;

        vertexArray.Bind();

        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(indexBuffer->GetCount()),
            GL_UNSIGNED_INT,
            nullptr);
    }
}
