#pragma once
#include "Engine/Scene/ScriptableEntity.h"
#include <glm/glm.hpp>
#include <random>
#include <cstdint>

namespace NoJob {
class GuardAI final : public Script {
public:
    float PatrolSpeed = 2.5f;
    float ChaseSpeed = 4.5f;
    float ChaseStoppingDistance = 1.5f;
    float ArrivalDistance = 1.0f;
    float PatrolWait = 1.0f;
    float SearchDuration = 4.0f;
    float RepathInterval = 0.35f;
    void OnCreate() override;
    void OnUpdate(float dt) override;
    void OnDestroy() override;
private:
    enum class State { Patrol, Chase, Search };
    State m_State = State::Patrol;
    std::mt19937 m_Rng{std::random_device{}()};
    float m_Timer = 0.0f;
    float m_Repath = 0.0f;
    glm::vec3 m_LastSeen{0.0f};
    bool m_HasLastSeen = false;
    bool ChoosePatrolDestination();
};
}
