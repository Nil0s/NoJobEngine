#pragma once

#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"

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

    class Scene
    {
    public:
        Scene();
        Scene(const Scene& other);
        Scene& operator=(const Scene&) = delete;
        ~Scene();

        std::unique_ptr<Scene> Copy() const;

        Entity CreateEntity(const std::string& name = "Entity");
        void DestroyEntity(Entity entity);

        bool IsValid(std::uint32_t handle) const;
        std::vector<Entity> GetEntities();

        void OnRuntimeStart();
        void OnRuntimeStop();
        void OnUpdate(float deltaTime);
        float GetDeltaTime() const { return m_DeltaTime; }

        Entity GetParent(Entity entity);
        std::vector<Entity> GetChildren(Entity entity);
        bool IsDescendant(Entity possibleDescendant, Entity ancestor) const;

        // keepWorldTransform=true makes editor reparenting behave naturally:
        // the object does not jump when its parent changes.
        bool SetParent(
            Entity child,
            Entity parent,
            bool keepWorldTransform = true);

        void Unparent(
            Entity child,
            bool keepWorldTransform = true);

        glm::mat4 GetWorldTransform(Entity entity) const;
        void SetWorldTransform(Entity entity, const glm::mat4& worldTransform);

    private:
        struct EntityData
        {
            IDComponent ID;
            TagComponent Tag;
            TransformComponent Transform;
            RelationshipComponent Relationship;

            std::optional<MeshComponent> Mesh;
            std::optional<MeshRendererComponent> MeshRenderer;
            std::optional<NativeScriptComponent> NativeScript;
            std::optional<RigidbodyComponent> Rigidbody;
            std::optional<BoxColliderComponent> BoxCollider;
        };

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
            else if constexpr (std::is_same_v<T, MeshComponent>)
                return data.Mesh.value();
            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
                return data.MeshRenderer.value();
            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
                return data.NativeScript.value();
            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                return data.Rigidbody.value();
            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                return data.BoxCollider.value();
            else
                static_assert(!sizeof(T), "Unsupported NoJob component type.");
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
            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
                return data.MeshRenderer.value();
            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
                return data.NativeScript.value();
            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                return data.Rigidbody.value();
            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                return data.BoxCollider.value();
            else
                static_assert(!sizeof(T), "Unsupported NoJob component type.");
        }

        template<typename T, typename... Args>
        T& AddComponent(std::uint32_t handle, Args&&... args)
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
            else if constexpr (std::is_same_v<T, MeshComponent>)
            {
                data.Mesh.emplace(T{ std::forward<Args>(args)... });
                return data.Mesh.value();
            }
            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
            {
                data.MeshRenderer.emplace(T{ std::forward<Args>(args)... });
                return data.MeshRenderer.value();
            }
            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
            {
                data.NativeScript.emplace(T{ std::forward<Args>(args)... });
                return data.NativeScript.value();
            }
            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
            {
                data.Rigidbody.emplace(T{ std::forward<Args>(args)... });
                return data.Rigidbody.value();
            }
            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
            {
                data.BoxCollider.emplace(T{ std::forward<Args>(args)... });
                return data.BoxCollider.value();
            }
            else
                static_assert(!sizeof(T), "Unsupported NoJob component type.");
        }

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
            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                data.BoxCollider.reset();
            else
                static_assert(!sizeof(T), "This NoJob component cannot be removed.");
        }

        template<typename T>
        bool HasComponent(std::uint32_t handle) const
        {
            if (!IsValid(handle))
                return false;

            const auto& data = m_Entities.at(handle);

            if constexpr (std::is_same_v<T, IDComponent>
                || std::is_same_v<T, TagComponent>
                || std::is_same_v<T, TransformComponent>
                || std::is_same_v<T, RelationshipComponent>)
                return true;
            else if constexpr (std::is_same_v<T, MeshComponent>)
                return data.Mesh.has_value();
            else if constexpr (std::is_same_v<T, MeshRendererComponent>)
                return data.MeshRenderer.has_value();
            else if constexpr (std::is_same_v<T, NativeScriptComponent>)
                return data.NativeScript.has_value();
            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                return data.Rigidbody.has_value();
            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                return data.BoxCollider.has_value();
            else
                return false;
        }

        std::unordered_map<std::uint32_t, EntityData> m_Entities;
        std::uint32_t m_NextHandle = 1;
        std::uint64_t m_NextID = 1;
        float m_DeltaTime = 0.0f;
        bool m_RuntimeRunning = false;
        std::unordered_map<std::uint32_t, std::unique_ptr<ScriptableEntity>> m_ScriptInstances;
        void CreateScriptInstance(std::uint32_t handle);
        void DestroyScriptInstance(std::uint32_t handle);

        friend class Entity;
    };

    inline Entity::operator bool() const
    {
        return m_Scene != nullptr && m_Scene->IsValid(m_Handle);
    }

    template<typename T, typename... Args>
    T& Entity::AddComponent(Args&&... args)
    {
        return m_Scene->AddComponent<T>(
            m_Handle, std::forward<Args>(args)...);
    }

    template<typename T>
    void Entity::RemoveComponent()
    {
        m_Scene->RemoveComponent<T>(m_Handle);
    }

    template<typename T>
    T& Entity::GetComponent()
    {
        return m_Scene->GetComponent<T>(m_Handle);
    }

    template<typename T>
    const T& Entity::GetComponent() const
    {
        return m_Scene->GetComponent<T>(m_Handle);
    }

    template<typename T>
    bool Entity::HasComponent() const
    {
        return m_Scene && m_Scene->HasComponent<T>(m_Handle);
    }
}
