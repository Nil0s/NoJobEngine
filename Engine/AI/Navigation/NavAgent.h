#pragma once

#include "Engine/AI/Navigation/NavMeshFunnel.h"

#include <cstddef>

#include <glm/glm.hpp>

namespace NoJob
{
    class NavigationSystem;

    class NavAgent
    {
    public:
        bool SetDestination(
            const NavigationSystem& navigationSystem,
            const glm::vec3& currentPosition,
            const glm::vec3& destination);

        glm::vec3 Update(
            const glm::vec3& currentPosition,
            float deltaTime);

        void Stop();

        bool HasPath() const
        {
            return m_Path.IsValid() &&
                m_CurrentWaypoint <
                m_Path.Points.size();
        }

        bool HasReachedDestination() const
        {
            return m_ReachedDestination;
        }

        const glm::vec3& GetDestination() const
        {
            return m_Destination;
        }

        const NavPointPath& GetPath() const
        {
            return m_Path;
        }

        std::size_t GetCurrentWaypointIndex() const
        {
            return m_CurrentWaypoint;
        }

        float& Speed()
        {
            return m_Speed;
        }

        float Speed() const
        {
            return m_Speed;
        }

        float& StoppingDistance()
        {
            return m_StoppingDistance;
        }

        float StoppingDistance() const
        {
            return m_StoppingDistance;
        }

    private:
        NavPointPath m_Path;

        glm::vec3 m_Destination{ 0.0f };

        std::size_t m_CurrentWaypoint = 0;

        float m_Speed = 3.5f;
        float m_StoppingDistance = 0.1f;

        bool m_ReachedDestination = true;
    };
}