#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace NoJob
{
    class Mesh;
    class Material;
    class AnimationAsset;

    struct IDComponent
    {
        std::uint64_t ID = 0;
    };

    struct TagComponent
    {
        std::string Tag;
    };

    // ---------------------------------------------------------
    // Global Entity Layers
    // ---------------------------------------------------------

    using LayerMask = std::uint32_t;

    using LayerMask = std::uint32_t;

    namespace EntityLayers
    {
        constexpr std::uint32_t Default = 0;
        constexpr std::uint32_t Player = 1;
        constexpr std::uint32_t Enemy = 2;
        constexpr std::uint32_t Environment = 3;
        constexpr std::uint32_t Interactable = 4;

        constexpr std::uint32_t MaxLayers = 32;

        constexpr LayerMask All = 0xFFFFFFFFu;
        constexpr LayerMask None = 0u;

        constexpr LayerMask Bit(std::uint32_t layer)
        {
            return layer < MaxLayers
                ? (LayerMask{ 1 } << layer)
                : None;
        }
    }

    struct LayerComponent
    {
        std::uint32_t Layer = EntityLayers::Default;
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

    struct AudioSourceComponent
    {
        std::string ClipPath;
        bool PlayOnAwake = true;
        bool Loop = false;
        float Volume = 1.0f;
        float Pitch = 1.0f;

        // V1.6 Block 2 spatial audio. SpatialBlend is intentionally Unity-like:
        // 0 = fully 2D, 1 = fully 3D, intermediate values crossfade between both.
        float SpatialBlend = 0.0f;
        float MinDistance = 1.0f;
        float MaxDistance = 50.0f;
        float DopplerFactor = 1.0f;
    };

    struct AudioListenerComponent
    {
        bool Enabled = true;
    };


    enum class ParticleShape : std::uint8_t { Point = 0, Sphere = 1, Cone = 2 };
    enum class ParticleBlendMode : std::uint8_t { Alpha = 0, Additive = 1 };

    struct ParticleSystemComponent
    {
        bool Playing = true;
        bool Loop = true;
        float Duration = 5.0f;
        float StartLifetime = 2.0f;
        float LifetimeRandom = 0.0f;
        float StartSpeed = 2.0f;
        float SpeedRandom = 0.0f;
        float StartSize = 0.2f;
        float SizeRandom = 0.0f;
        glm::vec4 StartColor{ 1.0f, 0.65f, 0.15f, 1.0f };
        glm::vec4 EndColor{ 1.0f, 0.15f, 0.02f, 0.0f };
        float EndSizeMultiplier = 0.25f;
        float EmissionRate = 20.0f;
        std::uint32_t MaxParticles = 500;
        glm::vec3 Direction{ 0.0f, 1.0f, 0.0f };
        glm::vec3 Gravity{ 0.0f, -1.0f, 0.0f };

        ParticleShape Shape = ParticleShape::Point;
        float ShapeRadius = 0.5f;
        float ConeAngle = 25.0f;

        ParticleBlendMode BlendMode = ParticleBlendMode::Alpha;
        std::string TexturePath;
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
    struct NavAgentComponent
    {
        bool Enabled = true;

        float Speed = 3.5f;
        float StoppingDistance = 0.1f;

        glm::vec3 Destination{ 0.0f };

        bool HasDestination = false;

        float RepathInterval = 0.5f;
    };
    struct PerceptionComponent
    {
        bool Enabled = true;
        LayerMask DetectionMask = EntityLayers::All;

        float DetectionRadius = 10.0f;
        float FieldOfView = 120.0f;

     
        float MemoryDuration = 3.0f;

        
        float UpdateInterval = 0.1f;

        bool DebugDraw = true;


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

    enum class ScriptFieldType { Float = 0, Int, Bool, Vec3 };

    struct ScriptFieldValue
    {
        ScriptFieldType Type = ScriptFieldType::Float;
        float Float = 0.0f;
        int Int = 0;
        bool Bool = false;
        glm::vec3 Vec3{ 0.0f };

        static ScriptFieldValue MakeFloat(float value) { ScriptFieldValue v; v.Type=ScriptFieldType::Float; v.Float=value; return v; }
        static ScriptFieldValue MakeInt(int value) { ScriptFieldValue v; v.Type=ScriptFieldType::Int; v.Int=value; return v; }
        static ScriptFieldValue MakeBool(bool value) { ScriptFieldValue v; v.Type=ScriptFieldType::Bool; v.Bool=value; return v; }
        static ScriptFieldValue MakeVec3(glm::vec3 value) { ScriptFieldValue v; v.Type=ScriptFieldType::Vec3; v.Vec3=value; return v; }
    };

    struct NativeScriptComponent
    {
        bool Enabled = true;
        std::string ScriptName = "Rotator";
        std::unordered_map<std::string, ScriptFieldValue> Fields;
    };


    struct AnimatorComponent
    {
        std::shared_ptr<AnimationAsset> Animation;
        int ClipIndex = 0;
        float TimeSeconds = 0.0f;
        float Speed = 1.0f;
        bool Playing = true;
        bool Loop = true;
    };

    struct PrefabInstanceComponent
    {
        std::string SourcePath;
        bool IsRoot = true;
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
