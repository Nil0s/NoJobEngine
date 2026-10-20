#pragma once
#include "BTNode.h"
#include <memory>
#include <utility>
#include <vector>

namespace NoJob {
class BTSelector final : public BTNode {
public:
    explicit BTSelector(BTNodeID id) : BTNode(id) {}
    void AddChild(std::unique_ptr<BTNode> child) {
        if (child) m_Children.push_back(std::move(child));
    }
    std::string DebugLabel() const override { return "Selector"; }
    BTStatus Tick(BTContext& context, float dt) const override {
        // A running fallback (e.g. Patrol) must not lock out higher-priority
        // branches. Probe them on every tick and preempt when one is ready.
        // This also supports ordinary Condition nodes, without a decorator
        // Blackboard observer being required.
        if (context.ConsumeReevaluation())
            context.Emit(GetID(), "Reevaluation", "Selector checking priorities");

        const auto runningIndex = context.GetNodeState(GetID()).CurrentChild;
        if (runningIndex > 0 && runningIndex < m_Children.size()) {
            for (std::size_t i = 0; i < runningIndex; ++i) {
                const auto status = m_Children[i]->Tick(context, dt);
                if (status == BTStatus::Failure) {
                    m_Children[i]->Reset(context);
                    continue;
                }
                context.Emit(GetID(), "PriorityChanged", "Higher priority branch preempted running fallback");
                m_Children[runningIndex]->Reset(context);
                context.GetNodeState(GetID()).CurrentChild = i;
                if (status == BTStatus::Success) {
                    Reset(context);
                    return BTStatus::Success;
                }
                return BTStatus::Running;
            }
        }
        while (true) {
            const auto index = context.GetNodeState(GetID()).CurrentChild;
            if (index >= m_Children.size()) {
                Reset(context);
                return BTStatus::Failure;
            }
            const auto status = m_Children[index]->Tick(context, dt);
            if (status == BTStatus::Running) return BTStatus::Running;
            if (status == BTStatus::Success) {
                Reset(context);
                return BTStatus::Success;
            }
            context.GetNodeState(GetID()).CurrentChild = index + 1;
        }
    }
    void Reset(BTContext& context) const override {
        for (const auto& child : m_Children) child->Reset(context);
        BTNode::Reset(context);
    }
    void CollectChildren(std::vector<const BTNode*>& out) const override {
        for (const auto& child : m_Children) out.push_back(child.get());
    }
private:
    std::vector<std::unique_ptr<BTNode>> m_Children;
};
}
