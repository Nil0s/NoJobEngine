/** @file ReinforcementPanel.h @brief Native RL training controls. @ingroup Panels */
#pragma once
#include "Engine/AI/Reinforcement/ReinforcementLearning.h"
#include "Engine/AI/Reinforcement/TrackingEvaluation.h"
#include "Engine/AI/Reinforcement/ObstacleEvaluation.h"
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
    /** @brief Report active worker jobs without touching their mutable state. */
    bool IsRunning() const { return running_.load(); }
    void SetOpen(bool value) { open_=value; }
    void Draw();
private:
    void Start();
    void Stop();
    /** @brief Evaluate an immutable copy of the saved policy on a background worker. */
    void Evaluate();
    /** @brief Export the completed report to the project Reports/RL directory. */
    void SaveReport();
    /** @brief Refresh available policies while preserving the selected full path. */
    void RefreshPolicies();
    /** @brief Inspect the selected file and automatically select its supported benchmark. */
    void SelectEvaluationPolicy(const std::filesystem::path& path);
    int evaluationTask_=-1; ///< Detected benchmark; independent from training settings.
    std::vector<std::filesystem::path> policyFiles_;
    std::filesystem::path evaluationPolicy_; ///< Independent from the training output name.
    bool policiesLoaded_=false;
    int task_=0; ///< Zero: axis tracking; one: learned obstacle avoidance.
    RL::ObstacleReport obstacleReport_; ///< Guarded by mutex_.
    bool open_=false;
    int episodes_=1600, maxSteps_=40, batch_=32, hidden_=32;
    float learningRate_=.001f, gamma_=.97f;
    char modelName_[128]="RLTrackingPolicy";
    std::atomic<bool> running_{false}, cancel_{false};
    std::atomic<int> completed_{0};
    std::thread worker_;
    std::mutex mutex_;
    std::vector<float> rewards_, losses_;
    std::string status_;
    bool evaluating_=false; ///< UI-thread job type; settings are frozen during work.
    RL::TrackingReport report_; ///< Access guarded by mutex_.
    std::filesystem::path reportDirectory_; ///< Captured at evaluation start.
    std::string reportStatus_; ///< Export feedback, UI thread only.
};
}
