#pragma once
#include "BTBlackboard.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <vector>
#include <utility>
#include <string>
#include <deque>

namespace NoJob {
class ScriptableEntity;
using BTNodeID = std::uint64_t;
struct BTNodeRuntimeState {
    std::size_t CurrentChild = 0;
    std::size_t TickCount = 0; // Diagnostic counter, per agent.
};
class BTContext {
public:
    struct Event {
        std::uint64_t Tick = 0;
        BTNodeID Node = 0;
        std::string Kind;
        std::string Detail;
    };
    void BeginTick() {
        ++m_Tick;
        m_CurrentStatuses.clear();
    }
    void Emit(BTNodeID node, std::string kind, std::string detail = {}) {
        if (m_Events.size() == 96) m_Events.pop_front();
        m_Events.push_back({m_Tick, node, std::move(kind), std::move(detail)});
    }
    const std::deque<Event>& Events() const { return m_Events; }
    const std::unordered_map<BTNodeID, int>& CurrentStatuses() const { return m_CurrentStatuses; }
    std::uint64_t CurrentTick() const { return m_Tick; }

    BTNodeRuntimeState& GetNodeState(BTNodeID id) { return m_States[id]; }
    const BTNodeRuntimeState* FindNodeState(BTNodeID id) const {
        auto it = m_States.find(id);
        return it == m_States.end() ? nullptr : &it->second;
    }
    void ResetNodeState(BTNodeID id) { m_States.erase(id); }

    // Non-owning: assigned by the owning native script during Play.
    void BindScript(ScriptableEntity* script) noexcept { m_Script = script; }
    ScriptableEntity* GetScript() const noexcept { return m_Script; }
    void UnbindScript() noexcept { m_Script = nullptr; }

    // Per-agent cancellation handlers, invoked before execution state is cleared.
    BTNodeID ActiveNode() const noexcept { return m_ActiveNode; }
    void SetActiveNode(BTNodeID id) noexcept { m_ActiveNode = id; }
    void SetCancellation(BTNodeID id, std::function<void(BTContext&)> fn) {
        m_Cancellations.insert_or_assign(id, std::move(fn));
    }
    void CancelNode(BTNodeID id) {
        auto it = m_Cancellations.find(id);
        if (it == m_Cancellations.end()) return;
        auto callback = std::move(it->second);
        m_Cancellations.erase(it);
        const auto current = m_CurrentStatuses.find(id);
        if (current == m_CurrentStatuses.end() || current->second == 2)
            Emit(id, "Aborted", "Running action cancelled");
        if (callback) callback(*this);
    }
    void CancelAll() {
        while (!m_Cancellations.empty()) CancelNode(m_Cancellations.begin()->first);
    }
    void RequestReevaluation() { m_ReevaluationRequested = true; Emit(0, "Reevaluation", "Blackboard observer requested priority update"); }
    bool ConsumeReevaluation() noexcept { return std::exchange(m_ReevaluationRequested, false); }
    void RecordStatus(BTNodeID id, int status) {
        const auto previous = m_LastStatuses.find(id);
        if (previous == m_LastStatuses.end() || previous->second != status)
            Emit(id, status == 0 ? "Succeeded" : status == 1 ? "Failed" : "Running");
        m_LastStatuses[id] = status;
        m_CurrentStatuses[id] = status;
        ++m_TickCounters[id];
    }
    const std::unordered_map<BTNodeID, int>& LastStatuses() const { return m_LastStatuses; }
    const std::unordered_map<BTNodeID, std::size_t>& TickCounters() const { return m_TickCounters; }

    // Reset execution state only; agent knowledge survives restarting a tree.
    void Reset() { CancelAll(); m_States.clear(); m_LastStatuses.clear(); m_TickCounters.clear(); m_ReevaluationRequested = false; m_CurrentStatuses.clear(); m_Events.clear(); m_Tick = 0; }
    BTBlackboard& Blackboard() { return m_Blackboard; }
    const BTBlackboard& Blackboard() const { return m_Blackboard; }

    // Explicitly discard both running state and agent knowledge.
    void ResetAll() { Reset(); m_Blackboard.Clear(); }
private:
    std::unordered_map<BTNodeID, BTNodeRuntimeState> m_States;
    BTBlackboard m_Blackboard;
    std::unordered_map<BTNodeID, std::function<void(BTContext&)>> m_Cancellations;
    std::unordered_map<BTNodeID, int> m_LastStatuses;
    std::unordered_map<BTNodeID, std::size_t> m_TickCounters;
    ScriptableEntity* m_Script = nullptr;
    BTNodeID m_ActiveNode = 0;
    bool m_ReevaluationRequested = false;
    std::uint64_t m_Tick = 0;
    std::unordered_map<BTNodeID, int> m_CurrentStatuses;
    std::deque<Event> m_Events;
};
}
