/** @file RLTargetPatrol.cpp @brief Repeatable target motion without manual recording input. */
#include "RLTargetPatrol.h"
#include "Engine/AI/Reinforcement/ObstacleArena.h"
#include "Engine/Scene/ScriptRegistry.h"
#include <algorithm>
#include <cmath>
namespace NoJob {
std::array<glm::vec3,5> RLTargetPatrol::Waypoints(float y) {
    return {{{6.4f,y,0.f},{6.4f,y,6.4f},{-6.4f,y,6.4f},
             {-6.4f,y,-6.4f},{6.4f,y,-6.4f}}};
}
void RLTargetPatrol::OnCreate() {
    next_=1;
    wait_=std::isfinite(InitialDelay)?std::max(0.f,InitialDelay):8.f;
    SetAIDebugState("Demo target: initial hold");
}
void RLTargetPatrol::OnUpdate(float dt) {
    if(!AutoMove || !std::isfinite(dt) || dt<=0.f) return;
    if(!std::isfinite(Speed) || Speed<=0.f || !std::isfinite(PauseSeconds)) return;
    auto* scene=GetEntity().GetScene();
    if(!scene || !HasComponent<TransformComponent>()) return;
    TickAIDebug(dt);
    // Limit a stalled frame so the recording does not skip the entire trajectory.
    float remaining=std::min(dt,.25f);
    for(int transitions=0; remaining>0.f && transitions<16; ++transitions) {
        if(wait_>0.f) {
            const float held=std::min(wait_,remaining);
            wait_-=held; remaining-=held;
            if(remaining<=0.f) return;
        }
        auto world=scene->GetWorldTransform(GetEntity());
        const glm::vec3 from(world[3]);
        const auto points=Waypoints(from.y);
        const auto destination=points[next_];
        const float distance=glm::length(destination-from);
        if(!std::isfinite(distance)) return;
        const float travel=std::min(distance,Speed*remaining);
        const auto to=distance>1e-5f?from+(destination-from)*(travel/distance):destination;
        if(!RL::ArenaSegmentClear(from,to)) {
            SetAIDebugState("Demo target: path obstructed; disable AutoMove for manual placement");
            return;
        }
        world[3]=glm::vec4(to,1.f);
        scene->SetWorldTransform(GetEntity(),world);
        remaining=std::max(0.f,remaining-travel/Speed);
        if(travel<distance) { SetAIDebugState("Demo target: automatic X/Z motion");return; }
        next_=(next_+1)%points.size();
        wait_=std::max(0.f,PauseSeconds);
        SetAIDebugState("Demo target: waypoint hold");
    }
}
NOJOB_REGISTER_SCRIPT(RLTargetPatrol,"AI / Reinforcement Learning",
    NOJOB_FIELD(RLTargetPatrol,AutoMove),NOJOB_FIELD(RLTargetPatrol,Speed),
    NOJOB_FIELD(RLTargetPatrol,InitialDelay),NOJOB_FIELD(RLTargetPatrol,PauseSeconds))
}
