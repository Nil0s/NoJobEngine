#pragma once
#include "Engine/AI/Neural/NeuralNetwork.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace NoJob {
// Editor-only UI. Neural inference/training remains independent of ImGui.
class NeuralEditorPanel {
public:
    NeuralEditorPanel();
    ~NeuralEditorPanel();
    NeuralEditorPanel(const NeuralEditorPanel&) = delete;
    NeuralEditorPanel& operator=(const NeuralEditorPanel&) = delete;
    bool IsOpen() const { return open_; }
    void SetOpen(bool open) { open_ = open; }
    void Draw();
private:
    void Build();
    void StartTraining();
    void ResetTrainingSamples();
    void StopTraining();
    void DrawGraph();
    void DrawInspector();
    void DrawTraining();
    void DrawPredictions();
    void FinishWorker();
    bool open_ = false;
    Neural::NeuralNetwork network_;
    std::vector<int> widths_{2, 4, 1};
    std::vector<int> activations_{3, 2}; // Tanh, Sigmoid
    int selectedLayer_ = 0;
    int epochs_ = 3000;
    int batchSize_ = 4;
    float learningRate_ = 0.04f;
    bool adam_ = true;
    char assetName_[256] = "Untitled";
    std::string currentName_ = "Untitled";
    std::string assetMessage_;
    bool dirty_ = true;
    bool restoreAttempted_ = false;
    void LoadNamed(const std::string& name);
    void SaveNamed(const std::string& name, bool saveAs);
    void RememberAsset(const std::string& name) const;
    void RestoreLastAsset();
    char error_[512] = "";
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<int> currentEpoch_{0};
    std::mutex workerMutex_;
    std::vector<float> losses_;
    Neural::NeuralNetwork trained_;
    bool resultReady_ = false;
    std::string workerError_;
    std::vector<float> inputs_{0.f, 1.f};
    std::vector<std::vector<float>> trainingX_, trainingY_;
    std::vector<float> lastPrediction_;
    std::vector<std::vector<float>> lastActivations_;
};
}
