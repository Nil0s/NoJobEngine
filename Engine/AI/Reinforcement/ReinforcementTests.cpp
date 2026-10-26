/** @file ReinforcementTests.cpp @brief Standalone RL regression tests. @ingroup ReinforcementLearning */
#include "ReinforcementLearning.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
int main() {
    using namespace NoJob::RL;
    TrackingEnvironment env;
    auto state=env.Reset(42);
    assert(state.size()==2 && env.ActionCount()==3);
    bool threw=false;
    try { env.Step(3); } catch(const std::invalid_argument&) { threw=true; }
    assert(threw);
    ReplayBuffer buffer(2);
    buffer.Push({{0,0},0,0,{0,0},false});
    buffer.Push({{0,0},1,0,{0,0},false});
    buffer.Push({{0,0},2,0,{0,0},false});
    assert(buffer.Size()==2);
    std::mt19937 rng(42);
    assert(buffer.Sample(2,rng).size()==2);
    Options options;
    options.batchSize=4; options.warmup=4; options.hiddenUnits=8; options.targetSyncSteps=5;
    DQNAgent agent(2,3,options);
    assert(agent.Act({0,0},true)>=0 && agent.Act({0,0},true)<3);
    for(int i=0;i<10;++i) agent.Observe({{0,0},1,1.f,{0,0},true});
    assert(agent.ReplaySize()==10 && std::isfinite(agent.LastLoss()));
    const auto path=(std::filesystem::temp_directory_path()/"nojob_rl_test.njn").string();
    agent.SavePolicy(path);
    DQNAgent loaded(2,3,options);
    loaded.LoadPolicy(path);
    assert(loaded.Act({0,0},true)==agent.Act({0,0},true));
    std::filesystem::remove(path);
    int callbacks=0;
    Train(env,agent,4,10,100,[&](size_t,const EpisodeMetrics& m){
        assert(m.steps>0 && std::isfinite(m.totalReward)); ++callbacks; return true;
    });
    assert(callbacks==4);
    std::cout<<"NoJob RL tests passed\n";
}
