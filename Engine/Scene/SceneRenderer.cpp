#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace NoJob
{
    namespace
    {
        glm::vec3 WorldPosition(const glm::mat4& world)
        {
            return glm::vec3(world[3]);
        }

        glm::vec3 WorldForward(const glm::mat4& world)
        {
            glm::vec3 forward = -glm::vec3(world[2]);
            const float length = glm::length(forward);
            return length > 0.0001f
                ? forward / length
                : glm::vec3(0.0f, 0.0f, -1.0f);
        }

        void UploadLighting(
            Scene& scene,
            const std::shared_ptr<Shader>& shader,
            const glm::vec3& cameraPosition)
        {
            shader->Bind();
            shader->SetFloat3("u_ViewPosition", cameraPosition);
            shader->SetFloat3("u_AmbientColor", {0.28f, 0.29f, 0.32f});

            bool hasDirectional = false;
            int pointCount = 0;
            int spotCount = 0;

            for (Entity lightEntity : scene.GetEntities())
            {
                const glm::mat4 world =
                    scene.GetWorldTransform(lightEntity);

                if (!hasDirectional &&
                    lightEntity.HasComponent<DirectionalLightComponent>())
                {
                    const auto& light =
                        lightEntity.GetComponent<DirectionalLightComponent>();
                    shader->SetInt("u_HasDirectionalLight", 1);
                    shader->SetFloat3(
                        "u_DirectionalLight.direction",
                        WorldForward(world));
                    shader->SetFloat3(
                        "u_DirectionalLight.color",
                        light.Color);
                    shader->SetFloat(
                        "u_DirectionalLight.intensity",
                        std::max(light.Intensity, 0.0f));
                    hasDirectional = true;
                }

                if (pointCount < 4 &&
                    lightEntity.HasComponent<PointLightComponent>())
                {
                    const auto& light =
                        lightEntity.GetComponent<PointLightComponent>();
                    const std::string base =
                        "u_PointLights[" + std::to_string(pointCount) + "]";
                    shader->SetFloat3(
                        base + ".position", WorldPosition(world));
                    shader->SetFloat3(base + ".color", light.Color);
                    shader->SetFloat(
                        base + ".intensity",
                        std::max(light.Intensity, 0.0f));
                    shader->SetFloat(
                        base + ".range",
                        std::max(light.Range, 0.01f));
                    ++pointCount;
                }

                if (spotCount < 4 &&
                    lightEntity.HasComponent<SpotLightComponent>())
                {
                    const auto& light =
                        lightEntity.GetComponent<SpotLightComponent>();
                    const std::string base =
                        "u_SpotLights[" + std::to_string(spotCount) + "]";
                    shader->SetFloat3(
                        base + ".position", WorldPosition(world));
                    shader->SetFloat3(
                        base + ".direction", WorldForward(world));
                    shader->SetFloat3(base + ".color", light.Color);
                    shader->SetFloat(
                        base + ".intensity",
                        std::max(light.Intensity, 0.0f));
                    shader->SetFloat(
                        base + ".range",
                        std::max(light.Range, 0.01f));
                    shader->SetFloat(
                        base + ".innerCos",
                        std::cos(glm::radians(light.InnerAngle)));
                    shader->SetFloat(
                        base + ".outerCos",
                        std::cos(glm::radians(light.OuterAngle)));
                    ++spotCount;
                }
            }

            shader->SetInt(
                "u_HasDirectionalLight",
                hasDirectional ? 1 : 0);
            shader->SetInt("u_PointLightCount", pointCount);
            shader->SetInt("u_SpotLightCount", spotCount);
        }
    }

    void SceneRenderer::Render(
        Scene& scene,
        const glm::mat4& viewProjection,
        const glm::vec3& cameraPosition)
    {
        for (Entity entity : scene.GetEntities())
        {
            if (!entity.HasComponent<MeshComponent>() ||
                !entity.HasComponent<MeshRendererComponent>())
                continue;

            const auto& mesh =
                entity.GetComponent<MeshComponent>();
            const auto& renderer =
                entity.GetComponent<MeshRendererComponent>();

            if (!mesh.MeshAsset ||
                !renderer.MaterialAsset ||
                !renderer.MaterialAsset->GetShader())
                continue;

            const auto& material = renderer.MaterialAsset;
            const auto& shader = material->GetShader();

            UploadLighting(scene, shader, cameraPosition);

            if (material->IsUsingTexture())
                material->GetTexture()->Bind(0);

            Renderer::Submit(
                mesh.MeshAsset->GetVertexArray(),
                shader,
                scene.GetWorldTransform(entity),
                viewProjection,
                material->GetColor(),
                material->IsUsingTexture() ? 1 : 0);
        }
    }
}
