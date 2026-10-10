#include "GuardAI.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/AI/Navigation/NavigationSystem.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace NoJob {
namespace {
float DistanceXZ(const glm::vec3& a, const glm::vec3& b) {
    return glm::length(glm::vec2(a.x-b.x, a.z-b.z));
}
}
void GuardAI::OnCreate() {
    m_State = State::Patrol;
    m_Timer = 0.0f;
    m_Repath = 0.0f;
    m_HasLastSeen = false;
    ChoosePatrolDestination();
}
bool GuardAI::ChoosePatrolDestination() {
    Scene* scene = GetEntity().GetScene();
    if (!scene || !HasComponent<NavAgentComponent>()) return false;
    auto* nav = scene->GetNavigationSystem();
    if (!nav || !nav->HasNavMesh()) return false;
    auto& agent = GetComponent<NavAgentComponent>();
    const auto& mesh = nav->GetNavMesh();
    const size_t count = mesh.GetPolygonCount();
    if (!count) return false;
    const glm::vec3 origin = glm::vec3(scene->GetWorldTransform(GetEntity())[3]);
    std::uniform_int_distribution<size_t> pick(0, count - 1);
    for (int n = 0; n < 8; ++n) {
        const auto* poly = mesh.GetPolygon(static_cast<NavPolygonID>(pick(m_Rng)));
        if (!poly || DistanceXZ(origin, poly->Center) < 5.0f) continue;
        if (!nav->CalculatePath(origin, poly->Center).IsValid()) continue;
        agent.Enabled = true;
        agent.Speed = PatrolSpeed;
        agent.StoppingDistance = ArrivalDistance;
        agent.Destination = poly->Center;
        agent.HasDestination = true;
        return true;
    }
    agent.HasDestination = false;
    return false;
}
void GuardAI::OnUpdate(float dt) {
    if (!HasComponent<NavAgentComponent>()) return;
    Scene* scene = GetEntity().GetScene();
    if (!scene) return;
    auto& agent = GetComponent<NavAgentComponent>();
    const glm::vec3 position = glm::vec3(scene->GetWorldTransform(GetEntity())[3]);
    bool visible = false;
    glm::vec3 seenPosition{0.0f};
    if (HasComponent<PerceptionComponent>()) {
        for (const auto& target : scene->GetPerceivedTargets(GetEntity().GetHandle())) {
            if (!target.IsVisible) continue;
            visible = true;
            seenPosition = target.LastKnownPosition;
            break;
        }
    }
    if (visible) {
        if (m_State != State::Chase) m_Repath = 0.0f;
        m_State = State::Chase;
        m_LastSeen = seenPosition;
        m_HasLastSeen = true;
        agent.Enabled = true;
        agent.Speed = ChaseSpeed;
        agent.StoppingDistance = ChaseStoppingDistance;
        m_Repath -= dt;
        if (m_Repath <= 0.0f || !agent.HasDestination) {
            agent.Destination = seenPosition;
            agent.HasDestination = true;
            m_Repath = std::max(0.15f, RepathInterval);
        }
        return;
    }
    if (m_State == State::Chase) {
        m_State = State::Search;
        m_Timer = SearchDuration;
        agent.Speed = PatrolSpeed;
        agent.StoppingDistance = ArrivalDistance;
        if (m_HasLastSeen) {
            agent.Destination = m_LastSeen;
            agent.HasDestination = true;
        }
    }
    if (m_State == State::Search) {
        m_Timer -= dt;
        if (m_Timer > 0.0f) return;
        m_State = State::Patrol;
        m_Timer = 0.0f;
        agent.HasDestination = false;
    }
    // Patrol. Do not run pathfinding every frame.
    if (agent.HasDestination && DistanceXZ(position, agent.Destination) <= ArrivalDistance) {
        agent.HasDestination = false;
        m_Timer = PatrolWait;
    }
    if (!agent.HasDestination) {
        m_Timer -= dt;
        if (m_Timer <= 0.0f) {
            if (!ChoosePatrolDestination()) m_Timer = 1.0f;
        }
    }
}
void GuardAI::OnDestroy() {}
NOJOB_REGISTER_SCRIPT(GuardAI, "AI",
    NOJOB_FIELD(GuardAI, PatrolSpeed),
    NOJOB_FIELD(GuardAI, ChaseSpeed),
    NOJOB_FIELD(GuardAI, ChaseStoppingDistance),
    NOJOB_FIELD(GuardAI, ArrivalDistance),
    NOJOB_FIELD(GuardAI, PatrolWait),
    NOJOB_FIELD(GuardAI, SearchDuration),
    NOJOB_FIELD(GuardAI, RepathInterval))
}
