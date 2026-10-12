#include "../BTSelector.h"
#include "../BTSequence.h"
#include "../BTTestNode.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace NoJob;
static std::unique_ptr<BTTestNode> Test(BTNodeID id, BTStatus s) {
    return std::make_unique<BTTestNode>(id, s);
}
int main() {
    constexpr float dt = 0.016f;
    {
        BTContext c; BTSelector s(1);
        s.AddChild(Test(2, BTStatus::Failure)); s.AddChild(Test(3, BTStatus::Success));
        assert(s.Tick(c, dt) == BTStatus::Success);
        assert(c.FindNodeState(1) == nullptr);
        std::cout << "TEST 1 PASSED\n";
    }
    {
        BTContext c; BTSelector s(1);
        s.AddChild(Test(2, BTStatus::Failure)); s.AddChild(Test(3, BTStatus::Failure));
        assert(s.Tick(c, dt) == BTStatus::Failure);
        std::cout << "TEST 2 PASSED\n";
    }
    {
        BTContext c; BTSelector s(1);
        s.AddChild(Test(2, BTStatus::Failure)); s.AddChild(Test(3, BTStatus::Running));
        assert(s.Tick(c, dt) == BTStatus::Running);
        assert(s.Tick(c, dt) == BTStatus::Running);
        assert(c.GetNodeState(1).CurrentChild == 1);
        assert(c.GetNodeState(2).TickCount == 1);
        assert(c.GetNodeState(3).TickCount == 2);
        std::cout << "TEST 3 PASSED\n";
    }
    {
        BTContext c; BTSelector s(1);
        s.AddChild(Test(2, BTStatus::Success)); s.AddChild(Test(3, BTStatus::Failure));
        assert(s.Tick(c, dt) == BTStatus::Success);
        assert(c.FindNodeState(3) == nullptr);
        std::cout << "TEST 4 PASSED\n";
    }
    {
        BTContext a, b; BTSelector s(1);
        s.AddChild(Test(2, BTStatus::Failure)); s.AddChild(Test(3, BTStatus::Running));
        assert(s.Tick(a, dt) == BTStatus::Running);
        assert(b.FindNodeState(1) == nullptr);
        assert(s.Tick(b, dt) == BTStatus::Running);
        s.Reset(a);
        assert(a.FindNodeState(1) == nullptr);
        assert(b.GetNodeState(1).CurrentChild == 1);
        std::cout << "TEST 5 PASSED\n";
    }
    {
        // Parent success must clear a nested Running child.
        BTContext c; BTSelector s(1);
        auto seq = std::make_unique<BTSequence>(2);
        seq->AddChild(Test(3, BTStatus::Success));
        seq->AddChild(Test(4, BTStatus::Running));
        s.AddChild(std::move(seq));
        assert(s.Tick(c, dt) == BTStatus::Running);
        s.AddChild(Test(5, BTStatus::Success));
        c.GetNodeState(1).CurrentChild = 1;
        assert(s.Tick(c, dt) == BTStatus::Success);
        assert(c.FindNodeState(2) == nullptr);
        assert(c.FindNodeState(4) == nullptr);
        std::cout << "TEST 6 PASSED\n";
    }
    std::cout << "ALL SELECTOR TESTS PASSED\n";
}
