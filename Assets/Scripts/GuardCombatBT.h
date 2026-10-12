#pragma once
#include "Engine/AI/BehaviorTree/BTScriptBridge.h"
#include "Engine/Scene/ScriptRegistry.h"

namespace NoJob {
// Add GuardCombatBT and BTAssetAgent to the same entity.
// In a .njbt Action node, set native name to "Guard.Attack".
class GuardCombatBT final : public Script {
public:
    float AttackCooldown = 1.0f;

    BTStatus Attack(BTContext& context, float dt) {
        (void)dt;
        // Replace with project combat logic; Blackboard belongs to the BT agent.
        return context.Blackboard().Contains("TargetHandle")
            ? BTStatus::Success : BTStatus::Failure;
    }
    bool HasTarget(BTContext& context) {
        return context.Blackboard().Contains("TargetHandle");
    }
    // Editor catalog only: names and types, no script instance or runtime callbacks.
    static std::vector<BTCallbackMetadata> DescribeBehaviorTreeBindings() {
        return {{"Guard.Attack", false}, {"Guard.HasTarget", true}};
    }
    void RegisterBehaviorTreeBindings(BTNativeRegistry& registry) const override {
        BTRegisterScriptAction<GuardCombatBT>(registry, "Guard.Attack", "GuardCombatBT", &GuardCombatBT::Attack);
        BTRegisterScriptCondition<GuardCombatBT>(registry, "Guard.HasTarget", "GuardCombatBT", &GuardCombatBT::HasTarget);
    }
};
// Register through the same mechanism as other project scripts.

} // namespace NoJob
