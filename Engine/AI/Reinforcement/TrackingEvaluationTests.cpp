/** @file TrackingEvaluationTests.cpp @brief Determinism, cancellation, metrics and CSV regressions. */
#include "TrackingEvaluation.h"
#include <fstream>
#include <iostream>
/** @brief Fail a regression in all build configurations. */
void Require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
/** @brief Validate the evaluator; optionally evaluate an asset and write its CSV report. */
int main(int argc,char** argv) {
    try {
        using namespace NoJob::RL;
        DQNAgent initial(2,3);
        TrackingReport settings; settings.scenarios=12; settings.policy="a,\"quoted\" policy";
        auto first=EvaluateTracking(initial.Policy(),settings);
        auto second=EvaluateTracking(initial.Policy(),settings);
        Require(first.complete && first.CSV()==second.CSV(),"Evaluation must be deterministic");
        Require(first.metrics[0].successes==first.metrics[1].successes &&
                first.metrics[0].finalDistance==first.metrics[1].finalDistance,
                "Fresh equal-topology comparator must match an identically seeded untrained policy");
        for(const auto& m:first.metrics) {
            Require(m.phases==24 && m.successes<=m.phases,"Invalid phase counts");
            Require(std::isfinite(m.MeanDistance()) && m.MeanDistance()>=0. && m.MeanDistance()<3.,"Invalid distance");
            if(m.successes) Require(m.MeanArrivalSteps()>=1. && m.MeanArrivalSteps()<=40.,"Invalid arrival metric");
        }
        Require(first.CSV().find("\"a,\"\"quoted\"\" policy\"")!=std::string::npos,"CSV escaping failed");
        auto cancelled=EvaluateTracking(initial.Policy(),settings,[](size_t){return false;});
        Require(!cancelled.complete && cancelled.metrics[0].phases==2,"Cancellation must return an incomplete report");
        settings.scenarios=0;
        bool threw=false; try { EvaluateTracking(initial.Policy(),settings); } catch(const std::invalid_argument&) {threw=true;}
        Require(threw,"Invalid evaluation budget accepted");
        NoJob::Neural::NeuralNetwork bad; bad.AddDense(4,3,NoJob::Neural::Activation::Linear);
        threw=false; try { EvaluateTracking(bad); } catch(const std::invalid_argument&) {threw=true;}
        Require(threw,"Incompatible policy accepted");
        if(argc>1) {
            auto policy=NoJob::Neural::NeuralNetwork::Load(argv[1]);
            TrackingReport full; full.policy=argv[1];
            auto result=EvaluateTracking(policy,full);
            Require(result.metrics[0].SuccessRate()>=.9 &&
                    result.metrics[0].SuccessRate()>=result.metrics[1].SuccessRate()+.3 &&
                    result.metrics[0].SuccessRate()>=result.metrics[2].SuccessRate()+.3,"Saved policy failed learning threshold");
            std::cout<<result.CSV();
            if(argc>2) { std::ofstream out(argv[2]); out<<result.CSV(); out.close(); Require(bool(out),"CSV export failed"); }
        }
        std::cout<<"PASS: evaluation metrics, determinism, cancellation, invalid input and CSV"<<std::endl;
    } catch(const std::exception& e) { std::cerr<<e.what()<<std::endl; return 1; }
}
