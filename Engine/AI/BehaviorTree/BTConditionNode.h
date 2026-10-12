#pragma once
#include "BTNativeRegistry.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace NoJob {
class BTConditionNode final : public BTNode {
public:
    BTConditionNode(BTNodeID id, std::shared_ptr<const BTNativeRegistry> registry,
                    std::string conditionName)
        : BTNode(id), m_Registry(std::move(registry)), m_Name(std::move(conditionName)) {
        if (!m_Registry || !m_Registry->HasCondition(m_Name))
            throw std::invalid_argument("Unregistered native condition: " + m_Name);
    }
    std::string DebugLabel() const override { return "Condition: " + m_Name; }
    BTStatus Tick(BTContext& context, float dt) const override {
        (void)dt;
        const auto status = m_Registry->GetCondition(m_Name)(context)
            ? BTStatus::Success : BTStatus::Failure;
        context.RecordStatus(GetID(), static_cast<int>(status));
        return status;
    }
    const std::string& Name() const { return m_Name; }
private:
    std::shared_ptr<const BTNativeRegistry> m_Registry;
    std::string m_Name;
};
} // namespace NoJob
