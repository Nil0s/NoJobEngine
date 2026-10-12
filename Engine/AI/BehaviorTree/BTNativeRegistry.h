#pragma once
#include "BTContext.h"
#include "BTNode.h"
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

namespace NoJob {
// Build/register before sharing as shared_ptr<const BTNativeRegistry>.
// Callbacks must not retain references to a temporary BTContext.
class BTNativeRegistry {
public:
    using Action = std::function<BTStatus(BTContext&, float)>;
    using Condition = std::function<bool(BTContext&)>;

    void RegisterAction(std::string name, Action callback) {
        if (name.empty() || !callback)
            throw std::invalid_argument("Action requires a name and callback");
        if (m_Actions.contains(name))
            throw std::invalid_argument("Duplicate native action: " + name);
        m_Actions.emplace(std::move(name), std::move(callback));
    }

    void RegisterCondition(std::string name, Condition callback) {
        if (name.empty() || !callback)
            throw std::invalid_argument("Condition requires a name and callback");
        if (m_Conditions.contains(name))
            throw std::invalid_argument("Duplicate native condition: " + name);
        m_Conditions.emplace(std::move(name), std::move(callback));
    }

    bool HasAction(std::string_view name) const {
        return m_Actions.contains(std::string(name));
    }
    bool HasCondition(std::string_view name) const {
        return m_Conditions.contains(std::string(name));
    }

    // Stable sorted copies for editor dropdowns. No callbacks or DLL pointers escape.
    std::vector<std::string> ActionNames() const {
        std::vector<std::string> result;
        result.reserve(m_Actions.size());
        for (const auto& [name, callback] : m_Actions) result.push_back(name);
        std::sort(result.begin(), result.end());
        return result;
    }
    std::vector<std::string> ConditionNames() const {
        std::vector<std::string> result;
        result.reserve(m_Conditions.size());
        for (const auto& [name, callback] : m_Conditions) result.push_back(name);
        std::sort(result.begin(), result.end());
        return result;
    }

    const Action& GetAction(std::string_view name) const {
        const auto it = m_Actions.find(std::string(name));
        if (it == m_Actions.end())
            throw std::out_of_range("Unknown native action: " + std::string(name));
        return it->second;
    }
    const Condition& GetCondition(std::string_view name) const {
        const auto it = m_Conditions.find(std::string(name));
        if (it == m_Conditions.end())
            throw std::out_of_range("Unknown native condition: " + std::string(name));
        return it->second;
    }
private:
    std::unordered_map<std::string, Action> m_Actions;
    std::unordered_map<std::string, Condition> m_Conditions;
};
} // namespace NoJob
