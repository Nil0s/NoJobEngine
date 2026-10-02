#include "Engine/Platform/OpenGL/OpenGLShader.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <string>

namespace NoJob
{
    OpenGLShader::OpenGLShader(
        const std::string& vertexSource,
        const std::string& fragmentSource)
    {
        const std::uint32_t vertexShader =
            CompileShader(GL_VERTEX_SHADER, vertexSource);

        const std::uint32_t fragmentShader =
            CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, vertexShader);
        glAttachShader(m_RendererID, fragmentShader);
        glLinkProgram(m_RendererID);

        GLint linked = GL_FALSE;
        glGetProgramiv(m_RendererID, GL_LINK_STATUS, &linked);

        if (linked != GL_TRUE)
        {
            GLint length = 0;
            glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &length);
            std::string message(static_cast<std::size_t>(length), '\0');
            glGetProgramInfoLog(m_RendererID, length, nullptr, message.data());

            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteProgram(m_RendererID);
            m_RendererID = 0;

            throw std::runtime_error(
                "NoJobEngine: shader link failed: " + message);
        }

        glDetachShader(m_RendererID, vertexShader);
        glDetachShader(m_RendererID, fragmentShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    OpenGLShader::~OpenGLShader()
    {
        glDeleteProgram(m_RendererID);
    }

    void OpenGLShader::Bind() const
    {
        glUseProgram(m_RendererID);
    }

    void OpenGLShader::Unbind() const
    {
        glUseProgram(0);
    }

    void OpenGLShader::SetMat4(
        const std::string& name,
        const glm::mat4& value)
    {
        const GLint location =
            glGetUniformLocation(m_RendererID, name.c_str());

        glUniformMatrix4fv(
            location, 1, GL_FALSE, glm::value_ptr(value));
    }

    void OpenGLShader::SetFloat4(
        const std::string& name,
        const glm::vec4& value)
    {
        const GLint location =
            glGetUniformLocation(m_RendererID, name.c_str());

        glUniform4f(
            location,
            value.x,
            value.y,
            value.z,
            value.w);
    }

    std::uint32_t OpenGLShader::CompileShader(
        std::uint32_t type,
        const std::string& source)
    {
        const std::uint32_t shader = glCreateShader(type);
        const char* sourcePointer = source.c_str();

        glShaderSource(shader, 1, &sourcePointer, nullptr);
        glCompileShader(shader);

        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

        if (compiled != GL_TRUE)
        {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string message(static_cast<std::size_t>(length), '\0');
            glGetShaderInfoLog(shader, length, nullptr, message.data());

            glDeleteShader(shader);

            throw std::runtime_error(
                "NoJobEngine: shader compilation failed: " + message);
        }

        return shader;
    }
}
