#include "../BTSequence.h"
#include "../BTSelector.h"
#include "../BTTestNode.h"
#include "../BTInstance.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace NoJob;
static std::unique_ptr<BTTestNode> Test(BTNodeID id, BTStatus s) {
    return std::make_unique<BTTestNode>(id, s);
}
int main() {
    constexpr float dt = 0.016f;
    {
        BTContext c; BTSequence s(1);
        s.AddChild(Test(2, BTStatus::Success)); s.AddChild(Test(3, BTStatus::Success));
        assert(s.Tick(c, dt) == BTStatus::Success);
        assert(c.FindNodeState(1) == nullptr);
        std::cout << "TEST 1 PASSED\n";
    }
    {
        BTContext c; BTSequence s(1);
        s.AddChild(Test(2, BTStatus::Success)); s.AddChild(Test(3, BTStatus::Failure));
        assert(s.Tick(c, dt) == BTStatus::Failure);
        assert(c.FindNodeState(1) == nullptr);
        std::cout << "TEST 2 PASSED\n";
    }
    {
        BTContext c; BTSequence s(1);
        s.AddChild(Test(2, BTStatus::Success)); s.AddChild(Test(3, BTStatus::Running));
        assert(s.Tick(c, dt) == BTStatus::Running);
        assert(s.Tick(c, dt) == BTStatus::Running);
        assert(c.GetNodeState(1).CurrentChild == 1);
        assert(c.GetNodeState(2).TickCount == 1);
        assert(c.GetNodeState(3).TickCount == 2);
        std::cout << "TEST 3 PASSED\n";
    }
    {
        auto root = std::make_unique<BTSequence>(1);
        root->AddChild(Test(2, BTStatus::Success)); root->AddChild(Test(3, BTStatus::Running));
        auto tree = std::make_shared<BTTree>(std::move(root));
        BTInstance a(tree), b(tree);
        assert(a.Tick(dt) == BTStatus::Running);
        assert(a.Context().GetNodeState(1).CurrentChild == 1);
        assert(b.Context().FindNodeState(1) == nullptr);
        assert(b.Tick(dt) == BTStatus::Running);
        a.Reset();
        assert(a.Context().FindNodeState(1) == nullptr);
        assert(b.Context().GetNodeState(1).CurrentChild == 1);
        std::cout << "TEST 4 PASSED\n";
    }
    {
        BTContext c; BTSequence s(1);
        s.AddChild(Test(2, BTStatus::Success));
        auto sel = std::make_unique<BTSelector>(3);
        sel->AddChild(Test(4, BTStatus::Failure));
        sel->AddChild(Test(5, BTStatus::Running));
        s.AddChild(std::move(sel));
        assert(s.Tick(c, dt) == BTStatus::Running);
        assert(c.GetNodeState(1).CurrentChild == 1);
        assert(c.GetNodeState(3).CurrentChild == 1);
        s.Reset(c);
        assert(c.FindNodeState(1) == nullptr);
        assert(c.FindNodeState(3) == nullptr);
        assert(c.FindNodeState(5) == nullptr);
        std::cout << "TEST 5 PASSED\n";
    }
    {
        // A nested child left in Running must be cleared when its parent fails.
        BTContext c; BTSequence s(1);
        auto sel = std::make_unique<BTSelector>(2);
        sel->AddChild(Test(3, BTStatus::Failure));
        sel->AddChild(Test(4, BTStatus::Running));
        s.AddChild(std::move(sel));
        assert(s.Tick(c, dt) == BTStatus::Running);
        // Force the parent to resume at an immediate failing child.
        s.AddChild(Test(5, BTStatus::Failure));
        c.GetNodeState(1).CurrentChild = 1;
        assert(s.Tick(c, dt) == BTStatus::Failure);
        assert(c.FindNodeState(2) == nullptr);
        assert(c.FindNodeState(4) == nullptr);
        std::cout << "TEST 6 PASSED\n";
    }
    {
        auto s = std::make_unique<BTSequence>(1);
        s->AddChild(Test(2, BTStatus::Success)); s->AddChild(Test(2, BTStatus::Running));
        bool rejected = false;
        try { BTTree tree(std::move(s)); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        std::cout << "TEST 7 PASSED\n";
    }
    std::cout << "ALL SEQUENCE TESTS PASSED\n";
}
