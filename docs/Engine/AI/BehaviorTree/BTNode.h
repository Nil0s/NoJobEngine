#pragma once
#include "BTContext.h"
#include <vector>
#include <string>

namespace NoJob {
enum class BTStatus { Success, Failure, Running };
class BTNode {
public:
    explicit BTNode(BTNodeID id) : m_ID(id) {}
    virtual ~BTNode() = default;
    BTNode(const BTNode&) = delete;
    BTNode& operator=(const BTNode&) = delete;
    BTNodeID GetID() const { return m_ID; }
    virtual std::string DebugLabel() const { return "Node"; }
    virtual BTStatus Tick(BTContext& context, float deltaTime) const = 0;
    virtual void Reset(BTContext& context) const { context.CancelNode(m_ID); context.ResetNodeState(m_ID); }
    virtual void CollectChildren(std::vector<const BTNode*>&) const {}
private:
    BTNodeID m_ID;
};
}
