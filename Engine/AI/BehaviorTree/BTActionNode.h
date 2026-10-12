#pragma once
#include "BTNativeRegistry.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace NoJob {
// Stateless definition: all mutable per-agent progress belongs in BTContext.
class BTActionNode final : public BTNode {
public:
    BTActionNode(BTNodeID id, std::shared_ptr<const BTNativeRegistry> registry,
                 std::string actionName)
        : BTNode(id), m_Registry(std::move(registry)), m_Name(std::move(actionName)) {
        if (!m_Registry || !m_Registry->HasAction(m_Name))
            throw std::invalid_argument("Unregistered native action: " + m_Name);
    }
    std::string DebugLabel() const override { return "Action: " + m_Name; }
    BTStatus Tick(BTContext& context, float dt) const override {
        // Resolve by name to keep the node independent of concrete native scripts.
        const auto previous = context.ActiveNode();
        context.SetActiveNode(GetID());
        BTStatus status;
        try { status = m_Registry->GetAction(m_Name)(context, dt); }
        catch (...) { context.SetActiveNode(previous); throw; }
        context.SetActiveNode(previous);
        context.RecordStatus(GetID(), static_cast<int>(status));
        if (status != BTStatus::Running) context.CancelNode(GetID());
        return status;
    }
    const std::string& Name() const { return m_Name; }
private:
    std::shared_ptr<const BTNativeRegistry> m_Registry;
    std::string m_Name;
};
} // namespace NoJob
