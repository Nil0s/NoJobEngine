/** @file TrackingPolicy.h @brief Shared axis inference for separable X/Z tracking. */
#pragma once
#include "ReinforcementLearning.h"
namespace NoJob::RL {
/** @brief One axis decision, including the exact observation and raw Q values. */
struct AxisDecision {
    std::vector<float> observation, q;
    int action = 1;
    float next = 0.f;
};
/** @brief Apply a trained 2-input/3-action policy to one normalized axis.
 * @details X and Z share weights. Inside the terminal radius the axis holds;
 * outside it movement always comes from the network's greedy action.
 */
inline AxisDecision TrackAxis(const Neural::NeuralNetwork& model, float position, float target) {
    if (!std::isfinite(position) || !std::isfinite(target))
        throw std::invalid_argument("Non-finite tracking position");
    position=std::clamp(position,-1.f,1.f);
    target=std::clamp(target,-1.f,1.f);
    AxisDecision d;
    d.observation={position,target-position};
    d.q=model.Predict(d.observation);
    if(d.q.size()!=3 || !std::all_of(d.q.begin(),d.q.end(),[](float v){return std::isfinite(v);}))
        throw std::runtime_error("Invalid tracking Q values");
    d.action=std::abs(target-position)<.08f ? 1 :
        static_cast<int>(std::distance(d.q.begin(),std::max_element(d.q.begin(),d.q.end())));
    d.next=std::clamp(position+float(d.action-1)*.12f,-1.f,1.f);
    return d;
}
}
