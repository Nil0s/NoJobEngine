#pragma once
#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/AI/Neural/NeuralNetwork.h"
#include <optional>
#include <string>

namespace NoJob {
// Project-native example. This is NOT compiled into the engine.
class PongAgent final : public Script {
public:
    std::string ActiveModel = "PongBrainDemo";
    std::string TargetTag = "PongBall";
    float ObservationScale = 10.f;
    float MovementSpeed = 5.f;
    float MinY = -4.f;
    float MaxY = 4.f;
    void OnCreate() override;
    void OnUpdate(float dt) override;
    void OnDestroy() override;
    bool GetNeuralDebugSnapshot(NeuralAgent::Snapshot& out) const override;
private:
    std::optional<Neural::NeuralNetwork> m_Model;
    glm::vec3 m_LastBallPosition{0.f};
    bool m_HasLastBall = false;
    bool m_HasSnapshot = false;
    NeuralAgent::Snapshot m_Snapshot;
};
}
