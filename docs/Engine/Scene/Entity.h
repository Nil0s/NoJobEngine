#pragma once
#include <cstdint>
#include <type_traits>

namespace NoJob
{
    class Scene;
    class PrefabSerializer;

    class Entity
    {
    public:
        Entity() = default;
        Entity(std::uint32_t handle, Scene* scene)
            : m_Handle(handle), m_Scene(scene)
        {
        }

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args);

        template<typename T>
        void RemoveComponent();

        template<typename T>
        T& GetComponent();

        template<typename T>
        const T& GetComponent() const;

        template<typename T>
        bool HasComponent() const;

        explicit operator bool() const;

        std::uint32_t GetHandle() const { return m_Handle; }
        Scene* GetScene() const { return m_Scene; }

        bool operator==(const Entity& other) const
        {
            return m_Handle == other.m_Handle && m_Scene == other.m_Scene;
        }

        bool operator!=(const Entity& other) const
        {
            return !(*this == other);
        }

    private:
        std::uint32_t m_Handle = 0;
        Scene* m_Scene = nullptr;

        friend class Scene;
        friend class PrefabSerializer;
    };
}
