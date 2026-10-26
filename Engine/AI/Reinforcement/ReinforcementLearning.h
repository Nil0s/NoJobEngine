/**
 * @file ReinforcementLearning.h
 * @brief Native discrete-action reinforcement learning environments and Double DQN.
 * @ingroup ReinforcementLearning
 */
#pragma once
#include "Engine/AI/Neural/NeuralNetwork.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <limits>
#include <random>
#include <utility>
#include <stdexcept>
#include <string>
#include <vector>

namespace NoJob::RL {
/** @brief One transition in a Markov decision process. */
struct Transition {
    std::vector<float> observation;
    int action = 0;
    float reward = 0.f;
    std::vector<float> nextObservation;
    bool terminal = false;
};
/** @brief Environment response after applying an action. */
struct StepResult {
    std::vector<float> observation;
    float reward = 0.f;
    bool terminated = false;
};
/** @brief Interface for episodic, discrete-action environments. */
class Environment {
public:
    virtual ~Environment() = default;
    /** @brief Reset the episode and return the initial observation. */
    virtual std::vector<float> Reset(uint32_t seed) = 0;
    /** @brief Apply one discrete action. */
    virtual StepResult Step(int action) = 0;
    /** @brief Number of scalar observation features. */
    virtual size_t ObservationSize() const = 0;
    /** @brief Number of available discrete actions. */
    virtual int ActionCount() const = 0;
};
/** @brief Bounded replay memory sampled uniformly without replacement. */
class ReplayBuffer {
public:
    explicit ReplayBuffer(size_t capacity) : capacity_(capacity) {
        if (!capacity) throw std::invalid_argument("Replay capacity must be positive");
    }
    void Push(Transition transition) {
        if (data_.size() == capacity_) data_.pop_front();
        data_.push_back(std::move(transition));
    }
    size_t Size() const { return data_.size(); }
    std::vector<Transition> Sample(size_t count, std::mt19937& rng) const {
        if (count > data_.size()) throw std::invalid_argument("Not enough replay samples");
        std::vector<size_t> ids(data_.size());
        for (size_t i=0;i<ids.size();++i) ids[i]=i;
        std::shuffle(ids.begin(),ids.end(),rng);
        std::vector<Transition> batch; batch.reserve(count);
        for (size_t i=0;i<count;++i) batch.push_back(data_[ids[i]]);
        return batch;
    }
private:
    size_t capacity_;
    std::deque<Transition> data_;
};
/** @brief Hyperparameters for native Double DQN optimization. */
struct Options {
    size_t replayCapacity = 10000;
    size_t batchSize = 32;
    size_t warmup = 128;
    size_t targetSyncSteps = 100;
    size_t hiddenUnits = 32;
    float gamma = .97f;
    float learningRate = .001f;
    float epsilonStart = 1.f;
    float epsilonEnd = .05f;
    float epsilonDecay = .995f;
    uint32_t seed = 42;
};
/** @brief A completed episode's reward and exploration diagnostics. */
struct EpisodeMetrics {
    float totalReward = 0.f;
    float epsilon = 1.f;
    float loss = 0.f;
    size_t steps = 0;
};
/** @brief Discrete-action Double DQN learner using the existing native neural network. */
class DQNAgent {
public:
    DQNAgent(size_t observationSize, int actions, Options options = {})
        : observations_(observationSize), actions_(actions), options_(options), rng_(options.seed),
          replay_(options.replayCapacity), online_(options.seed), target_(options.seed), epsilon_(options.epsilonStart) {
        if (!observations_ || actions_ < 2 || actions_ > 128 || !options_.batchSize ||
            !options_.targetSyncSteps || !options_.hiddenUnits || !std::isfinite(options_.gamma) ||
            options_.gamma < 0 || options_.gamma > 1 || !std::isfinite(options_.learningRate) ||
            options_.learningRate <= 0 || options_.epsilonEnd < 0 || options_.epsilonEnd > 1 ||
            options_.epsilonStart < options_.epsilonEnd || options_.epsilonStart > 1 ||
            options_.epsilonDecay <= 0 || options_.epsilonDecay > 1)
            throw std::invalid_argument("Invalid DQN configuration");
        online_.AddDense(observations_, options_.hiddenUnits, Neural::Activation::ReLU);
        online_.AddDense(options_.hiddenUnits, static_cast<size_t>(actions_), Neural::Activation::Linear);
        target_ = online_;
    }
    /** @brief Select an action; greedy inference is deterministic and exploration-free. */
    int Act(const std::vector<float>& observation, bool greedy = false) {
        Validate(observation);
        if (!greedy && std::uniform_real_distribution<float>(0.f,1.f)(rng_) < epsilon_)
            return std::uniform_int_distribution<int>(0,actions_-1)(rng_);
        const auto q = online_.Predict(observation);
        return static_cast<int>(std::distance(q.begin(),std::max_element(q.begin(),q.end())));
    }
    /** @brief Store a transition and run one minibatch update when replay is warm. */
    bool Observe(Transition transition) {
        Validate(transition.observation); Validate(transition.nextObservation);
        if (transition.action < 0 || transition.action >= actions_ || !std::isfinite(transition.reward))
            throw std::invalid_argument("Invalid transition");
        replay_.Push(std::move(transition));
        if (replay_.Size() < std::max(options_.warmup,options_.batchSize)) return false;
        const auto batch = replay_.Sample(options_.batchSize,rng_);
        std::vector<std::vector<float>> xs,ys;
        xs.reserve(batch.size()); ys.reserve(batch.size());
        for (const auto& sample:batch) {
            auto q = online_.Predict(sample.observation);
            float bellman = sample.reward;
            if (!sample.terminal) {
                const auto nextOnline = online_.Predict(sample.nextObservation);
                const auto best = static_cast<size_t>(std::distance(nextOnline.begin(),
                    std::max_element(nextOnline.begin(),nextOnline.end())));
                bellman += options_.gamma * target_.Predict(sample.nextObservation)[best];
            }
            q[static_cast<size_t>(sample.action)] = bellman;
            xs.push_back(sample.observation); ys.push_back(std::move(q));
        }
        Neural::TrainingOptions training;
        training.epochs=1; training.batchSize=options_.batchSize;
        training.learningRate=options_.learningRate; training.adam=true;
        training.seed=options_.seed + static_cast<uint32_t>(updates_);
        auto history=online_.Train(xs,ys,training);
        lastLoss_=history.epochLoss.back();
        if (++updates_ % options_.targetSyncSteps == 0) target_=online_;
        return true;
    }
    /** @brief Decay exploration once per completed episode. */
    void EndEpisode() { epsilon_=std::max(options_.epsilonEnd,epsilon_*options_.epsilonDecay); }
    /** @brief Persist policy in the existing NJN format. */
    void SavePolicy(const std::string& path) const { online_.Save(path); }
    /** @brief Load a policy while verifying observation and action dimensions. */
    void LoadPolicy(const std::string& path) {
        auto loaded=Neural::NeuralNetwork::Load(path);
        if (loaded.Layers().empty() || loaded.Layers().front().Inputs()!=observations_ ||
            loaded.Layers().back().Outputs()!=static_cast<size_t>(actions_))
            throw std::invalid_argument("Policy dimensions mismatch");
        online_=std::move(loaded); target_=online_;
    }
    float Epsilon() const { return epsilon_; }
    float LastLoss() const { return lastLoss_; }
    size_t ReplaySize() const { return replay_.Size(); }
    const Neural::NeuralNetwork& Policy() const { return online_; }
private:
    void Validate(const std::vector<float>& observation) const {
        if (observation.size()!=observations_ ||
            !std::all_of(observation.begin(),observation.end(),[](float v){return std::isfinite(v);}))
            throw std::invalid_argument("Invalid RL observation");
    }
    size_t observations_; int actions_; Options options_; std::mt19937 rng_;
    ReplayBuffer replay_; Neural::NeuralNetwork online_,target_;
    float epsilon_,lastLoss_=0.f; size_t updates_=0;
};
/** @brief Simple one-dimensional target tracking environment for RL validation. */
class TrackingEnvironment final : public Environment {
public:
    size_t ObservationSize() const override { return 2; }
    int ActionCount() const override { return 3; }
    std::vector<float> Reset(uint32_t seed) override {
        std::mt19937 rng(seed);
        position_=std::uniform_real_distribution<float>(-.9f,.9f)(rng);
        target_=std::uniform_real_distribution<float>(-.9f,.9f)(rng);
        ticks_=0; return {position_,target_-position_};
    }
    StepResult Step(int action) override {
        if (action<0 || action>=ActionCount()) throw std::invalid_argument("Invalid tracking action");
        position_=std::clamp(position_ + static_cast<float>(action-1)*.12f,-1.f,1.f);
        const float distance=std::abs(target_-position_);
        ++ticks_;
        return {{position_,target_-position_},distance<.08f?1.f:-distance*.1f,
                distance<.08f || ticks_>=40};
    }
private:
    float position_=0.f,target_=0.f; size_t ticks_=0;
};
/** @brief Train an agent in an episodic environment, reporting each episode. */
template<class Callback>
void Train(Environment& environment, DQNAgent& agent, size_t episodes,
           size_t maxSteps, uint32_t seed, Callback callback) {
    if (!maxSteps) throw std::invalid_argument("Episode step limit must be positive");
    for (size_t episode=0;episode<episodes;++episode) {
        auto observation=environment.Reset(seed+static_cast<uint32_t>(episode));
        EpisodeMetrics metrics;
        for (size_t step=0;step<maxSteps;++step) {
            const int action=agent.Act(observation);
            auto result=environment.Step(action);
            const bool terminal=result.terminated || step+1==maxSteps;
            agent.Observe({observation,action,result.reward,result.observation,terminal});
            metrics.totalReward+=result.reward; metrics.steps=step+1;
            observation=std::move(result.observation);
            if (terminal) break;
        }
        agent.EndEpisode(); metrics.epsilon=agent.Epsilon(); metrics.loss=agent.LastLoss();
        if (!callback(episode+1,metrics)) break;
    }
}
} // namespace NoJob::RL
