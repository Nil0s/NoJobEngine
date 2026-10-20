#pragma once
#include "BTTree.h"
#include <memory>
#include <stdexcept>

namespace NoJob {
class BTInstance {
public:
    explicit BTInstance(std::shared_ptr<const BTTree> tree) : m_Tree(std::move(tree)) {
        if (!m_Tree) throw std::invalid_argument("BTInstance requires a tree");
    }
    BTStatus Tick(float dt) { m_Context.BeginTick(); return m_Tree->Root().Tick(m_Context, dt); }
    void Reset() { m_Tree->Root().Reset(m_Context); m_Context.Reset(); }
    const BTTree& Tree() const { return *m_Tree; }
    BTContext& Context() { return m_Context; }
    const BTContext& Context() const { return m_Context; }
private:
    std::shared_ptr<const BTTree> m_Tree;
    BTContext m_Context;
};
}
