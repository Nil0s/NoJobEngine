#include "Engine/Platform/OpenGL/OpenGLFramebuffer.h"

#include <glad/gl.h>
#include <stdexcept>

namespace NoJob
{
    OpenGLFramebuffer::OpenGLFramebuffer(
        const FramebufferSpecification& specification)
        : m_Specification(specification)
    {
        Invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer()
    {
        if (m_RendererID)
            glDeleteFramebuffers(1, &m_RendererID);

        if (m_ColorAttachment)
            glDeleteTextures(1, &m_ColorAttachment);

        if (m_DepthAttachment)
            glDeleteTextures(1, &m_DepthAttachment);
    }

    void OpenGLFramebuffer::Invalidate()
    {
        if (m_RendererID)
        {
            glDeleteFramebuffers(1, &m_RendererID);
            glDeleteTextures(1, &m_ColorAttachment);
            glDeleteTextures(1, &m_DepthAttachment);

            m_RendererID = 0;
            m_ColorAttachment = 0;
            m_DepthAttachment = 0;
        }

        glCreateFramebuffers(1, &m_RendererID);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorAttachment);
        glTextureStorage2D(
            m_ColorAttachment,
            1,
            GL_RGBA8,
            static_cast<GLsizei>(m_Specification.Width),
            static_cast<GLsizei>(m_Specification.Height));

        glTextureParameteri(
            m_ColorAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(
            m_ColorAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(
            m_ColorAttachment, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(
            m_ColorAttachment, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glNamedFramebufferTexture(
            m_RendererID,
            GL_COLOR_ATTACHMENT0,
            m_ColorAttachment,
            0);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment);
        glTextureStorage2D(
            m_DepthAttachment,
            1,
            GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(m_Specification.Width),
            static_cast<GLsizei>(m_Specification.Height));

        glNamedFramebufferTexture(
            m_RendererID,
            GL_DEPTH_STENCIL_ATTACHMENT,
            m_DepthAttachment,
            0);

        const GLenum status =
            glCheckNamedFramebufferStatus(
                m_RendererID, GL_FRAMEBUFFER);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            throw std::runtime_error(
                "NoJobEngine: framebuffer is incomplete.");
        }
    }

    void OpenGLFramebuffer::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
        glViewport(
            0,
            0,
            static_cast<GLsizei>(m_Specification.Width),
            static_cast<GLsizei>(m_Specification.Height));
    }

    void OpenGLFramebuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Resize(
        std::uint32_t width,
        std::uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        if (width == m_Specification.Width
            && height == m_Specification.Height)
        {
            return;
        }

        m_Specification.Width = width;
        m_Specification.Height = height;
        Invalidate();
    }
}
