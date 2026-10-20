#include "Engine/AI/Perception/PerceptionSensor.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace NoJob
{
    PerceptionSensor::PerceptionSensor(
        const PerceptionSettings& settings)
    {
        SetSettings(settings);
    }

    void PerceptionSensor::SetSettings(
        const PerceptionSettings& settings)
    {
        m_Settings = settings;

        m_Settings.DetectionRadius =
            std::max(0.0f, settings.DetectionRadius);

        m_Settings.FieldOfView =
            std::clamp(settings.FieldOfView, 0.0f, 360.0f);
    }

    const PerceptionSettings&
        PerceptionSensor::GetSettings() const
    {
        return m_Settings;
    }

    bool PerceptionSensor::IsWithinRange(
        const glm::vec3& observerPosition,
        const glm::vec3& targetPosition) const
    {
        const glm::vec3 offset =
            targetPosition - observerPosition;

        const float distanceSquared =
            glm::dot(offset, offset);

        const float radius =
            m_Settings.DetectionRadius;

        return distanceSquared <= radius * radius;
    }

    bool PerceptionSensor::IsWithinFieldOfView(
        const glm::vec3& observerPosition,
        const glm::vec3& forward,
        const glm::vec3& targetPosition) const
    {
        const glm::vec3 direction =
            targetPosition - observerPosition;

        const float directionLengthSquared =
            glm::dot(direction, direction);

        if (directionLengthSquared < 0.000001f)
            return true;

        const float forwardLengthSquared =
            glm::dot(forward, forward);

        if (forwardLengthSquared < 0.000001f)
            return false;

        if (m_Settings.FieldOfView >= 360.0f)
            return true;

        const glm::vec3 normalizedDirection =
            glm::normalize(direction);

        const glm::vec3 normalizedForward =
            glm::normalize(forward);

        constexpr float Pi = 3.14159265358979323846f;

        const float halfFovRadians =
            (m_Settings.FieldOfView * 0.5f)
            * (Pi / 180.0f);

        const float threshold =
            std::cos(halfFovRadians);

        return glm::dot(
            normalizedForward,
            normalizedDirection) >= threshold;
    }

    bool PerceptionSensor::CanDetect(
        const glm::vec3& observerPosition,
        const glm::vec3& forward,
        const glm::vec3& targetPosition) const
    {
        return IsWithinRange(
            observerPosition,
            targetPosition)
            &&
            IsWithinFieldOfView(
                observerPosition,
                forward,
                targetPosition);
    }
}