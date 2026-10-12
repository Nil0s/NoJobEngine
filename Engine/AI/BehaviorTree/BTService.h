#pragma once
#include "BTNativeRegistry.h"
#include <memory>
#include <stdexcept>
#include <string>
namespace NoJob {
// Periodic service while its branch is active; action is executed on a timer.
class BTService final : public BTNode {
public:
    BTService(BTNodeID id, std::shared_ptr<const BTNativeRegistry> registry,
              std::string action, float interval, std::unique_ptr<BTNode> child)
        : BTNode(id), m_Registry(std::move(registry)), m_Action(std::move(action)),
          m_Interval(interval), m_Child(std::move(child)) {
        if (!m_Child || !m_Registry || !m_Registry->HasAction(m_Action) || interval <= 0)
            throw std::invalid_argument("Invalid BT service");
    }
    std::string DebugLabel() const override { return "Service: " + m_Action; }
    BTStatus Tick(BTContext& ctx, float dt) const override {
        auto& state = ctx.GetNodeState(GetID());
        // Time is stored per-agent; TickCount holds milliseconds of elapsed time.
        const auto elapsed = ctx.Blackboard().GetOr<float>(TimerKey(), 0.f) + dt;
        if (elapsed >= m_Interval) {
            ctx.Blackboard().Set(TimerKey(), 0.f);
            m_Registry->GetAction(m_Action)(ctx, dt);
        } else ctx.Blackboard().Set(TimerKey(), elapsed);
        ++state.TickCount;
        auto status = m_Child->Tick(ctx, dt);
        ctx.RecordStatus(GetID(), static_cast<int>(status));
        if (status != BTStatus::Running) Reset(ctx);
        return status;
    }
    void Reset(BTContext& ctx) const override {
        m_Child->Reset(ctx);
        ctx.Blackboard().Remove(TimerKey());
        BTNode::Reset(ctx);
    }
    void CollectChildren(std::vector<const BTNode*>& out) const override { out.push_back(m_Child.get()); }
private:
    std::string TimerKey() const { return "__bt_service_" + std::to_string(GetID()); }
    std::shared_ptr<const BTNativeRegistry> m_Registry;
    std::string m_Action;
    float m_Interval;
    std::unique_ptr<BTNode> m_Child;
};
}
