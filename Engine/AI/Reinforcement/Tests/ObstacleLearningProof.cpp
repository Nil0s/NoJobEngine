/** @file ObstacleLearningProof.cpp @brief Reproduce reward-driven learning from scratch. */
#include "Engine/AI/Reinforcement/ObstacleEvaluation.h"
#include "Engine/AI/Reinforcement/ObstacleRefinement.h"
#include <iostream>
#include <fstream>
/** @brief Train 6000 base episodes plus 3000 refinement episodes; save only on pass. */
int main(int argc,char**argv){
 try{
    using namespace NoJob::RL;
    ObstacleEnvironment env;DQNAgent agent(8,4,ObstacleOptions());
    Train(env,agent,6000,64,42,[](size_t n,const EpisodeMetrics&m){if(n%1000==0)std::cout<<"episode="<<n<<" loss="<<m.loss<<std::endl;return true;});
    ObstacleReport settings;settings.policy=argc>1?argv[1]:"RLObstaclePolicy.njn";
    auto refined=RefineObstaclePolicy(agent.Policy(),[](size_t n,const EpisodeMetrics&m){if(n%500==0)std::cout<<"refinement="<<n<<" loss="<<m.loss<<std::endl;return true;});
    if(!refined)return 5;
    const auto r=EvaluateObstacles(*refined,settings);std::cout<<r.CSV();
    if(r.metrics[0].Rate()<.9||r.metrics[0].Rate()<r.metrics[1].Rate()+.3||r.metrics[0].Rate()<r.metrics[2].Rate()+.3)return 2;
    refined->Save(settings.policy);
    const auto loaded=NoJob::Neural::NeuralNetwork::Load(settings.policy);
    if(EvaluateObstacles(loaded,settings).CSV()!=r.CSV())return 3;
    if(argc>2){std::ofstream out(argv[2]);out<<r.CSV();if(!out)return 4;}
    std::cout<<"PASS: learned obstacle avoidance, separate evaluation seeds and reload parity"<<std::endl;
    return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
}
