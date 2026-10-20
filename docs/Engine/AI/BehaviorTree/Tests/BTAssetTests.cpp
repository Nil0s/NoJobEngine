#include "Engine/AI/BehaviorTree/BTAsset.h"
#include "Engine/AI/BehaviorTree/BTInstance.h"
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace NoJob;
int main() {
    auto a=BTAsset::Default();
    auto r=std::make_shared<BTNativeRegistry>();
    r->RegisterCondition("CanSeeTarget",[](BTContext&){return true;});
    r->RegisterAction("ChaseTarget",[](BTContext&,float){return BTStatus::Success;});
    r->RegisterAction("StopMoving",[](BTContext&,float){return BTStatus::Success;});
    const auto path=std::filesystem::temp_directory_path()/"nojob_v19_asset_test.njbt";
    a.Save(path); auto loaded=BTAsset::Load(path); std::filesystem::remove(path);
    assert(loaded.Nodes.size()==a.Nodes.size());
    BTInstance instance(loaded.Build(r));
    assert(instance.Tick(0.01f)==BTStatus::Success);
    loaded.Nodes[1].Parent=999;
    bool failed=false; try {loaded.Validate();} catch(const std::exception&) {failed=true;}
    assert(failed);
    std::cout << "BTAssetTests PASS\n";
}
