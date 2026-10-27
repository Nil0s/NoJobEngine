/** @file ObstacleWorldPolicy.h @brief Explicit composition of learned core/exterior policies. */
#pragma once
#include "ObstacleLearning.h"
#include "TrackingPolicy.h"
#include <functional>
namespace NoJob::RL {
inline constexpr int WorldCellMin=-26,WorldCellMax=34,WorldActionBudget=192;
inline constexpr float LearnedWorldLimit=48.f;
/** @brief The obstacle model's original trained core. */
inline bool IsObstacleCore(GridCell p){return p.x>=0&&p.x<=8&&p.z>=0&&p.z<=8;}
/** @brief Safe floor domain, with margin inside testplane_100x100.obj. */
inline bool InLearnedWorld(GridCell p){return p.x>=WorldCellMin&&p.z>=WorldCellMin&&p.x<=WorldCellMax&&p.z<=WorldCellMax;}
/** @brief A raw action from one of the learned controllers, before physical collision checks. */
struct ObstacleWorldDecision {
    GridCell next,proxyGoal;
    bool exterior=false,blocked=false;
    int action=-1,xAction=1,zAction=1;
    float reward=0;
    std::vector<float> observations,q;
};
/** @brief Use obstacle DQN in its core and learned axis tracking on the open exterior.
 * @details The controller switch and boundary projection are explicit engineering
 * rules, not an end-to-end learned hierarchy. Neither actor uses NavMesh or a route.
 * The exterior is obstacle-free in the validated arena. At the core boundary, the
 * tracking actor continues toward the true goal instead of treating the proxy as arrival.
 */
inline ObstacleWorldDecision DecideObstacleWorld(const Neural::NeuralNetwork& obstacle,const Neural::NeuralNetwork& tracking,
    GridCell position,GridCell goal,const std::function<bool(GridCell)>& sensor={}){
    if(!InLearnedWorld(position)||!InLearnedWorld(goal))
        throw std::invalid_argument("Invalid learned-world domain");
    ObstacleWorldDecision d;d.next=position;
    d.proxyGoal={std::clamp(goal.x,0,8),std::clamp(goal.z,0,8)};
    d.exterior=!IsObstacleCore(position)||(position==d.proxyGoal&&!(position==goal));
    if(d.exterior){
        const auto x=TrackAxis(tracking,0.f,(goal.x-position.x)*.12f);
        const auto z=TrackAxis(tracking,0.f,(goal.z-position.z)*.12f);
        d.xAction=x.action;d.zAction=z.action;
        d.next={position.x+x.action-1,position.z+z.action-1};
        d.observations=x.observation;d.observations.insert(d.observations.end(),z.observation.begin(),z.observation.end());
        d.q=x.q;d.q.insert(d.q.end(),z.q.begin(),z.q.end());
        if(!InLearnedWorld(d.next)){d.next=position;d.blocked=true;}
    }else{
        d.observations=ObstacleEnvironment::Observe(position,d.proxyGoal);
        if(sensor)for(int a=0;a<4;++a)d.observations[4+a]=sensor(ObstacleEnvironment::Candidate(position,a))?1.f:0.f;
        d.q=obstacle.Predict(d.observations);
        d.action=ObstacleAction(obstacle,d.observations);ObstacleEnvironment env;env.SetEpisode(position,d.proxyGoal);
        const auto r=env.Step(d.action);d.next=env.Position();d.reward=r.reward;d.blocked=d.next==position;
    }return d;
}
}
