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


    enum class CameraProjectionType
    {
        Perspective = 0,
        Orthographic
    };

    struct CameraComponent
    {
        CameraProjectionType ProjectionType = CameraProjectionType::Perspective;
        bool Primary = true;
        float PerspectiveFOV = 45.0f;
        float PerspectiveNear = 0.1f;
        float PerspectiveFar = 1000.0f;
        float OrthographicSize = 10.0f;
        float OrthographicNear = -1.0f;
        float OrthographicFar = 1000.0f;

        glm::mat4 GetProjection(float aspectRatio) const
        {
            aspectRatio = aspectRatio > 0.0001f ? aspectRatio : 1.0f;
            if (ProjectionType == CameraProjectionType::Perspective)
                return glm::perspective(
                    glm::radians(PerspectiveFOV),
                    aspectRatio,
                    PerspectiveNear,
                    PerspectiveFar);

            const float halfHeight = OrthographicSize * 0.5f;
            const float halfWidth = halfHeight * aspectRatio;
            return glm::ortho(
                -halfWidth, halfWidth,
                -halfHeight, halfHeight,
                OrthographicNear, OrthographicFar);
        }
    };

    struct DirectionalLightComponent
    {
        glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
        float Intensity = 1.0f;
        bool CastShadows = true;
        float ShadowBias = 0.002f;
    };

    struct PointLightComponent
    {
        glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
        float Intensity = 1.0f;
        float Range = 10.0f;
        bool CastShadows = true;
        float ShadowBias = 0.02f;
    };

    struct SpotLightComponent
    {
        glm::vec3 Color{ 1.0f, 1.0f, 1.0f };
        float Intensity = 1.0f;
        float Range = 10.0f;
        float InnerAngle = 20.0f;
        float OuterAngle = 30.0f;
        bool CastShadows = true;
        float ShadowBias = 0.002f;
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

    struct PhysicsMaterial
    {
        float Friction = 0.5f;
        float Bounciness = 0.0f;
    };

    struct BoxColliderComponent
    {
        glm::vec3 Size{ 1.0f, 1.0f, 1.0f };
        bool IsTrigger = false;
        PhysicsMaterial Material;
    };

    struct SphereColliderComponent
    {
        float Radius = 0.5f;
        bool IsTrigger = false;
        PhysicsMaterial Material;
    };

    struct CapsuleColliderComponent
    {
        float Radius = 0.5f;
        float Height = 2.0f;
        bool IsTrigger = false;
        PhysicsMaterial Material;
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
        std::vector<std::shared_ptr<Material>> Materials;

        std::shared_ptr<Material> GetMaterial(std::size_t index) const
        {
            if(index<Materials.size() && Materials[index]) return Materials[index];
            return index==0 ? MaterialAsset : nullptr;
        }
        void SetMaterials(std::vector<std::shared_ptr<Material>> materials)
        {
            Materials=std::move(materials);
            MaterialAsset=Materials.empty()?nullptr:Materials.front();
        }
    };
}
