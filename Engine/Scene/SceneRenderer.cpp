#include "Engine/Scene/SceneRenderer.h"

#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

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
            {
                continue;
            }

            const auto& transform =
                entity.GetComponent<TransformComponent>();

            const auto& meshComponent =
                entity.GetComponent<MeshComponent>();

            const auto& rendererComponent =
                entity.GetComponent<MeshRendererComponent>();

            if (!meshComponent.MeshAsset
                || !rendererComponent.MaterialAsset
                || !rendererComponent.MaterialAsset->GetShader())
            {
                continue;
            }

            Renderer::Submit(
                meshComponent.MeshAsset->GetVertexArray(),
                rendererComponent.MaterialAsset->GetShader(),
                transform.GetTransform(),
                viewProjection,
                rendererComponent.MaterialAsset->GetColor());
        }
    }
}
