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
        Stop();

        NavPointPath pointPath =
            navigationSystem.CalculatePath(
                currentPosition,
                destination);

        if (!pointPath.IsValid())
            return false;

        m_Path =
            std::move(pointPath);

        m_Destination =
            destination;

        if (m_Path.Points.size() > 1)
            m_CurrentWaypoint = 1;
        else
            m_CurrentWaypoint = 0;

        m_ReachedDestination = false;

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

            if (distance <=
                m_StoppingDistance)
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
                m_Speed *
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