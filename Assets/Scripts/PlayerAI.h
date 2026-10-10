#pragma once
#include "Engine/Scene/ScriptableEntity.h"
#include <glm/glm.hpp>
#include <random>

namespace NoJob {
class PlayerAI final : public Script {
public:
    float WanderSpeed = 3.0f;
    float FleeSpeed = 6.0f;
    float ArrivalDistance = 1.0f;
    float WaitDuration = 1.5f;
    float SafeDistance = 15.0f;
    float FleeRepathInterval = 0.7f;
    void OnCreate() override;
    void OnUpdate(float dt) override;
    void OnDestroy() override;
private:
    enum class State { Wander, Flee };
    State m_State = State::Wander;
    std::mt19937 m_Rng{std::random_device{}()};
    float m_WaitTimer = 0.0f;
    float m_RepathTimer = 0.0f;
    bool ChooseWanderDestination();
    void UpdateFlee(float dt, const glm::vec3& threat);
};
}
