/** @file ObstacleRefinement.h @brief Reward-driven curriculum for deterministic policy stalls. */
#pragma once
#include "ObstacleLearning.h"
#include <optional>
#include "TrainingTelemetry.h"
namespace NoJob::RL {
/** @brief A failed finite-arena start/goal pair; no teacher action is supplied. */
struct ObstacleFailure {GridCell start,goal;};
/** @brief Audit every distinct free-cell pair, detecting deterministic state cycles. */
inline std::vector<ObstacleFailure> FindObstacleFailures(const Neural::NeuralNetwork& policy){
    std::vector<ObstacleFailure> failed;
    for(int sz=0;sz<9;++sz)for(int sx=0;sx<9;++sx){GridCell start{sx,sz};if(ObstacleEnvironment::Blocked(start))continue;
        for(int gz=0;gz<9;++gz)for(int gx=0;gx<9;++gx){GridCell goal{gx,gz};if(ObstacleEnvironment::Blocked(goal)||start==goal)continue;
            ObstacleEnvironment env;env.SetEpisode(start,goal);std::array<bool,81> seen{};
            for(int t=0;t<64;++t){auto p=env.Position();if(seen[p.z*9+p.x])break;seen[p.z*9+p.x]=true;
                const auto r=env.Step(ObstacleAction(policy,ObstacleEnvironment::Observe(p,goal)));if(r.terminated)break;}
            if(!env.Success())failed.push_back({start,goal});
        }
    }return failed;
}
/** @brief Refine with a balanced mix of random episodes and the learner's own failures.
 * @details One in four episodes revisits a failed pair. Failures are refreshed every
 * 500 episodes; actions still use epsilon-greedy DQN and the original rewards only.
 * @return Refined weights, or no result when cancelled. No graph/path teacher is used.
 */
inline std::optional<Neural::NeuralNetwork> RefineObstaclePolicy(const Neural::NeuralNetwork& initial,
    const std::function<bool(size_t,const EpisodeMetrics&)>& callback={},
    const std::function<bool(const TrainingStep&)>& stepCallback={}){
    Options o=ObstacleOptions();o.seed=137;o.epsilonStart=.2f;o.epsilonDecay=.9993f;
    o.learningRate=.00003f;o.batchSize=64;o.warmup=512;o.targetSyncSteps=500;
    o.hiddenUnits=initial.Layers().front().Outputs();DQNAgent agent(8,4,o);agent.LoadPolicy(initial);
    auto failed=FindObstacleFailures(agent.Policy());std::mt19937 rng(731);ObstacleEnvironment env;
    for(int episode=0;episode<3000;++episode){
        std::vector<float> observation;
        if(episode%4==0&&!failed.empty()){
            const auto p=failed[std::uniform_int_distribution<size_t>(0,failed.size()-1)(rng)];
            env.SetEpisode(p.start,p.goal);observation=ObstacleEnvironment::Observe(p.start,p.goal);
        }else observation=env.Reset(700000+episode);
        EpisodeMetrics metrics;
        for(int t=0;t<64;++t){
            const auto before=observation;
            const auto q=stepCallback?agent.Policy().Predict(before):std::vector<float>{};
            const int action=agent.Act(observation);auto r=env.Step(action);
            agent.Observe({observation,action,r.reward,r.observation,r.terminated});observation=r.observation;
            metrics.totalReward+=r.reward;metrics.steps=t+1;
            if(stepCallback){TrainingStep frame;frame.phase=1;frame.episode=episode+1;frame.step=t+1;frame.action=action;
                frame.position=env.Position();frame.goal=env.Goal();frame.collisions=env.Collisions();frame.reward=r.reward;
                frame.totalReward=metrics.totalReward;frame.epsilon=agent.Epsilon();frame.loss=agent.LastLoss();
                frame.observations=before;frame.q=q;frame.terminal=r.terminated;
                if(!stepCallback(frame))return std::nullopt;}
            if(r.terminated)break;
        }
        agent.EndEpisode();metrics.epsilon=agent.Epsilon();metrics.loss=agent.LastLoss();
        if((episode+1)%500==0)failed=FindObstacleFailures(agent.Policy());
        if(callback&&!callback(episode+1,metrics))return std::nullopt;
    }return agent.Policy();
}
}
