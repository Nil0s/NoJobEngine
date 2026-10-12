#include "Engine/AI/BehaviorTree/BTAsset.h"
#include "Engine/AI/BehaviorTree/BTInstance.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace NoJob;
int main() {
    BTBlackboard board;
    int changes=0;
    auto id=board.Observe("target",[&](std::string_view){++changes;});
    board.Set("target",1); board.Set("other",3); board.Remove("target");
    assert(changes==2 && board.Unobserve(id));
    board.Set("target",4); assert(changes==2);

    auto registry=std::make_shared<BTNativeRegistry>();
    registry->RegisterCondition("See",[](BTContext& c){return c.Blackboard().GetOr<bool>("visible",false);});
    int chase=0, idle=0, cancels=0, services=0;
    registry->RegisterAction("Chase",[&](BTContext& c,float){++chase; c.SetCancellation(c.ActiveNode(),[&](BTContext&){++cancels;}); return BTStatus::Running;});
    registry->RegisterAction("Idle",[&](BTContext&,float){++idle;return BTStatus::Running;});
    registry->RegisterAction("Sense",[&](BTContext&,float){++services;return BTStatus::Success;});
    BTAsset a;
    a.Nodes={{1,0,"Selector","-"},{2,1,"Decorator","See","visible"},
             {3,2,"Action","Chase"},{4,1,"Action","Idle"}};
    BTInstance agent(a.Build(registry));
    agent.Context().Blackboard().Set("visible",false);
    assert(agent.Tick(.1f)==BTStatus::Running && idle==1 && chase==0);
    agent.Context().Blackboard().Set("visible",true);
    assert(agent.Tick(.1f)==BTStatus::Running && chase==1);
    agent.Context().Blackboard().Set("visible",false);
    assert(agent.Tick(.1f)==BTStatus::Running && idle==2 && cancels==1);

    BTAsset periodic;
    periodic.Nodes={{10,0,"Service","Sense","-",.2f},{11,10,"Action","Idle"}};
    BTInstance p(periodic.Build(registry));
    p.Tick(.1f); assert(services==0);
    p.Tick(.1f); assert(services==1);
    p.Tick(.1f); assert(services==1);
    p.Tick(.1f); assert(services==2);

    BTAsset parallel;
    parallel.Nodes={{20,0,"Parallel","-"},{21,20,"Action","Chase"},{22,20,"Action","Idle"}};
    BTInstance concurrent(parallel.Build(registry));
    int before=idle; concurrent.Tick(.1f); assert(idle==before+1);

    const auto file=std::filesystem::temp_directory_path()/"nojob_bt_v2_test.njbt";
    a.Save(file); auto loaded=BTAsset::Load(file); std::filesystem::remove(file);
    assert(loaded.Nodes.size()==a.Nodes.size() && loaded.Nodes[1].Parameter=="visible");
    std::cout << "BTReactiveTests PASS\n";
}
