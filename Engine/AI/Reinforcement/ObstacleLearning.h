/** @file ObstacleLearning.h @brief Reward-driven discrete X/Z obstacle avoidance. */
#pragma once
#include "Engine/AI/Reinforcement/ReinforcementLearning.h"
#include <array>
namespace NoJob::RL {
/** @brief Integer cell on the fixed obstacle-learning arena. */
struct GridCell { int x=0,z=0; bool operator==(const GridCell& b)const{return x==b.x&&z==b.z;} };
/** @brief Shared training/runtime rules; action selection never calls a planner. */
class ObstacleEnvironment final : public Environment {
public:
    static constexpr int Size=9;
    static constexpr float Spacing=1.6f;
    static constexpr int Budget=64;
    /** @brief True for the known static walls and arena boundaries. */
    static bool Blocked(GridCell p) {
        return p.x<0||p.z<0||p.x>=Size||p.z>=Size ||
            (p.x==4 && p.z>=2 && p.z<=6) || ((p.x==6||p.x==7)&&p.z==1);
    }
    /** @brief Apply a cardinal direction, without a safety filter or alternate action. */
    static GridCell Candidate(GridCell p,int action) {
        if(action<0||action>=4) throw std::invalid_argument("Invalid obstacle action");
        if(action==0)--p.x; if(action==1)++p.x; if(action==2)--p.z; if(action==3)++p.z;
        return p;
    }
    /** @brief Encode position, relative goal and four immediate obstacle sensors. */
    static std::vector<float> Observe(GridCell p,GridCell goal) {
        std::vector<float> obs={p.x/4.f-1.f,p.z/4.f-1.f,(goal.x-p.x)/8.f,(goal.z-p.z)/8.f};
        for(int a=0;a<4;++a)obs.push_back(Blocked(Candidate(p,a))?1.f:0.f);
        return obs;
    }
    /** @brief Eight scalar neural observations. */
    size_t ObservationSize()const override{return 8;}
    /** @brief Four cardinal actions; no action mask is applied. */
    int ActionCount()const override{return 4;}
    /** @brief Sample starts/goals from the fixed map, using no pathfinding or teacher. */
    std::vector<float> Reset(uint32_t seed)override {
        std::mt19937 rng(seed);std::uniform_int_distribution<int> cell(0,8);
        do{position_={cell(rng),cell(rng)};}while(Blocked(position_));
        do{goal_={cell(rng),cell(rng)};}while(Blocked(goal_)||std::abs(goal_.x-position_.x)+std::abs(goal_.z-position_.z)<4);
        ticks_=0;collisions_=0;return Observe(position_,goal_);
    }
    /** @brief Reward goal progress/arrival; penalize blocked attempts and elapsed steps. */
    StepResult Step(int action)override {
        const float before=Distance();
        const auto next=Candidate(position_,action);
        const bool hit=Blocked(next);
        if(hit)++collisions_;else position_=next;
        ++ticks_;
        const bool success=position_==goal_;
        return {Observe(position_,goal_),success?5.f:-.03f+.3f*(before-Distance())-(hit?.7f:0.f),success||ticks_>=Budget};
    }
    /** @brief Restore an explicit valid episode for runtime parity and regressions. */
    void SetEpisode(GridCell start,GridCell goal) {
        if(Blocked(start)||Blocked(goal))throw std::invalid_argument("Blocked obstacle episode");
        position_=start;goal_=goal;ticks_=0;collisions_=0;
    }
    /** @brief Current lattice position. */
    GridCell Position()const{return position_;}
    /** @brief Current lattice goal. */
    GridCell Goal()const{return goal_;}
    /** @brief Blocked action attempts; these do not move the agent. */
    int Collisions()const{return collisions_;}
    /** @brief Arrival at the goal cell. */
    bool Success()const{return position_==goal_;}
    /** @brief Euclidean distance in lattice cells. */
    float Distance()const{return std::hypot(float(position_.x-goal_.x),float(position_.z-goal_.z));}
private:GridCell position_,goal_;int ticks_=0,collisions_=0;
};
/** @brief Greedy raw-network action; blocked actions remain visible and are penalized. */
inline int ObstacleAction(const Neural::NeuralNetwork& net,const std::vector<float>& obs) {
    const auto q=net.Predict(obs);
    if(q.size()!=4||!std::all_of(q.begin(),q.end(),[](float v){return std::isfinite(v);}))throw std::runtime_error("Invalid obstacle Q values");
    return int(std::distance(q.begin(),std::max_element(q.begin(),q.end())));
}
/** @brief Reproducible hyperparameters used by the editor and proof executable. */
inline Options ObstacleOptions() {
    Options o;o.hiddenUnits=64;o.epsilonDecay=.9993f;o.learningRate=.0005f;
    o.gamma=.97f;o.replayCapacity=12000;o.targetSyncSteps=200;return o;
}
}
