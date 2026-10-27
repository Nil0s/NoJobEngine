/** @file NavTargetTracking.h @brief Scene-bound destination tracking on a baked NavMesh. */
#pragma once
#include "Engine/AI/Navigation/NavigationSystem.h"
#include "Engine/Scene/Scene.h"
#include <cmath>
#include <algorithm>
#include <cstddef>
namespace NoJob {
/** @brief Requests native NavAgent movement; never writes the entity transform.
 * @details Invalid targets immediately cancel the old destination. Failed queries
 * are retried at the component's repath interval; rebakes invalidate the cache.
 */
class NavTargetTracker {
public:
    /** @brief Result of the latest navigation request, exposed to the AI Debugger. */
    enum class State { MissingAgent, MissingBake, InvalidPosition, NoRoute, Following, Arrived };
    /** @brief Reset cached navigation decisions after creation or hot reload. */
    void Reset() { initialized_=false; reachable_=false; timer_=0.f; waypoints_=0; }
    /** @brief Refresh the live goal and submit a validated native navigation destination. */
    State Update(Scene& scene, Entity entity, const glm::vec3& goal, float dt) {
        if(!entity.HasComponent<NavAgentComponent>()) return State::MissingAgent;
        auto& agent=entity.GetComponent<NavAgentComponent>();
        auto stop=[&](State s) { agent.HasDestination=false; reachable_=false; waypoints_=0; return s; };
        auto* nav=scene.GetNavigationSystem();
        if(!nav || !nav->HasNavMesh()) { Reset(); return stop(State::MissingBake); }
        const glm::vec3 origin(scene.GetWorldTransform(entity)[3]);
        if(!Finite(origin) || !Finite(goal)) { Reset(); return stop(State::InvalidPosition); }
        glm::vec3 flatGoal=goal; flatGoal.y=origin.y;
        timer_+=std::max(0.f,dt);
        const bool changed=!initialized_ || Distance(goal,lastGoal_)>.01f || version_!=nav->GetNavMeshVersion();
        if(changed || timer_>=std::max(.05f,agent.RepathInterval)) {
            initialized_=true; lastGoal_=goal; version_=nav->GetNavMeshVersion(); timer_=0.f;
            const auto path=nav->CalculatePath(origin,flatGoal);
            reachable_=path.IsValid(); waypoints_=path.Points.size();
        }
        if(!reachable_) return stop(State::NoRoute);
        agent.Enabled=true;
        if(Distance(origin,goal)<=std::max(0.f,agent.StoppingDistance)) {
            agent.HasDestination=false; return State::Arrived;
        }
        agent.Destination=flatGoal;
        agent.HasDestination=true;
        return State::Following;
    }
    /** @brief Number of points in the most recently validated route. */
    size_t Waypoints() const { return waypoints_; }
    /** @brief Human-readable status; all navigation diagnostics are in English. */
    static const char* Label(State state) {
        switch(state) {
            case State::MissingAgent: return "NAV: NavAgent component required";
            case State::MissingBake: return "NAV: bake required";
            case State::InvalidPosition: return "NAV: invalid position";
            case State::NoRoute: return "NAV: no reachable route";
            case State::Following: return "NAV: following baked route";
            case State::Arrived: return "NAV: reached target";
        }
        return "NAV: unknown";
    }
private:
    /** @brief Test all coordinates before invoking polygon queries. */
    static bool Finite(const glm::vec3& p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
    /** @brief Horizontal world-space distance, independent of visual height. */
    static float Distance(const glm::vec3& a,const glm::vec3& b) { return std::hypot(a.x-b.x,a.z-b.z); }
    glm::vec3 lastGoal_{0.f};
    std::uint64_t version_=0;
    float timer_=0.f;
    bool initialized_=false,reachable_=false;
    size_t waypoints_=0;
};
}
