#include "../BTBlackboard.h"
#include "../BTInstance.h"
#include "../BTSequence.h"
#include "../BTTestNode.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>

using namespace NoJob;

// Demonstrates custom vector-like types without requiring GLM in standalone tests.
// In NoJobEngine the exact same API accepts glm::vec3.
struct Position { float x, y, z; };

int main() {
    {
        BTBlackboard b;
        b.Set("visible", true);
        b.Set("distance", 12.5f);
        b.Set("count", std::int64_t{7});
        b.Set("name", "Guard");
        b.Set("position", Position{1, 2, 3});
        assert(b.Get<bool>("visible"));
        assert(b.Get<float>("distance") == 12.5f);
        assert(b.Get<std::int64_t>("count") == 7);
        assert(b.Get<std::string>("name") == "Guard");
        assert(b.Get<Position>("position").y == 2);
        assert(b.Size() == 5);
        std::cout << "TEST 1 PASSED: typed values\n";
    }
    {
        BTBlackboard b;
        b.Set("speed", 1.0f);
        b.Set("speed", 2.0f);
        assert(b.Size() == 1 && b.Get<float>("speed") == 2.0f);
        assert(b.TryGet<int>("speed") == nullptr);
        assert(b.TryGet<float>("missing") == nullptr);
        assert(b.GetOr<float>("missing", 9.0f) == 9.0f);
        assert(b.GetOr<int>("speed", 8) == 8);
        bool thrown = false;
        try { (void)b.Get<int>("speed"); } catch (const std::runtime_error&) { thrown = true; }
        assert(thrown);
        assert(b.TypeOf("speed") == std::type_index(typeid(float)));
        std::cout << "TEST 2 PASSED: strict typing and defaults\n";
    }
    {
        BTBlackboard b;
        b.Set("x", 3);
        assert(b.Contains("x"));
        assert(b.Remove("x"));
        assert(!b.Remove("x"));
        assert(!b.Contains("x"));
        assert(b.TypeOf("x") == std::type_index(typeid(void)));
        b.Set("y", 2);
        b.Clear();
        assert(b.Size() == 0);
        bool thrown = false;
        try { b.Set("", 3); } catch (const std::invalid_argument&) { thrown = true; }
        assert(thrown);
        std::cout << "TEST 3 PASSED: lifecycle\n";
    }
    {
        BTContext a, b;
        a.Blackboard().Set("target", std::string("PlayerA"));
        b.Blackboard().Set("target", std::string("PlayerB"));
        a.Blackboard().Set("visible", true);
        assert(a.Blackboard().Get<std::string>("target") == "PlayerA");
        assert(b.Blackboard().Get<std::string>("target") == "PlayerB");
        assert(!b.Blackboard().Contains("visible"));
        std::cout << "TEST 4 PASSED: contexts isolated\n";
    }
    {
        auto root = std::make_unique<BTSequence>(1);
        root->AddChild(std::make_unique<BTTestNode>(2, BTStatus::Running));
        auto tree = std::make_shared<BTTree>(std::move(root));
        BTInstance guardA(tree), guardB(tree);
        guardA.Context().Blackboard().Set("lastSeen", Position{1, 2, 3});
        guardB.Context().Blackboard().Set("lastSeen", Position{9, 8, 7});
        assert(guardA.Tick(0.016f) == BTStatus::Running);
        assert(guardB.Tick(0.016f) == BTStatus::Running);
        assert(guardA.Context().Blackboard().Get<Position>("lastSeen").x == 1);
        assert(guardB.Context().Blackboard().Get<Position>("lastSeen").x == 9);
        guardA.Reset();
        assert(guardA.Context().Blackboard().Contains("lastSeen"));
        assert(guardB.Context().FindNodeState(2) != nullptr);
        std::cout << "TEST 5 PASSED: shared tree and instance isolation\n";
    }
    {
        BTContext context;
        context.Blackboard().Set("memory", true);
        context.GetNodeState(1).CurrentChild = 3;
        context.Reset();
        assert(context.FindNodeState(1) == nullptr);
        assert(context.Blackboard().Get<bool>("memory"));
        context.GetNodeState(1).CurrentChild = 2;
        context.ResetAll();
        assert(context.FindNodeState(1) == nullptr);
        assert(context.Blackboard().Size() == 0);
        std::cout << "TEST 6 PASSED: reset semantics\n";
    }
    {
        BTBlackboard b;
        b.Set("n", 1);
        auto copy = b;
        copy.Set("n", 2);
        assert(b.Get<int>("n") == 1);
        assert(copy.Get<int>("n") == 2);
        std::cout << "TEST 7 PASSED: value ownership\n";
    }
    std::cout << "ALL BLACKBOARD TESTS PASSED\n";
}
