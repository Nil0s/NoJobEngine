/**
 * @file BTNode.h
 * @brief Base interface for behavior tree nodes.
 * @ingroup BehaviorTree
 */
#pragma once
#include "BTContext.h"
#include <vector>
#include <string>

namespace NoJob {
/** @brief Result of a behavior tree node tick. */
enum class BTStatus { Success, Failure, Running };
/** @brief Base class for behavior tree nodes with stable node identifiers. */
class BTNode {
public:
    explicit BTNode(BTNodeID id) : m_ID(id) {}
    virtual ~BTNode() = default;
    BTNode(const BTNode&) = delete;
    BTNode& operator=(const BTNode&) = delete;
    BTNodeID GetID() const { return m_ID; }
    virtual std::string DebugLabel() const { return "Node"; }
    /** @brief Evaluates this node for one update.
     * @param context Per-agent execution context.
     * @param deltaTime Time elapsed since the previous tick.
     * @return Success, Failure or Running. */
    virtual BTStatus Tick(BTContext& context, float deltaTime) const = 0;
    virtual void Reset(BTContext& context) const { context.CancelNode(m_ID); context.ResetNodeState(m_ID); }
    virtual void CollectChildren(std::vector<const BTNode*>&) const {}
private:
    BTNodeID m_ID;
};
}
