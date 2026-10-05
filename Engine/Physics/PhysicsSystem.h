#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace NoJob
{
    class Scene;
    class Entity;

    struct RaycastHit
    {
        std::uint64_t EntityID = 0;
        glm::vec3 Point{ 0.0f };
        glm::vec3 Normal{ 0.0f, 1.0f, 0.0f };
        float Distance = 0.0f;
    };

    struct TriggerEvent
    {
        enum class Type { Enter, Exit };
        Type EventType = Type::Enter;
        std::uint64_t TriggerEntityID = 0;
        std::uint64_t OtherEntityID = 0;
    };

    class PhysicsSystem
    {
    public:
        PhysicsSystem();
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&) = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;

        void Start(Scene& scene);
        void Stop();
        void Update(float deltaTime);
        bool IsRunning() const { return m_Running; }

        bool AddForce(std::uint64_t entityID, const glm::vec3& force);
        bool AddImpulse(std::uint64_t entityID, const glm::vec3& impulse);
        bool SetLinearVelocity(std::uint64_t entityID, const glm::vec3& velocity);
        glm::vec3 GetLinearVelocity(std::uint64_t entityID) const;

        std::optional<RaycastHit> Raycast(
            const glm::vec3& origin,
            const glm::vec3& direction,
            float maxDistance = 1000.0f) const;

        const std::vector<TriggerEvent>& GetTriggerEvents() const
        {
            return m_TriggerEvents;
        }

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_Impl;
        Scene* m_Scene = nullptr;
        bool m_Running = false;
        std::vector<TriggerEvent> m_TriggerEvents;

        void CreateBodies();
        void SyncTransformsFromPhysics();
    };
}
