#include "Engine/Platform/OpenGL/OpenGLVertexArray.h"

#include "Engine/Renderer/Buffer.h"

#include <glad/gl.h>
#include <cstdint>

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
        // Position-only layout retained for the editor grid.
        SetVertexBuffer(
            vertexBuffer,
            BufferLayout{
                { ShaderDataType::Float3, "a_Position" }
            });
    }

    void OpenGLVertexArray::SetVertexBuffer(
        const std::shared_ptr<VertexBuffer>& vertexBuffer,
        const BufferLayout& layout)
    {
        m_VertexBuffer = vertexBuffer;

        Bind();
        vertexBuffer->Bind();

        std::uint32_t index = 0;

        for (const BufferElement& element : layout.GetElements())
        {
            glEnableVertexAttribArray(index);

            glVertexAttribPointer(
                index,
                static_cast<GLint>(element.GetComponentCount()),
                GL_FLOAT,
                element.Normalized ? GL_TRUE : GL_FALSE,
                static_cast<GLsizei>(layout.GetStride()),
                reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(element.Offset)));

            ++index;
        }
    }

    void OpenGLVertexArray::SetIndexBuffer(
        const std::shared_ptr<IndexBuffer>& indexBuffer)
    {
        Bind();
        indexBuffer->Bind();
        m_IndexBuffer = indexBuffer;
    }
}
