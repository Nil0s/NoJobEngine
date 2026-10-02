#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace NoJob
{
    class Shader;

    class Material
    {
    public:
        Material(
            std::shared_ptr<Shader> shader,
            const glm::vec4& color = glm::vec4(1.0f));

        const std::shared_ptr<Shader>& GetShader() const
        {
            return m_Shader;
        }

        glm::vec4& GetColor() { return m_Color; }
        const glm::vec4& GetColor() const { return m_Color; }

    private:
        std::shared_ptr<Shader> m_Shader;
        glm::vec4 m_Color{ 1.0f };
    };
}
