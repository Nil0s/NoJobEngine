/** @file ObstacleEvaluation.h @brief Held-out learned/untrained/random/NavMesh comparisons. */
#pragma once
#include "ObstacleLearning.h"
#include <functional>
#include <sstream>
#include <iomanip>
#include <locale>
#include <cstring>
namespace NoJob::RL {
/** @brief Outcome for one comparator on one episode. */
struct ObstacleOutcome { bool success=false; int steps=0,collisions=0; double distance=0.,pathLength=0.; };
/** @brief Aggregate success, collision attempts and travel efficiency. */
struct ObstacleMetrics {
    int episodes=0,successes=0,collisions=0,arrivalSteps=0;
    double distance=0.,pathLength=0.;
    /** @brief Accumulate an episode without hiding failed trajectories. */
    void Add(const ObstacleOutcome& r){++episodes;successes+=r.success;collisions+=r.collisions;distance+=r.distance;pathLength+=r.pathLength;if(r.success)arrivalSteps+=r.steps;}
    /** @brief Fraction of successful episodes. */
    double Rate()const{return episodes?double(successes)/episodes:0.;}
};
/** @brief Immutable report: same fixed arena and starts/goals for every comparator. */
struct ObstacleReport {
    std::string policy,fingerprint;
    uint32_t seed=500000;
    int scenarios=500;
    bool complete=false,navigation=false;
    std::array<ObstacleMetrics,4> metrics; ///< Learned, fresh network, random, native NavMesh.
    /** @brief CSV includes protocol, units, seeds, collision semantics and all outcomes. */
    std::string CSV()const{
        std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9);
        out<<"protocol,obstacle_grid_v1\nmap,fixed_9x9_two_walls\npolicy,\"";
        for(char c:policy){if(c=='"')out<<'"';out<<c;}
        out<<"\"\nmodel_fnv1a64,"<<fingerprint<<"\nevaluation_pairs_may_overlap_training,1\nscenario_seed,"<<seed<<"\nrandom_seed,90210\ninitialization_seed,42\nscenarios,"<<scenarios
           <<"\nbudget,64\nstep_world_units,1.6\nnavmesh_available,"<<navigation
           <<"\ncomplete,"<<complete<<"\ncollision_metric,blocked_action_attempts_or_invalid_baseline_segment"
           <<"\narrival_steps_population,successful_episodes_only\ndistance_units,world_XZ"
           <<"\ncomparator,episodes,successes,success_rate,blocked_attempts,mean_arrival_steps,mean_final_distance,mean_path_length\n";
        const char* names[]={"learned","untrained","random","native_navmesh"};
        for(size_t i=0;i<metrics.size();++i){if(i==3&&!navigation)continue;const auto&m=metrics[i];
            out<<names[i]<<','<<m.episodes<<','<<m.successes<<','<<m.Rate()<<','<<m.collisions<<',';
            if(m.successes)out<<double(m.arrivalSteps)/m.successes;else out<<"N/A";
            out<<','<<(m.episodes?m.distance/m.episodes:0.)<<','<<(m.episodes?m.pathLength/m.episodes:0.)<<'\n';
        }return out.str();
    }
};
/** @brief Run identical held-out episodes; optional callback uses the real engine NavMesh.
 * @details Evaluation never changes weights. Cancellation yields an incomplete report.
 */
inline ObstacleReport EvaluateObstacles(const Neural::NeuralNetwork& policy,ObstacleReport report={},
    const std::function<ObstacleOutcome(GridCell,GridCell)>& navigation={},
    const std::function<bool(size_t)>& progress={}) {
    if(policy.Layers().empty()||policy.Layers().front().Inputs()!=8||policy.Layers().back().Outputs()!=4||report.scenarios<1||report.scenarios>10000)
        throw std::invalid_argument("Obstacle policy requires 8 inputs and 4 Q outputs");
    report.metrics={};report.complete=false;report.navigation=bool(navigation);
    uint64_t hash=14695981039346656037ull;
    auto word=[&](uint64_t v){for(int i=0;i<8;++i){hash^=(v>>(i*8))&255;hash*=1099511628211ull;}};
    auto scalar=[&](float v){uint32_t bits=0;std::memcpy(&bits,&v,sizeof(bits));word(bits);};
    for(const auto&l:policy.Layers()){word(l.Inputs());word(l.Outputs());word(static_cast<uint64_t>(l.Function()));for(float v:l.Weights().Data())scalar(v);for(float v:l.Biases())scalar(v);}
    std::ostringstream id;id<<std::hex<<hash;report.fingerprint=id.str();
    Neural::NeuralNetwork fresh(42);for(const auto& l:policy.Layers())fresh.AddDense(l.Inputs(),l.Outputs(),l.Function());
    std::mt19937 random(90210);
    for(int episode=0;episode<report.scenarios;++episode){
        for(int kind=0;kind<4;++kind){
            if(kind==3&&!navigation)continue;
            ObstacleEnvironment env;auto obs=env.Reset(report.seed+episode);
            ObstacleOutcome result;
            if(kind==3)result=navigation(env.Position(),env.Goal());
            else for(int step=0;step<ObstacleEnvironment::Budget;++step){
                const int action=kind==2?std::uniform_int_distribution<int>(0,3)(random):ObstacleAction(kind==0?policy:fresh,obs);
                const auto before=env.Position();const auto r=env.Step(action);obs=r.observation;
                if(!(before==env.Position()))result.pathLength+=ObstacleEnvironment::Spacing;
                result.steps=step+1;result.success=env.Success();result.collisions=env.Collisions();result.distance=env.Distance()*ObstacleEnvironment::Spacing;
                if(r.terminated)break;
            }
            report.metrics[kind].Add(result);
        }
        if(progress&&!progress(episode+1))return report;
    }
    report.complete=true;return report;
}
}
