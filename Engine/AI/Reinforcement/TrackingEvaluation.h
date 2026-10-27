/** @file TrackingEvaluation.h @brief Held-out X/Z evaluation and CSV reporting. */
#pragma once
#include "TrackingPolicy.h"
#include <array>
#include <functional>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <locale>
namespace NoJob::RL {
/** @brief Aggregate metrics; arrival steps exclude failed phases. */
struct TrackingMetrics {
    size_t phases=0, successes=0, arrivalSteps=0;
    double finalDistance=0.;
    /** @brief Fraction of phases that reach the target. */
    double SuccessRate() const { return phases?double(successes)/phases:0.; }
    /** @brief Average decisions until arrival among successful phases only. */
    double MeanArrivalSteps() const { return successes?double(arrivalSteps)/successes:0.; }
    /** @brief Average final Euclidean distance over every phase. */
    double MeanDistance() const { return phases?finalDistance/phases:0.; }
};
/** @brief Evaluation settings and results, independent of the live scene. */
struct TrackingReport {
    std::string policy, fingerprint, topology;
    uint32_t scenarioSeed=100000, randomSeed=90210, initializationSeed=42;
    size_t scenarios=500, budget=40;
    bool complete=false;
    std::array<TrackingMetrics,3> metrics; ///< Saved policy, fresh network, uniform random.
    /** @brief Serialize metrics, units, seeds and failure semantics as CSV. */
    std::string CSV() const {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out<<std::setprecision(9)<<"protocol,tracking_xz_v1\npolicy,\"";
        for(char c:policy) { if(c=='"') out<<'"'; out<<c; }
        out<<"\"\nmodel_fnv1a64,"<<fingerprint<<"\ntopology,"<<topology<<"\ncomplete,"<<complete<<"\nscenario_seed,"<<scenarioSeed
           <<"\nrandom_seed,"<<randomSeed<<"\ninitialization_seed,"<<initializationSeed
           <<"\nscenarios,"<<scenarios<<"\nsteps_per_phase,"<<budget
           <<"\nphases_per_scenario,2\narrival_tolerance_per_axis,0.08\nstep_per_axis,0.12"
           <<"\ndistance_units,normalized_euclidean_XZ\narrival_steps_population,successful_phases_only"
           <<"\ncomparator,phases,successes,success_rate,mean_arrival_steps,mean_final_distance\n";
        const char* names[]={"saved","untrained","random"};
        for(size_t i=0;i<metrics.size();++i) {
            const auto& m=metrics[i];
            out<<names[i]<<','<<m.phases<<','<<m.successes<<','<<m.SuccessRate()<<',';
            if(m.successes) out<<m.MeanArrivalSteps(); else out<<"N/A";
            out<<','<<m.MeanDistance()<<'\n';
        }
        return out.str();
    }
};
/** @brief Compare a saved policy with a fresh equal-topology network and uniform random.
 * @details All policies share layouts, bounds and arrival hold. A second target is
 * relocated after the first budget, retaining each policy's final position.
 * @param progress Called after each scenario; false cancels without a complete report.
 */
inline TrackingReport EvaluateTracking(const Neural::NeuralNetwork& policy, TrackingReport report={},
                                      const std::function<bool(size_t)>& progress={}) {
    if(policy.Layers().empty() || policy.Layers().front().Inputs()!=2 ||
       policy.Layers().back().Outputs()!=3 || !report.scenarios || report.scenarios>10000 ||
       !report.budget || report.budget>1000)
        throw std::invalid_argument("Expected 2-input/3-action tracking policy and bounded settings");
    report.metrics={}; report.complete=false;
    // Identify the evaluated topology and weights even if the source file is replaced later.
    uint64_t hash=14695981039346656037ull;
    auto hashWord=[&](uint64_t word) { for(int i=0;i<8;++i) { hash^=(word>>(i*8))&255; hash*=1099511628211ull; } };
    auto hashFloat=[&](float value) { uint32_t bits=0; std::memcpy(&bits,&value,sizeof(bits)); hashWord(bits); };
    report.topology.clear();
    for(const auto& l:policy.Layers()) {
        hashWord(l.Inputs()); hashWord(l.Outputs()); hashWord(static_cast<uint64_t>(l.Function()));
        if(!report.topology.empty()) report.topology+=';';
        report.topology+=std::to_string(l.Inputs())+"x"+std::to_string(l.Outputs())+":"+std::to_string(static_cast<int>(l.Function()));
        for(float v:l.Weights().Data()) hashFloat(v);
        for(float v:l.Biases()) hashFloat(v);
    }
    std::ostringstream identity; identity<<std::hex<<hash; report.fingerprint=identity.str();
    Neural::NeuralNetwork fresh(report.initializationSeed);
    for(const auto& l:policy.Layers()) fresh.AddDense(l.Inputs(),l.Outputs(),l.Function());
    std::mt19937 actions(report.randomSeed);
    for(size_t episode=0;episode<report.scenarios;++episode) {
        std::mt19937 layout(report.scenarioSeed+static_cast<uint32_t>(episode));
        std::uniform_real_distribution<float> coord(-.9f,.9f);
        const float initialX=coord(layout),initialZ=coord(layout);
        std::array<float,2> gx{},gz{};
        for(size_t phase=0;phase<2;++phase) { gx[phase]=coord(layout); gz[phase]=coord(layout); }
        for(size_t kind=0;kind<3;++kind) {
            float x=initialX,z=initialZ;
            auto& m=report.metrics[kind];
            for(size_t phase=0;phase<2;++phase) {
                size_t arrival=0;
                for(size_t tick=0;tick<report.budget;++tick) {
                    if(kind==2) {
                        if(std::abs(gx[phase]-x)>=.08f) x=std::clamp(x+(std::uniform_int_distribution<int>(0,2)(actions)-1)*.12f,-1.f,1.f);
                        if(std::abs(gz[phase]-z)>=.08f) z=std::clamp(z+(std::uniform_int_distribution<int>(0,2)(actions)-1)*.12f,-1.f,1.f);
                    } else {
                        const auto& net=kind==0?policy:fresh;
                        x=TrackAxis(net,x,gx[phase]).next;
                        z=TrackAxis(net,z,gz[phase]).next;
                    }
                    if(!arrival && std::abs(gx[phase]-x)<.08f && std::abs(gz[phase]-z)<.08f) arrival=tick+1;
                }
                ++m.phases;
                if(arrival) { ++m.successes; m.arrivalSteps+=arrival; }
                m.finalDistance+=std::hypot(gx[phase]-x,gz[phase]-z);
            }
        }
        if(progress && !progress(episode+1)) return report;
    }
    report.complete=true;
    return report;
}
}
