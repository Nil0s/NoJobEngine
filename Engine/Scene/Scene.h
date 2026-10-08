#pragma once

#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/AI/Navigation/NavAgent.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NoJob
{
    class ScriptableEntity;
    class NavigationSystem;

    class Scene
    {
    public:
        struct PerceptionTargetInfo
        {
            std::uint32_t EntityHandle = 0;

            glm::vec3 LastKnownPosition{ 0.0f };

            float TimeSinceLastSeen = 0.0f;

            bool IsVisible = false;
        };
        Scene();
        Scene(const Scene& other);
        Scene& operator=(const Scene&) = delete;
        ~Scene();

        std::unique_ptr<Scene> Copy() const;
        void RestoreFrom(const Scene& other);

        Entity CreateEntity(const std::string& name = "Entity");
        void DestroyEntity(Entity entity);

        bool IsValid(std::uint32_t handle) const;
        std::vector<Entity> GetEntities();

        void OnRuntimeStart();
        void OnRuntimeStop();
        void OnUpdate(float deltaTime);
        std::vector<PerceptionTargetInfo> GetPerceivedTargets(
            std::uint32_t observerHandle) const;
        float GetDeltaTime() const
        {
            return m_DeltaTime;
        }

        // Scene does not own the NavigationSystem.
        // The editor/runtime supplies the navigation world that
        // contains the currently baked NavMesh.
        void SetNavigationSystem(
            NavigationSystem* navigationSystem)
        {
            m_NavigationSystem = navigationSystem;
        }

        NavigationSystem* GetNavigationSystem()
        {
            return m_NavigationSystem;
        }

        const NavigationSystem* GetNavigationSystem() const
        {
            return m_NavigationSystem;
        }

        struct RuntimeParticle
        {
            glm::vec3 Position{ 0.0f };
            glm::vec3 Velocity{ 0.0f };

            float Age = 0.0f;
            float Lifetime = 1.0f;

            float Size = 0.1f;
            float StartSize = 0.1f;

            glm::vec4 Color{ 1.0f };
            glm::vec4 StartColor{ 1.0f };
            glm::vec4 EndColor{ 1.0f };
        };

        const std::vector<RuntimeParticle>&
            GetParticles(std::uint32_t handle) const;

        Entity GetParent(Entity entity);

        std::vector<Entity>
            GetChildren(Entity entity);

        bool IsDescendant(
            Entity possibleDescendant,
            Entity ancestor) const;

        // keepWorldTransform=true makes editor reparenting
        // behave naturally: the object does not jump when
        // its parent changes.
        bool SetParent(
            Entity child,
            Entity parent,
            bool keepWorldTransform = true);

        void Unparent(
            Entity child,
            bool keepWorldTransform = true);

        glm::mat4 GetWorldTransform(
            Entity entity) const;

        void SetWorldTransform(
            Entity entity,
            const glm::mat4& worldTransform);


    private:

        struct EntityData
        {
            IDComponent ID;
            TagComponent Tag;
            TransformComponent Transform;
            RelationshipComponent Relationship;
            LayerComponent Layer;

            std::optional<MeshComponent> Mesh;
            std::optional<AnimatorComponent> Animator;
            std::optional<PrefabInstanceComponent> PrefabInstance;
            std::optional<MeshRendererComponent> MeshRenderer;
            std::optional<NativeScriptComponent> NativeScript;

            std::optional<RigidbodyComponent> Rigidbody;

        
           // V1.8 Native Gameplay AI
            std::optional<NavAgentComponent> NavAgent;
            std::optional<PerceptionComponent> Perception;

            std::optional<BoxColliderComponent> BoxCollider;
            std::optional<SphereColliderComponent> SphereCollider;
            std::optional<CapsuleColliderComponent> CapsuleCollider;

            std::optional<CameraComponent> Camera;

            std::optional<AudioSourceComponent> AudioSource;
            std::optional<AudioListenerComponent> AudioListener;

            std::optional<ParticleSystemComponent> ParticleSystem;

            std::optional<DirectionalLightComponent> DirectionalLight;
            std::optional<PointLightComponent> PointLight;
            std::optional<SpotLightComponent> SpotLight;
        };

        // ---------------------------------------------------------
        // GetComponent
        // ---------------------------------------------------------

        template<typename T>
        T& GetComponent(std::uint32_t handle)
        {
            auto& data = m_Entities.at(handle);

            if constexpr (std::is_same_v<T, IDComponent>)
                return data.ID;

            else if constexpr (std::is_same_v<T, TagComponent>)
                return data.Tag;

            else if constexpr (std::is_same_v<T, TransformComponent>)
                return data.Transform;

            else if constexpr (std::is_same_v<T, RelationshipComponent>)
                return data.Relationship;

            else if constexpr (std::is_same_v<T, LayerComponent>)
                return data.Layer;

            else if constexpr (std::is_same_v<T, MeshComponent>)
                return data.Mesh.value();

            else if constexpr (std::is_same_v<T, AnimatorComponent>)
                return data.Animator.value();

            else if constexpr (std::is_same_v<T, PrefabInstanceComponent>)
                return data.PrefabInstance.value();

            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
                return data.MeshRenderer.value();

            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
                return data.NativeScript.value();

            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                return data.Rigidbody.value();

            else if constexpr (std::is_same_v<T, NavAgentComponent>)
                return data.NavAgent.value();

            else if constexpr (std::is_same_v<T, PerceptionComponent>)
                return data.Perception.value();

            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                return data.BoxCollider.value();

            else if constexpr (std::is_same_v<T, SphereColliderComponent>)
                return data.SphereCollider.value();

            else if constexpr (std::is_same_v<T, CapsuleColliderComponent>)
                return data.CapsuleCollider.value();

            else if constexpr (std::is_same_v<T, CameraComponent>)
                return data.Camera.value();

            else if constexpr (std::is_same_v<T, AudioSourceComponent>)
                return data.AudioSource.value();

            else if constexpr (std::is_same_v<T, AudioListenerComponent>)
                return data.AudioListener.value();

            else if constexpr (std::is_same_v<T, ParticleSystemComponent>)
                return data.ParticleSystem.value();

            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)
                return data.DirectionalLight.value();

            else if constexpr (std::is_same_v<T, PointLightComponent>)
                return data.PointLight.value();

            else if constexpr (std::is_same_v<T, SpotLightComponent>)
                return data.SpotLight.value();

            else
                static_assert(
                    !sizeof(T),
                    "Unsupported NoJob component type.");
        }

        template<typename T>
        const T& GetComponent(std::uint32_t handle) const
        {
            const auto& data = m_Entities.at(handle);

            if constexpr (std::is_same_v<T, IDComponent>)
                return data.ID;

            else if constexpr (std::is_same_v<T, TagComponent>)
                return data.Tag;

            else if constexpr (std::is_same_v<T, TransformComponent>)
                return data.Transform;

            else if constexpr (std::is_same_v<T, RelationshipComponent>)
                return data.Relationship;

            else if constexpr (std::is_same_v<T, MeshComponent>)
                return data.Mesh.value();

            else if constexpr (std::is_same_v<T, AnimatorComponent>)
                return data.Animator.value();

            else if constexpr (std::is_same_v<T, PrefabInstanceComponent>)
                return data.PrefabInstance.value();

            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
                return data.MeshRenderer.value();

            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
                return data.NativeScript.value();

            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                return data.Rigidbody.value();

            else if constexpr (std::is_same_v<T, NavAgentComponent>)
                return data.NavAgent.value();

            else if constexpr (std::is_same_v<T, PerceptionComponent>)
                return data.Perception.value();

            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                return data.BoxCollider.value();

            else if constexpr (std::is_same_v<T, SphereColliderComponent>)
                return data.SphereCollider.value();

            else if constexpr (std::is_same_v<T, CapsuleColliderComponent>)
                return data.CapsuleCollider.value();

            else if constexpr (std::is_same_v<T, CameraComponent>)
                return data.Camera.value();

            else if constexpr (std::is_same_v<T, AudioSourceComponent>)
                return data.AudioSource.value();

            else if constexpr (std::is_same_v<T, AudioListenerComponent>)
                return data.AudioListener.value();

            else if constexpr (std::is_same_v<T, ParticleSystemComponent>)
                return data.ParticleSystem.value();

            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)
                return data.DirectionalLight.value();

            else if constexpr (std::is_same_v<T, PointLightComponent>)
                return data.PointLight.value();

            else if constexpr (std::is_same_v<T, SpotLightComponent>)
                return data.SpotLight.value();

            else
                static_assert(
                    !sizeof(T),
                    "Unsupported NoJob component type.");
        }

        // ---------------------------------------------------------
        // AddComponent
        // ---------------------------------------------------------

        template<typename T, typename... Args>
        T& AddComponent(
            std::uint32_t handle,
            Args&&... args)
        {
            auto& data = m_Entities.at(handle);

            if constexpr (std::is_same_v<T, IDComponent>)
                return data.ID;

            else if constexpr (std::is_same_v<T, TagComponent>)
                return data.Tag;

            else if constexpr (std::is_same_v<T, TransformComponent>)
                return data.Transform;

            else if constexpr (std::is_same_v<T, RelationshipComponent>)
                return data.Relationship;

            else if constexpr (std::is_same_v<T, LayerComponent>)
                return data.Layer;

            else if constexpr (std::is_same_v<T, MeshComponent>)
            {
                data.Mesh.emplace(
                    T{ std::forward<Args>(args)... });

                return data.Mesh.value();
            }

            else if constexpr (std::is_same_v<T, AnimatorComponent>)
            {
                data.Animator.emplace(
                    T{ std::forward<Args>(args)... });

                return data.Animator.value();
            }

            else if constexpr (std::is_same_v<T, PrefabInstanceComponent>)
            {
                data.PrefabInstance.emplace(
                    T{ std::forward<Args>(args)... });

                return data.PrefabInstance.value();
            }

            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
            {
                data.MeshRenderer.emplace(
                    T{ std::forward<Args>(args)... });

                return data.MeshRenderer.value();
            }

            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
            {
                data.NativeScript.emplace(
                    T{ std::forward<Args>(args)... });

                return data.NativeScript.value();
            }

            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
            {
                data.Rigidbody.emplace(
                    T{ std::forward<Args>(args)... });

                return data.Rigidbody.value();
            }

            else if constexpr (std::is_same_v<T, NavAgentComponent>)
            {
                data.NavAgent.emplace(
                    T{ std::forward<Args>(args)... });

                return data.NavAgent.value();
            }
            else if constexpr (std::is_same_v<T, PerceptionComponent>)
            {
                data.Perception.emplace(
                    T{ std::forward<Args>(args)... });

                return data.Perception.value();
            }
            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
            {
                data.BoxCollider.emplace(
                    T{ std::forward<Args>(args)... });

                return data.BoxCollider.value();
            }

            else if constexpr (std::is_same_v<T, SphereColliderComponent>)
            {
                data.SphereCollider.emplace(
                    T{ std::forward<Args>(args)... });

                return data.SphereCollider.value();
            }

            else if constexpr (std::is_same_v<T, CapsuleColliderComponent>)
            {
                data.CapsuleCollider.emplace(
                    T{ std::forward<Args>(args)... });

                return data.CapsuleCollider.value();
            }

            else if constexpr (std::is_same_v<T, CameraComponent>)
            {
                data.Camera.emplace(
                    T{ std::forward<Args>(args)... });

                return data.Camera.value();
            }

            else if constexpr (std::is_same_v<T, AudioSourceComponent>)
            {
                data.AudioSource.emplace(
                    T{ std::forward<Args>(args)... });

                return data.AudioSource.value();
            }

            else if constexpr (std::is_same_v<T, AudioListenerComponent>)
            {
                data.AudioListener.emplace(
                    T{ std::forward<Args>(args)... });

                return data.AudioListener.value();
            }

            else if constexpr (std::is_same_v<T, ParticleSystemComponent>)
            {
                data.ParticleSystem.emplace(
                    T{ std::forward<Args>(args)... });

                return data.ParticleSystem.value();
            }

            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)
            {
                data.DirectionalLight.emplace(
                    T{ std::forward<Args>(args)... });

                return data.DirectionalLight.value();
            }

            else if constexpr (std::is_same_v<T, PointLightComponent>)
            {
                data.PointLight.emplace(
                    T{ std::forward<Args>(args)... });

                return data.PointLight.value();
            }

            else if constexpr (std::is_same_v<T, SpotLightComponent>)
            {
                data.SpotLight.emplace(
                    T{ std::forward<Args>(args)... });

                return data.SpotLight.value();
            }

            else
                static_assert(
                    !sizeof(T),
                    "Unsupported NoJob component type.");
        }

        // ---------------------------------------------------------
        // RemoveComponent
        // ---------------------------------------------------------

        template<typename T>
        void RemoveComponent(std::uint32_t handle)
        {
            auto& data = m_Entities.at(handle);

            if constexpr (std::is_same_v<T, MeshComponent>)
                data.Mesh.reset();

            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
                data.MeshRenderer.reset();

            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
                data.NativeScript.reset();

            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                data.Rigidbody.reset();

            else if constexpr (std::is_same_v<T, NavAgentComponent>)
            {
                data.NavAgent.reset();
                m_NavAgentStates.erase(handle);
            }
            else if constexpr (std::is_same_v<T, PerceptionComponent>)
            {
                data.Perception.reset();
                m_PerceptionStates.erase(handle);
            }
            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                data.BoxCollider.reset();

            else if constexpr (std::is_same_v<T, SphereColliderComponent>)
                data.SphereCollider.reset();

            else if constexpr (std::is_same_v<T, CapsuleColliderComponent>)
                data.CapsuleCollider.reset();

            else if constexpr (std::is_same_v<T, CameraComponent>)
                data.Camera.reset();

            else if constexpr (std::is_same_v<T, AudioSourceComponent>)
                data.AudioSource.reset();

            else if constexpr (std::is_same_v<T, AudioListenerComponent>)
                data.AudioListener.reset();

            else if constexpr (std::is_same_v<T, ParticleSystemComponent>)
                data.ParticleSystem.reset();

            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)
                data.DirectionalLight.reset();

            else if constexpr (std::is_same_v<T, PointLightComponent>)
                data.PointLight.reset();

            else if constexpr (std::is_same_v<T, SpotLightComponent>)
                data.SpotLight.reset();

            else if constexpr (std::is_same_v<T, AnimatorComponent>)
                data.Animator.reset();

            else
                static_assert(
                    !sizeof(T),
                    "This NoJob component cannot be removed.");
        }

        // ---------------------------------------------------------
        // HasComponent
        // ---------------------------------------------------------

        template<typename T>
        bool HasComponent(std::uint32_t handle) const
        {
            if (!IsValid(handle))
                return false;

            const auto& data =
                m_Entities.at(handle);

            if constexpr (
                std::is_same_v<T, IDComponent> ||
                std::is_same_v<T, TagComponent> ||
                std::is_same_v<T, TransformComponent> ||
                std::is_same_v<T, RelationshipComponent> ||
                std::is_same_v<T, LayerComponent>)
            {
                return true;
            }

            else if constexpr (std::is_same_v<T, MeshComponent>)
                return data.Mesh.has_value();

            else if constexpr (std::is_same_v<T, AnimatorComponent>)
                return data.Animator.has_value();

            else if constexpr (std::is_same_v<T, PrefabInstanceComponent>)
                return data.PrefabInstance.has_value();

            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
                return data.MeshRenderer.has_value();

            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
                return data.NativeScript.has_value();

            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                return data.Rigidbody.has_value();

            else if constexpr (std::is_same_v<T, NavAgentComponent>)
                return data.NavAgent.has_value();

            else if constexpr (std::is_same_v<T, PerceptionComponent>)
                return data.Perception.has_value();

            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                return data.BoxCollider.has_value();

            else if constexpr (std::is_same_v<T, SphereColliderComponent>)
                return data.SphereCollider.has_value();

            else if constexpr (std::is_same_v<T, CapsuleColliderComponent>)
                return data.CapsuleCollider.has_value();

            else if constexpr (std::is_same_v<T, CameraComponent>)
                return data.Camera.has_value();

            else if constexpr (std::is_same_v<T, AudioSourceComponent>)
                return data.AudioSource.has_value();

            else if constexpr (std::is_same_v<T, AudioListenerComponent>)
                return data.AudioListener.has_value();

            else if constexpr (std::is_same_v<T, ParticleSystemComponent>)
                return data.ParticleSystem.has_value();

            else if constexpr (std::is_same_v<T, DirectionalLightComponent>)
                return data.DirectionalLight.has_value();

            else if constexpr (std::is_same_v<T, PointLightComponent>)
                return data.PointLight.has_value();

            else if constexpr (std::is_same_v<T, SpotLightComponent>)
                return data.SpotLight.has_value();

            else
                return false;
        }

        // ---------------------------------------------------------
        // Scene state
        // ---------------------------------------------------------

        std::unordered_map<
            std::uint32_t,
            EntityData>
            m_Entities;

        std::uint32_t m_NextHandle = 1;
        std::uint64_t m_NextID = 1;

        float m_DeltaTime = 0.0f;
        bool m_RuntimeRunning = false;

        // Non-owning navigation world.
        NavigationSystem* m_NavigationSystem = nullptr;

        // ---------------------------------------------------------
        // Particle runtime state
        // ---------------------------------------------------------

        struct ParticleRuntimeState
        {
            std::vector<RuntimeParticle> Particles;

            float SpawnAccumulator = 0.0f;
            float Elapsed = 0.0f;

            std::uint32_t Seed = 1;
        };

        std::unordered_map<
            std::uint32_t,
            ParticleRuntimeState>
            m_ParticleStates;

        // ---------------------------------------------------------
        // NavAgent runtime state
        // ---------------------------------------------------------

        struct NavAgentRuntimeState
        {
            NavAgent Agent;

            glm::vec3 RequestedDestination{ 0.0f };
            bool HasRequestedDestination = false;

            std::uint64_t PathNavMeshVersion = 0;
            float RepathTimer = 0.0f;
            bool PathQueryFailed = false;
        };

        std::unordered_map<
            std::uint32_t,
            NavAgentRuntimeState>
            m_NavAgentStates;

        // AI Perception runtime state
        struct PerceptionTargetState
        {
            std::uint32_t EntityHandle = 0;

            glm::vec3 LastKnownPosition{ 0.0f };

            float TimeSinceLastSeen = 0.0f;

            bool IsVisible = false;
        };

        struct PerceptionRuntimeState
        {
            float UpdateTimer = 0.0f;

            std::unordered_map<
                std::uint32_t,
                PerceptionTargetState> Targets;
        };

        std::unordered_map<
            std::uint32_t,
            PerceptionRuntimeState> m_PerceptionStates;
        // ---------------------------------------------------------
        // Native scripting runtime state
        // ---------------------------------------------------------

        std::unordered_map<
            std::uint32_t,
            std::unique_ptr<ScriptableEntity>>
            m_ScriptInstances;

        void CreateScriptInstance(
            std::uint32_t handle);

        void DestroyScriptInstance(
            std::uint32_t handle);

        void UpdatePerception(float deltaTime);

        friend class Entity;

        };

    inline Entity::operator bool() const
    {
        return
            m_Scene != nullptr &&
            m_Scene->IsValid(m_Handle);
    }

    template<typename T, typename... Args>
    T& Entity::AddComponent(Args&&... args)
    {
        return m_Scene->AddComponent<T>(
            m_Handle,
            std::forward<Args>(args)...);
    }

    template<typename T>
    void Entity::RemoveComponent()
    {
        m_Scene->RemoveComponent<T>(
            m_Handle);
    }

    template<typename T>
    T& Entity::GetComponent()
    {
        return m_Scene->GetComponent<T>(
            m_Handle);
    }

    template<typename T>
    const T& Entity::GetComponent() const
    {
        return m_Scene->GetComponent<T>(
            m_Handle);
    }

    template<typename T>
    bool Entity::HasComponent() const
    {
        return
            m_Scene &&
            m_Scene->HasComponent<T>(
                m_Handle);
    }
}