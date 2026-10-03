#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture.h"

namespace NoJob
{
    void SceneRenderer::Render(
        Scene& scene,
        const glm::mat4& viewProjection)
    {
        for (Entity entity : scene.GetEntities())
        {
            if (!entity.HasComponent<MeshComponent>()
                || !entity.HasComponent<MeshRendererComponent>())
                continue;

            const auto& transform =
                entity.GetComponent<TransformComponent>();
            const auto& mesh =
                entity.GetComponent<MeshComponent>();
            const auto& renderer =
                entity.GetComponent<MeshRendererComponent>();

            if (!mesh.MeshAsset
                || !renderer.MaterialAsset
                || !renderer.MaterialAsset->GetShader())
                continue;

            const auto& material = renderer.MaterialAsset;
            const auto& shader = material->GetShader();

            if (material->IsUsingTexture())
                material->GetTexture()->Bind(0);

            Renderer::Submit(
                mesh.MeshAsset->GetVertexArray(),
                shader,
                transform.GetTransform(),
                viewProjection,
                material->GetColor(),
                material->IsUsingTexture() ? 1 : 0);
        }
    }
}
