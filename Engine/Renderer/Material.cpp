#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Shader.h"

namespace NoJob
{
    Material::Material(
        std::shared_ptr<Shader> shader,
        const glm::vec4& color)
        : m_Shader(std::move(shader)),
          m_Color(color)
    {
    }
}
