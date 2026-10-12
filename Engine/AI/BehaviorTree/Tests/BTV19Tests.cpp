#include "Engine/AI/BehaviorTree/BTActionNode.h"
#include "Engine/AI/BehaviorTree/BTInstance.h"
#include "Engine/AI/BehaviorTree/BTSequence.h"
#include <cassert>
#include <iostream>
using namespace NoJob;
int main() {
    int cancellations=0;
    auto registry=std::make_shared<BTNativeRegistry>();
    registry->RegisterAction("LongRunning", [&](BTContext& c, float) {
        c.SetCancellation(2, [&](BTContext&) { ++cancellations; });
        return BTStatus::Running;
    });
    auto seq=std::make_unique<BTSequence>(1);
    seq->AddChild(std::make_unique<BTActionNode>(2,registry,"LongRunning"));
    BTInstance instance(std::make_shared<BTTree>(std::move(seq)));
    assert(instance.Tick(0.1f)==BTStatus::Running);
    assert(instance.Context().LastStatuses().at(2)==static_cast<int>(BTStatus::Running));
    assert(instance.Context().TickCounters().at(2)==1);
    instance.Reset(); assert(cancellations==1);
    instance.Reset(); assert(cancellations==1);
    assert(instance.Context().LastStatuses().empty());
    std::cout << "BTV19Tests: passed\n";
}
