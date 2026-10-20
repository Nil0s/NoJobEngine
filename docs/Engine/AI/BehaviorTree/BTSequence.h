#pragma once
#include "BTNode.h"
#include <memory>
#include <utility>
#include <vector>

namespace NoJob {
class BTSequence final : public BTNode {
public:
    explicit BTSequence(BTNodeID id) : BTNode(id) {}
    void AddChild(std::unique_ptr<BTNode> child) {
        if (child) m_Children.push_back(std::move(child));
    }
    std::string DebugLabel() const override { return "Sequence"; }
    BTStatus Tick(BTContext& context, float dt) const override {
        while (true) {
            const auto index = context.GetNodeState(GetID()).CurrentChild;
            if (index >= m_Children.size()) {
                Reset(context);
                return BTStatus::Success;
            }
            const auto status = m_Children[index]->Tick(context, dt);
            if (status == BTStatus::Running) return BTStatus::Running;
            if (status == BTStatus::Failure) {
                Reset(context);
                return BTStatus::Failure;
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
