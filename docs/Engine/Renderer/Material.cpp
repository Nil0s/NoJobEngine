#include "Engine/Renderer/Material.h"
#include <utility>

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
