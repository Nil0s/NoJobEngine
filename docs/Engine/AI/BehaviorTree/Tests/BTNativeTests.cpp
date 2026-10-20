#include "../BTActionNode.h"
#include "../BTConditionNode.h"
#include "../BTInstance.h"
#include "../BTSequence.h"
#include "../BTSelector.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
using namespace NoJob;

template<class Fn> bool Throws(Fn&& fn) {
    try { fn(); } catch (const std::exception&) { return true; }
    return false;
}

int main() {
    auto registry = std::make_shared<BTNativeRegistry>();
    registry->RegisterCondition("HasTarget", [](BTContext& c) {
        return c.Blackboard().GetOr<bool>("TargetVisible", false);
    });
    registry->RegisterAction("Chase", [](BTContext& c, float dt) {
        if (dt < 0) return BTStatus::Failure;
        int& ticks = c.Blackboard().Get<int>("ChaseTicks");
        ++ticks;
        return ticks >= 3 ? BTStatus::Success : BTStatus::Running;
    });
    registry->RegisterAction("Attack", [](BTContext& c, float) {
        c.Blackboard().Set("Attacked", true);
        return BTStatus::Success;
    });
    std::shared_ptr<const BTNativeRegistry> frozen = registry;

    // 1: Conditions map boolean results to BTStatus.
    {
        BTConditionNode condition(1, frozen, "HasTarget");
        BTContext c;
        assert(condition.Tick(c, 0.016f) == BTStatus::Failure);
        c.Blackboard().Set("TargetVisible", true);
        assert(condition.Tick(c, 0.016f) == BTStatus::Success);
        std::cout << "TEST 1 PASSED\n";
    }
    // 2: Action receives dt and preserves its per-agent progress in blackboard.
    {
        BTActionNode action(1, frozen, "Chase");
        BTContext c;
        c.Blackboard().Set("ChaseTicks", 0);
        assert(action.Tick(c, 0.016f) == BTStatus::Running);
        assert(action.Tick(c, 0.016f) == BTStatus::Running);
        assert(action.Tick(c, 0.016f) == BTStatus::Success);
        assert(c.Blackboard().Get<int>("ChaseTicks") == 3);
        std::cout << "TEST 2 PASSED\n";
    }
    // 3: Shared tree, independent agent memories and running cursors.
    {
        auto seq = std::make_unique<BTSequence>(1);
        seq->AddChild(std::make_unique<BTConditionNode>(2, frozen, "HasTarget"));
        seq->AddChild(std::make_unique<BTActionNode>(3, frozen, "Chase"));
        seq->AddChild(std::make_unique<BTActionNode>(4, frozen, "Attack"));
        auto tree = std::make_shared<BTTree>(std::move(seq));
        BTInstance a(tree), b(tree);
        a.Context().Blackboard().Set("TargetVisible", true);
        a.Context().Blackboard().Set("ChaseTicks", 0);
        b.Context().Blackboard().Set("TargetVisible", false);
        b.Context().Blackboard().Set("ChaseTicks", 0);
        assert(a.Tick(0.016f) == BTStatus::Running);
        assert(b.Tick(0.016f) == BTStatus::Failure);
        assert(a.Context().FindNodeState(1)->CurrentChild == 1);
        assert(a.Context().Blackboard().Get<int>("ChaseTicks") == 1);
        assert(b.Context().Blackboard().Get<int>("ChaseTicks") == 0);
        b.Context().Blackboard().Set("TargetVisible", true);
        assert(b.Tick(0.016f) == BTStatus::Running);
        assert(a.Tick(0.016f) == BTStatus::Running);
        assert(a.Tick(0.016f) == BTStatus::Success);
        assert(a.Context().Blackboard().Get<bool>("Attacked"));
        assert(!b.Context().Blackboard().Contains("Attacked"));
        assert(b.Tick(0.016f) == BTStatus::Running);
        assert(b.Tick(0.016f) == BTStatus::Success);
        assert(b.Context().Blackboard().Get<bool>("Attacked"));
        std::cout << "TEST 3 PASSED\n";
    }
    // 4: Duplicate and invalid registrations are rejected.
    {
        assert(Throws([&] { registry->RegisterAction("Chase", [](BTContext&, float){ return BTStatus::Success; }); }));
        assert(Throws([&] { registry->RegisterCondition("HasTarget", [](BTContext&){ return true; }); }));
        assert(Throws([&] { registry->RegisterAction("", [](BTContext&, float){ return BTStatus::Success; }); }));
        assert(Throws([&] { registry->RegisterCondition("Empty", {}); }));
        std::cout << "TEST 4 PASSED\n";
    }
    // 5: Missing bindings fail during node construction, not during Tick.
    {
        assert(Throws([&] { BTActionNode node(1, frozen, "Missing"); }));
        assert(Throws([&] { BTConditionNode node(1, frozen, "Missing"); }));
        assert(Throws([&] { BTActionNode node(1, nullptr, "Chase"); }));
        std::cout << "TEST 5 PASSED\n";
    }
    // 6: Condition short circuits action; fallback selector executes.
    {
        auto seq = std::make_unique<BTSequence>(2);
        seq->AddChild(std::make_unique<BTConditionNode>(3, frozen, "HasTarget"));
        seq->AddChild(std::make_unique<BTActionNode>(4, frozen, "Attack"));
        auto sel = std::make_unique<BTSelector>(1);
        sel->AddChild(std::move(seq));
        sel->AddChild(std::make_unique<BTActionNode>(5, frozen, "Chase"));
        BTContext c;
        c.Blackboard().Set("ChaseTicks", 2);
        assert(sel->Tick(c, 0.016f) == BTStatus::Success);
        assert(!c.Blackboard().Contains("Attacked"));
        std::cout << "TEST 6 PASSED\n";
    }
    // 7: Reset keeps knowledge; ResetAll clears it.
    {
        BTContext c;
        c.Blackboard().Set("ChaseTicks", 2);
        BTActionNode node(1, frozen, "Chase");
        node.Reset(c);
        assert(c.Blackboard().Get<int>("ChaseTicks") == 2);
        c.ResetAll();
        assert(!c.Blackboard().Contains("ChaseTicks"));
        std::cout << "TEST 7 PASSED\n";
    }
    // 8: Names are introspectable for future editor binding.
    {
        BTActionNode a(1, frozen, "Chase");
        BTConditionNode c(2, frozen, "HasTarget");
        assert(a.Name() == "Chase" && c.Name() == "HasTarget");
        assert(registry->HasAction("Chase") && registry->HasCondition("HasTarget"));
        std::cout << "TEST 8 PASSED\n";
    }
    std::cout << "ALL NATIVE TESTS PASSED\n";
}
