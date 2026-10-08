#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace NoJob
{
    class ISpatialQuery
    {
    public:
        virtual ~ISpatialQuery() = default;

        virtual bool HasObstacleBetween(
            const glm::vec3& origin,
            const glm::vec3& destination,
            std::uint64_t observerID,
            std::uint64_t targetID) const = 0;
    };
}