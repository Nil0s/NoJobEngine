#pragma once
#include "BTNativeRegistry.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace NoJob {
// Conditional branch gate. Observes one Blackboard key and requests an
// immediate re-evaluation on the next engine update (never re-enters a Tick).
class BTDecorator final : public BTNode {
public:
    BTDecorator(BTNodeID id, std::shared_ptr<const BTNativeRegistry> registry,
                std::string condition, std::string observedKey, std::unique_ptr<BTNode> child)
        : BTNode(id), m_Registry(std::move(registry)), m_Condition(std::move(condition)),
          m_Key(std::move(observedKey)), m_Child(std::move(child)) {
        if (!m_Child || !m_Registry || !m_Registry->HasCondition(m_Condition))
            throw std::invalid_argument("Invalid BT decorator");
    }
    std::string DebugLabel() const override { return "Decorator: " + m_Condition; }
    BTStatus Tick(BTContext& ctx, float dt) const override {
        if (!m_Key.empty() && !ctx.GetNodeState(GetID()).TickCount) {
            const auto token = ctx.Blackboard().Observe(m_Key, [&ctx](std::string_view key) { ctx.Emit(0, "BlackboardChanged", std::string(key)); ctx.RequestReevaluation(); });
            ctx.GetNodeState(GetID()).TickCount = static_cast<std::size_t>(token);
        }
        const bool allowed = m_Registry->GetCondition(m_Condition)(ctx);
        if (!allowed) {
            m_Child->Reset(ctx);
            ctx.RecordStatus(GetID(), static_cast<int>(BTStatus::Failure));
            return BTStatus::Failure;
        }
        const auto status = m_Child->Tick(ctx, dt);
        ctx.RecordStatus(GetID(), static_cast<int>(status));
        return status;
    }
    void Reset(BTContext& ctx) const override {
        m_Child->Reset(ctx);
        if (const auto* state = ctx.FindNodeState(GetID()); state && state->TickCount)
            ctx.Blackboard().Unobserve(static_cast<BTBlackboard::ObserverID>(state->TickCount));
        BTNode::Reset(ctx);
    }
    void CollectChildren(std::vector<const BTNode*>& out) const override { out.push_back(m_Child.get()); }
private:
    std::shared_ptr<const BTNativeRegistry> m_Registry;
    std::string m_Condition, m_Key;
    std::unique_ptr<BTNode> m_Child;
};
} // namespace NoJob
