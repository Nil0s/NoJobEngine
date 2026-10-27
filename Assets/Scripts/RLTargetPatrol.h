/** @file RLTargetPatrol.h @brief Automatic goal motion for the fixed-arena recording demo. */
#pragma once
#include "Engine/Scene/ScriptableEntity.h"
#include <array>
namespace NoJob {
/** @brief Move only the demo target along a fixed, wall-clear X/Z loop.
 * @details This scripted trajectory is presentation behavior, not a learned policy.
 * Disable AutoMove before Play to restore manual target placement. Stop/Play
 * restores the serialized target transform and restarts the initial delay.
 */
class RLTargetPatrol final : public Script {
public:
    bool AutoMove=true; ///< Enable automatic motion; false leaves the target untouched.
    float Speed=1.6f; ///< World units per second; the demo agent can move faster.
    float InitialDelay=8.f; ///< Initial hold in seconds, allowing the first agent arrival.
    float PauseSeconds=3.f; ///< Hold at each waypoint so arrivals remain visible.
    /** @brief Return the fixed free-space route at the target's world-space height. */
    static std::array<glm::vec3,5> Waypoints(float height);
    /** @brief Reset the route cursor and initial hold on each Play. */
    void OnCreate() override;
    /** @brief Interpolate target motion, preserving its world height, scale and rotation. */
    void OnUpdate(float dt) override;
private:
    size_t next_=1; ///< Serialized demo target starts at waypoint zero.
    float wait_=0.f;
};
}
