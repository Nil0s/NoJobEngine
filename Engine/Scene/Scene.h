#pragma once

#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NoJob
{
    class Scene
    {
    public:
        Entity CreateEntity(const std::string& name = "Entity");
        void DestroyEntity(Entity entity);

        bool IsValid(std::uint32_t handle) const;
        std::vector<Entity> GetEntities();

    private:
        struct EntityData
        {
            IDComponent ID;
            TagComponent Tag;
            TransformComponent Transform;
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
            else
                static_assert(!sizeof(T), "Unsupported NoJob component type.");
        }

        template<typename T, typename... Args>
        T& AddComponent(std::uint32_t handle, Args&&...)
        {
            return GetComponent<T>(handle);
        }

        template<typename T>
        bool HasComponent(std::uint32_t handle) const
        {
            if (!IsValid(handle))
                return false;

            return std::is_same_v<T, IDComponent>
                || std::is_same_v<T, TagComponent>
                || std::is_same_v<T, TransformComponent>;
        }

        std::unordered_map<std::uint32_t, EntityData> m_Entities;
        std::uint32_t m_NextHandle = 1;
        std::uint64_t m_NextID = 1;

        friend class Entity;
    };

    template<typename T, typename... Args>
    T& Entity::AddComponent(Args&&... args)
    {
        return m_Scene->AddComponent<T>(
            m_Handle, std::forward<Args>(args)...);
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
