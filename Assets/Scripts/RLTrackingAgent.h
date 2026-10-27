/** @file RLTrackingAgent.h @brief Runtime inference demo for a native DQN-trained cube. @ingroup ReinforcementLearning */
#pragma once
#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/AI/Neural/NeuralNetwork.h"
#include "Engine/AI/Navigation/NavTargetTracking.h"
#include <optional>
#include <string>
namespace NoJob {
/** @brief Drives a cube along X/Z with a shared axis policy using the same observations and actions as TrackingEnvironment.
 * In navigation mode, NavAgent owns movement and neural proposals are advisory.
 * AutoReset only applies to the original direct-movement mode.
 * The project script performs inference only; training is performed in the RL editor panel.
 * TrackingLearningProof compares untrained, trained and random policies on held-out X/Z tasks.
 */
class RLTrackingAgent final : public Script {
public:
    std::string PolicyName = "RLTrackingPolicy"; ///< Name of an Assets/Neural/*.njn policy.
    std::string TargetTag = "RLTarget"; ///< Entity tag used as target.
    float WorldScale = 8.f; ///< Maps normalized [-1,1] simulation positions to world units.
    float ActionInterval = 0.15f; ///< Seconds per neural decision; diagnostic refresh interval in navigation mode.
    bool UseNavigation = false; ///< Delegate movement to NavAgent; neural outputs become advisory.
    bool AutoReset = false; ///< Restart when the cube reaches the target.
    /** @brief Load and validate the axis policy. */
    void OnCreate() override;
    /** @brief Observe the live target and apply one decision to each axis. */
    void OnUpdate(float dt) override;
    /** @brief Release the policy and invalidate diagnostic data. */
    void OnDestroy() override;
    /** @brief Copy observations, raw Q values and applied actions across the script DLL. */
    bool GetNeuralDebugSnapshot(NeuralAgent::Snapshot& out) const override;
private:
    NavTargetTracker navigator_; ///< Cached validation of native navigation destinations.
    std::optional<Neural::NeuralNetwork> model_;
    NeuralAgent::Snapshot snapshot_;
    bool hasSnapshot_ = false;
    float accumulator_ = 0.f;
    int steps_ = 0;
    int successes_ = 0;
    bool reached_ = false; ///< Counts arrivals once until the target leaves the radius.
};
}
