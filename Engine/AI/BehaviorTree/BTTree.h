#pragma once
#include "BTNode.h"
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace NoJob {
class BTTree {
public:
    explicit BTTree(std::unique_ptr<BTNode> root) : m_Root(std::move(root)) {
        if (!m_Root) throw std::invalid_argument("Behavior Tree requires a root");
        std::unordered_set<BTNodeID> ids;
        std::vector<const BTNode*> pending{m_Root.get()};
        while (!pending.empty()) {
            const BTNode* node = pending.back();
            pending.pop_back();
            if (node->GetID() == 0 || !ids.insert(node->GetID()).second)
                throw std::invalid_argument("Behavior Tree has zero or duplicate node ID");
            node->CollectChildren(pending);
        }
    }
    const BTNode& Root() const { return *m_Root; }
private:
    std::unique_ptr<BTNode> m_Root;
};
}
