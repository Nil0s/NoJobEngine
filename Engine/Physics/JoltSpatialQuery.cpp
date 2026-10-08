#include "Engine/Physics/JoltSpatialQuery.h"
#include "Engine/Physics/PhysicsSystem.h"

namespace NoJob
{
    bool JoltSpatialQuery::HasObstacleBetween(
        const glm::vec3& origin,
        const glm::vec3& destination,
        std::uint64_t observerID,
        std::uint64_t targetID) const
    {
        return m_Physics.HasObstacleBetween(
            origin,
            destination,
            observerID,
            targetID
        );
    }
}