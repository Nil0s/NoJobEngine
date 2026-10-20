#pragma once
#include "BTNode.h"
#include <memory>
#include <stdexcept>
namespace NoJob {
// Main task and background task advance on the same frame, no threads.
class BTParallel final : public BTNode {
public:
    BTParallel(BTNodeID id, std::unique_ptr<BTNode> mainTask, std::unique_ptr<BTNode> background)
        : BTNode(id), m_Main(std::move(mainTask)), m_Background(std::move(background)) {
        if (!m_Main || !m_Background) throw std::invalid_argument("Parallel needs two children");
    }
    std::string DebugLabel() const override { return "Simple Parallel"; }
    BTStatus Tick(BTContext& ctx, float dt) const override {
        const auto primary = m_Main->Tick(ctx, dt);
        if (primary == BTStatus::Running) m_Background->Tick(ctx, dt);
        else Reset(ctx);
        ctx.RecordStatus(GetID(), static_cast<int>(primary));
        return primary;
    }
    void Reset(BTContext& ctx) const override {
        m_Main->Reset(ctx); m_Background->Reset(ctx); BTNode::Reset(ctx);
    }
    void CollectChildren(std::vector<const BTNode*>& out) const override {
        out.push_back(m_Main.get()); out.push_back(m_Background.get());
    }
private:
    std::unique_ptr<BTNode> m_Main, m_Background;
};
}
