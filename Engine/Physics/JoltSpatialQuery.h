#pragma once

#include "Engine/AI/Perception/ISpatialQuery.h"

namespace NoJob
{
    class PhysicsSystem;

    class JoltSpatialQuery final : public ISpatialQuery
    {
    public:
        explicit JoltSpatialQuery(const PhysicsSystem& physics)
            : m_Physics(physics)
        {
        }

        bool HasObstacleBetween(
            const glm::vec3& origin,
            const glm::vec3& destination,
            std::uint64_t observerID,
            std::uint64_t targetID) const override;

    private:
        const PhysicsSystem& m_Physics;
    };
}