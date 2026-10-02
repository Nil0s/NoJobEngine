#pragma once
#include "Engine/Renderer/Shader.h"
#include <cstdint>

namespace NoJob
{
    class OpenGLShader final : public Shader
    {
    public:
        OpenGLShader(
            const std::string& vertexSource,
            const std::string& fragmentSource);
        ~OpenGLShader() override;

        void Bind() const override;
        void Unbind() const override;

        void SetMat4(
            const std::string& name,
            const glm::mat4& value) override;

        void SetFloat4(
            const std::string& name,
            const glm::vec4& value) override;

    private:
        std::uint32_t CompileShader(
            std::uint32_t type,
            const std::string& source);

        std::uint32_t m_RendererID = 0;
    };
}
