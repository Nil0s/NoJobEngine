#pragma once

#include <glm/glm.hpp>

namespace NoJob
{
    struct PerceptionSettings
    {
        float DetectionRadius = 10.0f;
        float FieldOfView = 120.0f; // Degrees
    };

    class PerceptionSensor
    {
    public:
        explicit PerceptionSensor(
            const PerceptionSettings& settings = {});

        void SetSettings(const PerceptionSettings& settings);

        const PerceptionSettings& GetSettings() const;

        bool IsWithinRange(
            const glm::vec3& observerPosition,
            const glm::vec3& targetPosition) const;

        bool IsWithinFieldOfView(
            const glm::vec3& observerPosition,
            const glm::vec3& forward,
            const glm::vec3& targetPosition) const;

        bool CanDetect(
            const glm::vec3& observerPosition,
            const glm::vec3& forward,
            const glm::vec3& targetPosition) const;

    private:
        PerceptionSettings m_Settings;
    };
}
