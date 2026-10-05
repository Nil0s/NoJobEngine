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

        float& Metallic() { return m_Metallic; }
        float& Roughness() { return m_Roughness; }
        float& AmbientOcclusion() { return m_AmbientOcclusion; }
        float& NormalStrength() { return m_NormalStrength; }
        glm::vec3& EmissiveColor() { return m_EmissiveColor; }
        float& EmissiveStrength() { return m_EmissiveStrength; }
        const float& Metallic() const { return m_Metallic; }
        const float& Roughness() const { return m_Roughness; }
        const float& AmbientOcclusion() const { return m_AmbientOcclusion; }

        void SetNormalTexture(std::shared_ptr<Texture2D> v) { m_NormalTexture = std::move(v); }
        void SetMetallicTexture(std::shared_ptr<Texture2D> v) { m_MetallicTexture = std::move(v); }
        void SetRoughnessTexture(std::shared_ptr<Texture2D> v) { m_RoughnessTexture = std::move(v); }
        void SetAOTexture(std::shared_ptr<Texture2D> v) { m_AOTexture = std::move(v); }
        void SetEmissiveTexture(std::shared_ptr<Texture2D> v) { m_EmissiveTexture = std::move(v); }
        const std::shared_ptr<Texture2D>& GetNormalTexture() const { return m_NormalTexture; }
        const std::shared_ptr<Texture2D>& GetMetallicTexture() const { return m_MetallicTexture; }
        const std::shared_ptr<Texture2D>& GetRoughnessTexture() const { return m_RoughnessTexture; }
        const std::shared_ptr<Texture2D>& GetAOTexture() const { return m_AOTexture; }
        const std::shared_ptr<Texture2D>& GetEmissiveTexture() const { return m_EmissiveTexture; }

    private:
        std::shared_ptr<Shader> m_Shader;
        glm::vec4 m_Color{ 1.0f };
        std::shared_ptr<Texture2D> m_Texture;
        bool m_UseTexture = true;
        float m_Metallic = 0.0f;
        float m_Roughness = 0.5f;
        float m_AmbientOcclusion = 1.0f;
        float m_NormalStrength = 1.0f;
        glm::vec3 m_EmissiveColor{ 0.0f };
        float m_EmissiveStrength = 0.0f;
        std::shared_ptr<Texture2D> m_NormalTexture;
        std::shared_ptr<Texture2D> m_MetallicTexture;
        std::shared_ptr<Texture2D> m_RoughnessTexture;
        std::shared_ptr<Texture2D> m_AOTexture;
        std::shared_ptr<Texture2D> m_EmissiveTexture;
    };
}
