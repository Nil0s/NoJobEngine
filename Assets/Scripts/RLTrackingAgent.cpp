/** @file RLTrackingAgent.cpp @brief Scene-bound DQN inference and AI Debugger snapshot. */
#include "RLTrackingAgent.h"
#include "Engine/AI/Reinforcement/TrackingPolicy.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
namespace NoJob {
void RLTrackingAgent::OnCreate() {
    hasSnapshot_=false; reached_=false; accumulator_=0.f; steps_=0; successes_=0; model_.reset();
    try {
        model_.emplace(Neural::NeuralNetwork::Load(NeuralAssetPath(PolicyName).string()));
        if(model_->Layers().empty() || model_->Layers().front().Inputs()!=2 || model_->Layers().back().Outputs()!=3)
            throw std::runtime_error("RL tracking policy must have 2 inputs and 3 actions");
        SetAIDebugState("RL: policy ready");
    } catch(const std::exception& e) {
        std::fprintf(stderr,"[RLTrackingAgent] %s\n",e.what());
        model_.reset(); SetAIDebugState("RL: policy missing or invalid");
    }
}
void RLTrackingAgent::OnUpdate(float dt) {
    TickAIDebug(dt);
    if(!model_ || !std::isfinite(dt) || dt<=0.f || !HasComponent<TransformComponent>()) return;
    auto* scene=GetEntity().GetScene(); if(!scene) return;
    Entity target{};
    for(Entity e:scene->GetEntities()) {
        if(e==GetEntity() || !e.HasComponent<TagComponent>() || !e.HasComponent<TransformComponent>()) continue;
        if(e.GetComponent<TagComponent>().Tag==TargetTag) {target=e;break;}
    }
    if(!target) {hasSnapshot_=false; SetAIDebugState("RL: target missing");return;}
    accumulator_+=dt;
    if(accumulator_<std::max(.01f,ActionInterval)) return;
    accumulator_=0.f;
    auto& transform=GetComponent<TransformComponent>();
    const float scale=std::max(.01f,WorldScale);
    try {
        /** @brief Read both live target coordinates at every decision; never cache a goal. */
        const auto& goal=target.GetComponent<TransformComponent>().Position;
        const auto x=RL::TrackAxis(*model_,transform.Position.x/scale,goal.x/scale);
        const auto z=RL::TrackAxis(*model_,transform.Position.z/scale,goal.z/scale);
        transform.Position.x=x.next*scale;
        transform.Position.z=z.next*scale;
        ++steps_;
        const bool reached=std::abs(goal.x/scale-x.next)<.08f && std::abs(goal.z/scale-z.next)<.08f;
        if(reached && !reached_) ++successes_;
        reached_=reached;
        snapshot_.model=PolicyName;
        snapshot_.status="RL DQN: applied X="+std::to_string(x.action)+" Z="+std::to_string(z.action)+
            " (0:negative, 1:hold, 2:positive), arrivals="+std::to_string(successes_)+
            "; hold override X="+std::to_string(std::abs(x.observation[1])<.08f)+
            " Z="+std::to_string(std::abs(z.observation[1])<.08f)+
            ((std::abs(goal.x)>scale || std::abs(goal.z)>scale)?"; target outside bounds":"");
        snapshot_.inputs=x.observation;
        snapshot_.inputs.insert(snapshot_.inputs.end(),z.observation.begin(),z.observation.end());
        snapshot_.outputs=x.q;
        snapshot_.outputs.insert(snapshot_.outputs.end(),z.q.begin(),z.q.end());
        snapshot_.distance=std::hypot(goal.x-transform.Position.x,goal.z-transform.Position.z);
        // Independent passes are shown as values, not as a fictitious four-input network.
        snapshot_.activations.clear();
        hasSnapshot_=true;
        SetAIDebugState(reached?"RL: reached target":"RL: tracking X/Z");
        if(AutoReset && (reached || steps_>=40)) {
            transform.Position.x=-.8f*scale; transform.Position.z=-.8f*scale;
            steps_=0; reached_=false;
        }
    } catch(const std::exception& e) {
        std::fprintf(stderr,"[RLTrackingAgent] %s\n",e.what());
        hasSnapshot_=false; SetAIDebugState("RL: inference error");
    }
}
void RLTrackingAgent::OnDestroy() {model_.reset();hasSnapshot_=false;}
bool RLTrackingAgent::GetNeuralDebugSnapshot(NeuralAgent::Snapshot& out) const {
    if(!hasSnapshot_) return false;
    out=snapshot_;return true;
}
NOJOB_REGISTER_SCRIPT(RLTrackingAgent,"AI / Reinforcement Learning",
    NOJOB_FIELD(RLTrackingAgent,PolicyName),
    NOJOB_FIELD(RLTrackingAgent,TargetTag),
    NOJOB_FIELD(RLTrackingAgent,WorldScale),
    NOJOB_FIELD(RLTrackingAgent,ActionInterval),
    NOJOB_FIELD(RLTrackingAgent,AutoReset))
}
