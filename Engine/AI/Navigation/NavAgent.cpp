#include "Engine/AI/Navigation/NavAgent.h"

#include "Engine/AI/Navigation/NavigationSystem.h"

#include <algorithm>
#include <utility>

namespace NoJob
{
    bool NavAgent::SetDestination(
        const NavigationSystem& navigationSystem,
        const glm::vec3& currentPosition,
        const glm::vec3& destination)
    {
        NavPointPath newPath =
            navigationSystem.CalculatePath(
                currentPosition,
                destination);

        if (!newPath.IsValid())
        {
            Stop();
            return false;
        }

        m_Path = std::move(newPath);
        m_Destination = destination;

        m_CurrentWaypoint =
            m_Path.Points.size() > 1 ? 1 : 0;

        m_ReachedDestination = false;

        return true;
    }

    bool NavAgent::RecalculatePath(
        const NavigationSystem& navigationSystem,
        const glm::vec3& currentPosition)
    {
        if (m_ReachedDestination || !HasPath())
            return false;

        NavPointPath newPath =
            navigationSystem.CalculatePath(
                currentPosition,
                m_Destination);

        if (!newPath.IsValid())
            return false;

        // Commit only after successful path calculation.
        m_Path = std::move(newPath);

        m_CurrentWaypoint =
            m_Path.Points.size() > 1 ? 1 : 0;

        return true;
    }
    glm::vec3 NavAgent::Update(
        const glm::vec3& currentPosition,
        float deltaTime)
    {
        if (!HasPath() ||
            deltaTime <= 0.0f)
        {
            return glm::vec3(0.0f);
        }

        while (m_CurrentWaypoint <
            m_Path.Points.size())
        {
            const glm::vec3 target =
                m_Path.Points[
                    m_CurrentWaypoint];

            glm::vec3 toTarget =
                target -
                currentPosition;

            toTarget.y = 0.0f;

            const float distance =
                glm::length(toTarget);

            const bool isLastWaypoint =
                m_CurrentWaypoint + 1 >=
                m_Path.Points.size();

            // Intermediate waypoints must be reached accurately.
            // Using the final stopping distance here makes agents
            // cut corners and potentially collide with walls.
            constexpr float WaypointTolerance = 0.10f;
            const float tolerance = isLastWaypoint
                ? std::max(m_StoppingDistance, 0.0f)
                : WaypointTolerance;

            if (distance <= tolerance)
            {
                if (isLastWaypoint)
                {
                    m_CurrentWaypoint =
                        m_Path.Points.size();

                    m_ReachedDestination =
                        true;

                    return glm::vec3(0.0f);
                }

                ++m_CurrentWaypoint;
                continue;
            }

            const glm::vec3 direction =
                toTarget /
                distance;

            const float maxMovement =
                std::max(m_Speed, 0.0f) *
                deltaTime;

            const float movementDistance =
                std::min(
                    maxMovement,
                    distance);

            return
                direction *
                movementDistance;
        }

        m_ReachedDestination = true;

        return glm::vec3(0.0f);
    }

    void NavAgent::Stop()
    {
        m_Path.Clear();

        m_CurrentWaypoint = 0;
        m_ReachedDestination = true;
    }
}