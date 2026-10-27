/** @file TrackingLearningProof.cpp @brief Reproducible native DQN learning and moving-target regression. */
#include "TrackingEvaluation.h"
#include <iostream>
#include <iomanip>

/** @brief Use the same evaluation implementation as the editor panel. */
double Evaluate(const NoJob::Neural::NeuralNetwork& policy, bool random) {
    const auto report=NoJob::RL::EvaluateTracking(policy);
    return report.metrics[random?2:0].SuccessRate();
}

/** @brief Train from scratch, compare identical held-out layouts, and save only on pass.
 * @param argv Optional destination policy path (default RLTrackingPolicy.njn).
 * @return Nonzero when learning fails the declared acceptance thresholds.
 */
int main(int argc,char** argv) {
    try {
        using namespace NoJob::RL;
        Options options;
        options.seed=42;
        TrackingEnvironment env;
        DQNAgent agent(2,3,options);
        const double before=Evaluate(agent.Policy(),false);
        const double random=Evaluate(agent.Policy(),true);
        Train(env,agent,1600,40,42,[](size_t episode,const EpisodeMetrics& m){
            if(episode%200==0) std::cout<<"episode="<<episode<<" reward="<<m.totalReward<<" loss="<<m.loss<<std::endl;
            return true;
        });
        const double after=Evaluate(agent.Policy(),false);
        std::cout<<std::fixed<<std::setprecision(3)<<"XZ phase success: untrained="<<before
                 <<" random="<<random<<" trained="<<after<<std::endl;
        if(after<.9 || after<random+.3 || after<before+.3) return 2;
        const std::string path=argc>1?argv[1]:"RLTrackingPolicy.njn";
        agent.SavePolicy(path);
        DQNAgent loaded(2,3,options); loaded.LoadPolicy(path);
        if(Evaluate(loaded.Policy(),false)!=after) return 3;
        // Moving a reached goal must produce a new observation and a fresh action.
        const auto left=TrackAxis(loaded.Policy(),0.f,-.8f);
        const auto right=TrackAxis(loaded.Policy(),0.f,.8f);
        if(left.next>=0.f || right.next<=0.f || left.observation==right.observation) return 4;
        std::cout<<"PASS: learning, reload parity, and bidirectional retargeting"<<std::endl;
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<std::endl; return 1; }
}
