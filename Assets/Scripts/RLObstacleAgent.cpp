/** @file RLObstacleAgent.cpp @brief Explicit composition of learned core and exterior actors. */
#include "RLObstacleAgent.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include "Engine/Scene/ScriptRegistry.h"
#include <cstdio>
namespace NoJob {
void RLObstacleAgent::OnCreate(){
    hasSnapshot_=hasGoal_=halted_=false;animation_=0;steps_=coreSteps_=blocked_=arrivals_=0;model_.reset();trackingModel_.reset();
    try{
        model_=Neural::NeuralNetwork::Load(NeuralAssetPath(PolicyName).string());
        trackingModel_=Neural::NeuralNetwork::Load(NeuralAssetPath(TrackingPolicyName).string());
        if(model_->Layers().empty()||model_->Layers().front().Inputs()!=8||model_->Layers().back().Outputs()!=4)
            throw std::runtime_error("Obstacle policy must have 8 inputs and 4 Q outputs");
        if(trackingModel_->Layers().empty()||trackingModel_->Layers().front().Inputs()!=2||trackingModel_->Layers().back().Outputs()!=3)
            throw std::runtime_error("Exterior tracking policy must have 2 inputs and 3 Q outputs");
        auto* scene=GetEntity().GetScene();const glm::vec3 p(scene->GetWorldTransform(GetEntity())[3]);
        if(!std::isfinite(p.x)||!std::isfinite(p.z))throw std::runtime_error("Non-finite start");
        position_={int(std::lround(p.x/1.6f))+4,int(std::lround(p.z/1.6f))+4};
        if(!RL::InLearnedWorld(position_)||(RL::IsObstacleCore(position_)&&RL::ObstacleEnvironment::Blocked(position_))||glm::length(p-RL::CellWorld(position_,p.y))>.01f)
            throw std::runtime_error("Agent must start at a free cell centre inside [-48,48]");
        if(HasComponent<NavAgentComponent>()){auto&n=GetComponent<NavAgentComponent>();n.HasDestination=false;n.Enabled=false;}
        SetAIDebugState("RL obstacle: learned core/exterior ready (no NavMesh)");
    }catch(const std::exception&e){std::fprintf(stderr,"[RLObstacleAgent] %s\n",e.what());model_.reset();trackingModel_.reset();SetAIDebugState("RL obstacle: invalid model or start");}
}
void RLObstacleAgent::OnUpdate(float dt){
    if(!std::isfinite(dt)||dt<=0)return;TickAIDebug(dt);
    if(!model_||!trackingModel_||halted_)return;
    auto* scene=GetEntity().GetScene();if(!scene)return;
    if(!RL::MatchesObstacleArena(*scene)){halted_=true;animation_=0;hasSnapshot_=false;SetAIDebugState("RL obstacle: unsupported arena edit; restart after restoring walls");return;}
    if(animation_>0){
        animation_=std::max(0.f,animation_-dt);auto world=scene->GetWorldTransform(GetEntity());
        world[3]=glm::vec4(from_+(to_-from_)*(1.f-animation_/duration_),1);scene->SetWorldTransform(GetEntity(),world);
        if(animation_>0)return;
    }
    Entity target;
    for(auto e:scene->GetEntities())if(e!=GetEntity()&&e.HasComponent<TagComponent>()&&e.HasComponent<TransformComponent>()&&e.GetComponent<TagComponent>().Tag==TargetTag){target=e;break;}
    if(!target){hasSnapshot_=false;hasGoal_=false;SetAIDebugState("RL obstacle: target missing");return;}
    const glm::vec3 targetWorld(scene->GetWorldTransform(target)[3]);
    if(!std::isfinite(targetWorld.x)||!std::isfinite(targetWorld.z)||std::abs(targetWorld.x)>RL::LearnedWorldLimit+.001f||std::abs(targetWorld.z)>RL::LearnedWorldLimit+.001f){
        hasSnapshot_=false;hasGoal_=false;SetAIDebugState("RL obstacle: goal outside floor margin [-48,48]");return;
    }
    const RL::GridCell goal{int(std::lround(targetWorld.x/1.6f))+4,int(std::lround(targetWorld.z/1.6f))+4};
    if(RL::IsObstacleCore(goal)&&RL::SceneCellBlocked(*scene,goal)){hasSnapshot_=false;hasGoal_=false;SetAIDebugState("RL obstacle: blocked goal cell");return;}
    if(!hasGoal_||!(goal==goal_)){goal_=goal;hasGoal_=true;steps_=coreSteps_=blocked_=0;}
    const glm::vec3 current(scene->GetWorldTransform(GetEntity())[3]);
    if(hasSnapshot_)snapshot_.distance=std::hypot(targetWorld.x-current.x,targetWorld.z-current.z);
    if(position_==goal){snapshot_.status="True goal cell reached; last decision snapshot; arrivals="+std::to_string(arrivals_);SetAIDebugState("RL obstacle: reached true goal cell");return;}
    if(steps_>=RL::WorldActionBudget){snapshot_.status="World action budget exhausted; last decision snapshot";SetAIDebugState("RL obstacle: world step limit; move goal to retry");return;}
    try{
        if(glm::length(current-RL::CellWorld(position_,current.y))>.02f){halted_=true;hasSnapshot_=false;SetAIDebugState("RL obstacle: agent moved off lattice; restart scene");return;}
        const auto decision=RL::DecideObstacleWorld(*model_,*trackingModel_,position_,goal,[&](RL::GridCell p){return RL::SceneCellBlocked(*scene,p);});
        if(!decision.exterior&&coreSteps_>=RL::ObstacleEnvironment::Budget){snapshot_.status="Core policy action budget exhausted; last decision snapshot";SetAIDebugState("RL obstacle: core step limit; move goal to retry");return;}
        ++steps_;if(decision.exterior)coreSteps_=0;else ++coreSteps_;
        const auto before=position_;auto next=decision.next;
        bool blocked=decision.blocked;
        const auto destination=RL::CellWorld(next,current.y);
        if((RL::IsObstacleCore(next)&&RL::SceneCellBlocked(*scene,next))||!RL::ArenaSegmentClear(current,destination)){next=before;blocked=true;}
        if(blocked)++blocked_;position_=next;
        duration_=std::isfinite(ActionInterval)?std::max(.05f,ActionInterval):.4f;animation_=duration_;
        from_=current;to_=RL::CellWorld(position_,current.y);if(position_==goal)++arrivals_;
        snapshot_.model=decision.exterior?TrackingPolicyName:PolicyName;
        snapshot_.inputs=decision.observations;snapshot_.outputs=decision.q;
        snapshot_.activations=decision.exterior?std::vector<std::vector<float>>{}:NeuralAgent::Activations(*model_,decision.observations);
        snapshot_.distance=std::hypot(targetWorld.x-from_.x,targetWorld.z-from_.z);
        snapshot_.status=std::string(decision.exterior?"EXTERIOR learned axis tracking; ":"CORE learned obstacle DQN; ")+"action="+std::to_string(decision.action)+
            "; X="+std::to_string(decision.xAction)+" Z="+std::to_string(decision.zAction)+"; blocked="+std::to_string(blocked)+
            "; attempts="+std::to_string(blocked_)+"; world step="+std::to_string(steps_)+"/192; arrivals="+std::to_string(arrivals_)+
            "; explicit controller handoff, no NavMesh; true goal snapped to 1.6-unit grid";
        hasSnapshot_=true;SetAIDebugState(blocked?"RL obstacle: blocked learned action":decision.exterior?"RL obstacle: exterior learned tracking":"RL obstacle: core learned movement");
    }catch(const std::exception&e){hasSnapshot_=false;halted_=true;std::fprintf(stderr,"[RLObstacleAgent] %s\n",e.what());SetAIDebugState("RL obstacle: inference error");}
}
void RLObstacleAgent::OnDestroy(){model_.reset();trackingModel_.reset();hasSnapshot_=false;}
bool RLObstacleAgent::GetNeuralDebugSnapshot(NeuralAgent::Snapshot& out)const{if(!hasSnapshot_)return false;out=snapshot_;return true;}
NOJOB_REGISTER_SCRIPT(RLObstacleAgent,"AI / Reinforcement Learning",
    NOJOB_FIELD(RLObstacleAgent,PolicyName),NOJOB_FIELD(RLObstacleAgent,TrackingPolicyName),NOJOB_FIELD(RLObstacleAgent,TargetTag),NOJOB_FIELD(RLObstacleAgent,ActionInterval))
}
