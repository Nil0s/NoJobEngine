#pragma once
#include <glm/glm.hpp>
#include <memory>

namespace NoJob
{
    class Shader;
    class Texture2D;

    class Material
    {
    public:
        Material(std::shared_ptr<Shader> shader,
                 const glm::vec4& color = glm::vec4(1.0f));

        const std::shared_ptr<Shader>& GetShader() const { return m_Shader; }
        glm::vec4& GetColor() { return m_Color; }
        const glm::vec4& GetColor() const { return m_Color; }

        void SetTexture(std::shared_ptr<Texture2D> texture)
        {
            m_Texture = std::move(texture);
        }

        const std::shared_ptr<Texture2D>& GetTexture() const
        {
            return m_Texture;
        }

        bool& UseTexture() { return m_UseTexture; }

        bool IsUsingTexture() const
        {
            return m_UseTexture && m_Texture != nullptr;
        }

    private:
        std::shared_ptr<Shader> m_Shader;
        glm::vec4 m_Color{ 1.0f };
        std::shared_ptr<Texture2D> m_Texture;
        bool m_UseTexture = true;
    };
}
