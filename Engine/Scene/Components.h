#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace NoJob
{
    class Mesh;
    class Material;

    struct IDComponent
    {
        std::uint64_t ID = 0;
    };

    struct TagComponent
    {
        std::string Tag;
    };

    struct TransformComponent
    {
        glm::vec3 Position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation{ 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };

        glm::mat4 GetTransform() const
        {
            const glm::mat4 rotation =
                glm::rotate(
                    glm::mat4(1.0f),
                    Rotation.z,
                    { 0.0f, 0.0f, 1.0f }) *
                glm::rotate(
                    glm::mat4(1.0f),
                    Rotation.y,
                    { 0.0f, 1.0f, 0.0f }) *
                glm::rotate(
                    glm::mat4(1.0f),
                    Rotation.x,
                    { 1.0f, 0.0f, 0.0f });

            return glm::translate(
                       glm::mat4(1.0f), Position)
                * rotation
                * glm::scale(
                    glm::mat4(1.0f), Scale);
        }
    };

    struct RelationshipComponent
    {
        std::uint32_t Parent = 0;
        std::vector<std::uint32_t> Children;
    };


    enum class RigidbodyType
    {
        Static = 0,
        Dynamic,
        Kinematic
    };

    struct RigidbodyComponent
    {
        RigidbodyType Type = RigidbodyType::Dynamic;
        float Mass = 1.0f;
        bool UseGravity = true;
    };

    struct BoxColliderComponent
    {
        // Full local-space size. The entity world scale is applied at runtime.
        glm::vec3 Size{ 1.0f, 1.0f, 1.0f };
    };

    struct NativeScriptComponent
    {
        bool Enabled = true;
        float RotationSpeed = 1.0f; // radians per second
    };

    struct MeshComponent
    {
        std::shared_ptr<Mesh> MeshAsset;
    };

    struct MeshRendererComponent
    {
        std::shared_ptr<Material> MaterialAsset;
    };
}
