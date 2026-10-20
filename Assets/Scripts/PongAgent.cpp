#include "PongAgent.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>

namespace NoJob {
void PongAgent::OnCreate() {
    m_Model.reset(); m_HasSnapshot=false; m_HasLastBall=false;
    SetAIDebugState("Pong: initializing");
    try {
        m_Model.emplace(Neural::NeuralNetwork::Load(NeuralAssetPath(ActiveModel).string()));
        if (m_Model->Layers().empty() || m_Model->Layers().front().Inputs()!=6 ||
            m_Model->Layers().back().Outputs()!=1)
            throw std::runtime_error("PongAgent expects a 6-input / 1-output model");
        SetAIDebugState("Pong: model loaded");
    } catch(const std::exception& e) {
        std::fprintf(stderr,"[PongAgent] %s\n",e.what());
        m_Model.reset(); SetAIDebugState("Pong: model error");
    }
}
void PongAgent::OnUpdate(float dt) {
    TickAIDebug(dt);
    if (!m_Model || dt<=0.f || !HasComponent<TransformComponent>()) return;
    auto* scene=GetEntity().GetScene(); if(!scene) return;
    Entity ball{};
    for (Entity e : scene->GetEntities()) {
        if (e==GetEntity() || !e.HasComponent<TagComponent>() || !e.HasComponent<TransformComponent>()) continue;
        if (e.GetComponent<TagComponent>().Tag==TargetTag) {ball=e;break;}
    }
    if(!ball) {SetAIDebugState("Pong: ball missing");m_HasSnapshot=false;return;}
    auto& paddle=GetComponent<TransformComponent>();
    const glm::vec3 ballPos=ball.GetComponent<TransformComponent>().Position;
    const glm::vec3 pos=paddle.Position;
    const glm::vec3 velocity=m_HasLastBall ? (ballPos-m_LastBallPosition)/dt : glm::vec3(0.f);
    m_LastBallPosition=ballPos; m_HasLastBall=true;
    const float scale=std::max(0.01f,ObservationScale);
    const std::vector<float> inputs={
        std::clamp(ballPos.x/scale,-1.f,1.f),
        std::clamp(ballPos.y/scale,-1.f,1.f),
        std::clamp(velocity.x/scale,-1.f,1.f),
        std::clamp(velocity.y/scale,-1.f,1.f),
        std::clamp(pos.y/scale,-1.f,1.f),
        std::clamp(std::abs(ballPos.x-pos.x)/scale,0.f,1.f)};
    try {
        const auto outputs=m_Model->Predict(inputs);
        if(outputs.size()!=1 || !std::isfinite(outputs[0])) {SetAIDebugState("Pong: invalid output");return;}
        m_Snapshot.model=ActiveModel;
        m_Snapshot.status="PongAgent (project script DLL)";
        m_Snapshot.inputs=inputs;
        m_Snapshot.outputs=outputs;
        m_Snapshot.distance=glm::length(ballPos-pos);
        m_Snapshot.activations=NeuralAgent::Activations(*m_Model,inputs);
        m_HasSnapshot=true;
        paddle.Position.y=std::clamp(paddle.Position.y+
            std::clamp(outputs[0],-1.f,1.f)*std::max(0.f,MovementSpeed)*dt,
            std::min(MinY,MaxY),std::max(MinY,MaxY));
        SetAIDebugState("Pong: neural inference active");
    } catch(const std::exception& e) {
        std::fprintf(stderr,"[PongAgent] Inference: %s\n",e.what());
        SetAIDebugState("Pong: inference error");m_HasSnapshot=false;
    }
}
void PongAgent::OnDestroy() {m_Model.reset();m_HasSnapshot=false;}
bool PongAgent::GetNeuralDebugSnapshot(NeuralAgent::Snapshot& out) const {
    if(!m_HasSnapshot) return false;
    out=m_Snapshot;return true;
}
NOJOB_REGISTER_SCRIPT(PongAgent,"AI / Neural",
    NOJOB_FIELD(PongAgent,ActiveModel),
    NOJOB_FIELD(PongAgent,TargetTag),
    NOJOB_FIELD(PongAgent,ObservationScale),
    NOJOB_FIELD(PongAgent,MovementSpeed),
    NOJOB_FIELD(PongAgent,MinY),
    NOJOB_FIELD(PongAgent,MaxY))
}
