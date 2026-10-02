#include "Engine/Platform/OpenGL/OpenGLVertexArray.h"
#include "Engine/Renderer/Buffer.h"
#include <glad/gl.h>

namespace NoJob
{
    OpenGLVertexArray::OpenGLVertexArray()
    {
        glCreateVertexArrays(1, &m_RendererID);
    }

    OpenGLVertexArray::~OpenGLVertexArray()
    {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void OpenGLVertexArray::Bind() const
    {
        glBindVertexArray(m_RendererID);
    }

    void OpenGLVertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }

    void OpenGLVertexArray::SetVertexBuffer(
        const std::shared_ptr<VertexBuffer>& vertexBuffer)
    {
        m_VertexBuffer = vertexBuffer;

        Bind();
        m_VertexBuffer->Bind();

        // Step 2 layout: location 0 = vec3 position.
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            3 * sizeof(float),
            nullptr);
    }

    void OpenGLVertexArray::SetIndexBuffer(
        const std::shared_ptr<IndexBuffer>& indexBuffer)
    {
        m_IndexBuffer = indexBuffer;

        Bind();
        m_IndexBuffer->Bind();
    }
}
