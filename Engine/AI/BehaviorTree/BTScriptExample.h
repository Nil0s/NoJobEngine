#pragma once
#include "Engine/AI/BehaviorTree/BTScriptBridge.h"
#include "Engine/AI/BehaviorTree/BTNativeRegistry.h"
#include "Engine/AI/BehaviorTree/BTActionNode.h"
#include "Engine/AI/BehaviorTree/BTConditionNode.h"
#include "Engine/AI/BehaviorTree/BTSequence.h"
#include "Engine/Scene/ScriptRegistry.h"

namespace NoJob {
// Example integration: assign "BTExampleMover" as a NativeScriptComponent script.
// Uses only TransformComponent; replace with NavAgent/Perception callbacks later.
class BTExampleMover final : public BTScriptController {
protected:
    std::shared_ptr<const BTTree> CreateTree() override {
        static const std::shared_ptr<const BTTree> tree = [] {
            auto registry = std::make_shared<BTNativeRegistry>();
            registry->RegisterCondition("CanMove", [](BTContext& ctx) {
                auto* script = BTGetScript(ctx);
                return script && script->HasComponent<TransformComponent>();
            });
            registry->RegisterAction("MoveForward", [](BTContext& ctx, float dt) {
                auto* script = BTGetScript(ctx);
                if (!script || !script->HasComponent<TransformComponent>())
                    return BTStatus::Failure;
                auto& transform = script->GetComponent<TransformComponent>();
                transform.Position.x += dt;
                ctx.Blackboard().Set("Elapsed", ctx.Blackboard().GetOr<float>("Elapsed", 0.f) + dt);
                return BTStatus::Running;
            });
            auto sequence = std::make_unique<BTSequence>(1);
            sequence->AddChild(std::make_unique<BTConditionNode>(2, registry, "CanMove"));
            sequence->AddChild(std::make_unique<BTActionNode>(3, registry, "MoveForward"));
            return std::make_shared<const BTTree>(std::move(sequence));
        }();
        return tree;
    }
};

inline void RegisterBTExampleMover() {
    ScriptRegistry::Register({"BTExampleMover", "AI", {},
        [] { return std::make_unique<BTExampleMover>(); }});
}
} // namespace NoJob
