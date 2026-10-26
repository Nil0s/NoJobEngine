/** @file ReinforcementPanel.h @brief Native RL training controls. @ingroup Panels */
#pragma once
#include "Engine/AI/Reinforcement/ReinforcementLearning.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace NoJob {
/** @brief Editor window for training, inspecting and saving native RL policies. */
class ReinforcementPanel {
public:
    ~ReinforcementPanel();
    bool IsOpen() const { return open_; }
    void SetOpen(bool value) { open_=value; }
    void Draw();
private:
    void Start();
    void Stop();
    bool open_=false;
    int episodes_=400, maxSteps_=40, batch_=32, hidden_=32;
    float learningRate_=.001f, gamma_=.97f;
    char modelName_[128]="RLTrackingPolicy";
    std::atomic<bool> running_{false}, cancel_{false};
    std::atomic<int> completed_{0};
    std::thread worker_;
    std::mutex mutex_;
    std::vector<float> rewards_, losses_;
    std::string status_;
};
}
