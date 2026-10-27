/** @file RLObstacleAgent.h @brief Learned obstacle avoidance without a navigation fallback. */
#pragma once
#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/AI/Reinforcement/ObstacleArena.h"
#include "Engine/AI/Reinforcement/ObstacleWorldPolicy.h"
#include <optional>
namespace NoJob {
/** @brief Compose a learned core DQN with learned tracking on the open exterior.
 * @details Movement is interpolated between lattice cells. Solid collisions reject
 * a move and count as failed attempts; they never select another action or a route.
 */
class RLObstacleAgent final: public Script {
public:
    std::string PolicyName="RLObstaclePolicy"; ///< Eight-input/four-output learned asset.
    std::string TrackingPolicyName="RLTrackingPolicy"; ///< Learned open-floor axis controller.
    std::string TargetTag="RLTarget"; ///< Live goal, snapped to the nearest lattice cell.
    float ActionInterval=.4f; ///< Seconds per 1.6-unit interpolated cardinal action.
    /** @brief Validate the model and initial scene alignment. */
    void OnCreate()override;
    /** @brief Observe live colliders/goal, choose a neural action and animate that move. */
    void OnUpdate(float dt)override;
    /** @brief Release model and snapshots when Play stops. */
    void OnDestroy()override;
    /** @brief Copy actual observations, Q values and action outcomes across the DLL. */
    bool GetNeuralDebugSnapshot(NeuralAgent::Snapshot& out)const override;
private:
    std::optional<Neural::NeuralNetwork> model_,trackingModel_;
    RL::GridCell position_; ///< Logical cell, including the open exterior.
    NeuralAgent::Snapshot snapshot_;
    bool hasSnapshot_=false,hasGoal_=false,halted_=false;
    RL::GridCell goal_;
    glm::vec3 from_{0},to_{0};
    float animation_=0,duration_=.4f;
    int steps_=0,coreSteps_=0,blocked_=0,arrivals_=0;
};
}
