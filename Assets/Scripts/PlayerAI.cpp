#include "PlayerAI.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/AI/Navigation/NavigationSystem.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace NoJob {
namespace {
float DistanceXZ(const glm::vec3& a, const glm::vec3& b) {
    return glm::length(glm::vec2(a.x-b.x, a.z-b.z));
}
}
void PlayerAI::OnCreate() {
    m_State = State::Wander;
    m_WaitTimer = 0.0f;
    m_RepathTimer = 0.0f;
    ChooseWanderDestination();
}
bool PlayerAI::ChooseWanderDestination() {
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
        agent.Speed = WanderSpeed;
        agent.StoppingDistance = ArrivalDistance;
        agent.Destination = poly->Center;
        agent.HasDestination = true;
        return true;
    }
    agent.HasDestination = false;
    return false;
}
void PlayerAI::UpdateFlee(float dt, const glm::vec3& threat) {
    Scene* scene = GetEntity().GetScene();
    if (!scene || !HasComponent<NavAgentComponent>()) return;
    auto* nav = scene->GetNavigationSystem();
    if (!nav || !nav->HasNavMesh()) return;
    auto& agent = GetComponent<NavAgentComponent>();
    agent.Enabled = true;
    agent.Speed = FleeSpeed;
    agent.StoppingDistance = ArrivalDistance;
    m_RepathTimer -= dt;
    if (m_RepathTimer > 0.0f && agent.HasDestination) return;
    m_RepathTimer = std::max(0.2f, FleeRepathInterval);
    const glm::vec3 origin = glm::vec3(scene->GetWorldTransform(GetEntity())[3]);
    const auto& mesh = nav->GetNavMesh();
    const size_t count = mesh.GetPolygonCount();
    if (!count) return;
    const float currentThreatDistance = DistanceXZ(origin, threat);
    std::uniform_int_distribution<size_t> pick(0, count - 1);
    // Cheap scoring first: only pathfind the best 3 sampled points.
    struct Candidate { glm::vec3 point; float score; };
    Candidate best[3]{};
    for (auto& candidate : best) candidate.score = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < 32; ++i) {
        const auto* poly = mesh.GetPolygon(static_cast<NavPolygonID>(pick(m_Rng)));
        if (!poly) continue;
        const glm::vec3 point = poly->Center;
        const float threatDistance = DistanceXZ(point, threat);
        const float travel = DistanceXZ(origin, point);
        if (travel < ArrivalDistance * 2.0f || threatDistance <= currentThreatDistance) continue;
        const float score = threatDistance - 0.3f * travel + (threatDistance >= SafeDistance ? 5.0f : 0.0f);
        for (int j = 0; j < 3; ++j) {
            if (score > best[j].score) {
                for (int k = 2; k > j; --k) best[k] = best[k-1];
                best[j] = {point, score};
                break;
            }
        }
    }
    for (const auto& candidate : best) {
        if (!std::isfinite(candidate.score)) continue;
        if (!nav->CalculatePath(origin, candidate.point).IsValid()) continue;
        agent.Destination = candidate.point;
        agent.HasDestination = true;
        return;
    }
    // Keep the previous valid destination if no new escape is found.
}
void PlayerAI::OnUpdate(float dt) {
    if (!HasComponent<NavAgentComponent>()) return;
    Scene* scene = GetEntity().GetScene();
    if (!scene) return;
    auto& agent = GetComponent<NavAgentComponent>();
    bool threatVisible = false;
    glm::vec3 threat{0.0f};
    if (HasComponent<PerceptionComponent>()) {
        for (const auto& target : scene->GetPerceivedTargets(GetEntity().GetHandle())) {
            if (!target.IsVisible) continue;
            threatVisible = true;
            threat = target.LastKnownPosition;
            break;
        }
    }
    if (threatVisible) {
        if (m_State != State::Flee) {
            m_State = State::Flee;
            m_RepathTimer = 0.0f;
            m_WaitTimer = 0.0f;
            agent.HasDestination = false;
        }
        UpdateFlee(dt, threat);
        return;
    }
    if (m_State == State::Flee) {
        m_State = State::Wander;
        m_WaitTimer = 0.0f;
        agent.HasDestination = false;
        agent.Speed = WanderSpeed;
        if (!ChooseWanderDestination()) m_WaitTimer = 1.0f;
        return;
    }
    if (agent.HasDestination) {
        const glm::vec3 position = glm::vec3(scene->GetWorldTransform(GetEntity())[3]);
        if (DistanceXZ(position, agent.Destination) <= ArrivalDistance) {
            agent.HasDestination = false;
            m_WaitTimer = WaitDuration;
        }
        return;
    }
    m_WaitTimer -= dt;
    if (m_WaitTimer <= 0.0f) {
        if (!ChooseWanderDestination()) m_WaitTimer = 1.0f;
    }
}
void PlayerAI::OnDestroy() {}
NOJOB_REGISTER_SCRIPT(PlayerAI, "AI",
    NOJOB_FIELD(PlayerAI, WanderSpeed),
    NOJOB_FIELD(PlayerAI, FleeSpeed),
    NOJOB_FIELD(PlayerAI, ArrivalDistance),
    NOJOB_FIELD(PlayerAI, WaitDuration),
    NOJOB_FIELD(PlayerAI, SafeDistance),
    NOJOB_FIELD(PlayerAI, FleeRepathInterval))
}
