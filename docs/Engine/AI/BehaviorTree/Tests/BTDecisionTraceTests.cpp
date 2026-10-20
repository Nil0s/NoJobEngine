#include "Engine/AI/BehaviorTree/BTContext.h"
#include "Engine/AI/BehaviorTree/BTAsset.h"
#include <cassert>
#include <iostream>
int main() {
    NoJob::BTContext ctx;
    ctx.BeginTick();
    ctx.RecordStatus(3, 2);
    assert(ctx.CurrentStatuses().at(3) == 2);
    ctx.SetCancellation(3, [](NoJob::BTContext&){});
    ctx.BeginTick();
    assert(ctx.CurrentStatuses().empty());
    ctx.CancelNode(3);
    assert(ctx.Events().back().Kind == "Aborted");
    ctx.Blackboard().Set("TargetVisible", true);
    ctx.BeginTick();
    ctx.RecordStatus(4, 0);
    assert(ctx.Events().back().Kind == "Succeeded");
    const auto asset=NoJob::BTAsset::Load("Assets/AI/GuardChasePatrol.njbt");
    assert(asset.Nodes.size()==5);
    std::cout << "BTDecisionTraceTests PASS\n";
}
