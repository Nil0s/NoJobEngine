#pragma once
#include "BTNode.h"

namespace NoJob {
class BTTestNode final : public BTNode {
public:
    BTTestNode(BTNodeID id, BTStatus status) : BTNode(id), m_Status(status) {}
    BTStatus Tick(BTContext& context, float dt) const override {
        (void)dt;
        ++context.GetNodeState(GetID()).TickCount;
        return m_Status;
    }
private:
    BTStatus m_Status;
};
}
