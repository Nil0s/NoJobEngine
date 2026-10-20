#pragma once
#include "Engine/AI/Neural/NeuralNetwork.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

namespace NoJob::NeuralAgent {
struct Snapshot {
    std::string model, status;
    std::vector<float> inputs, outputs;
    std::vector<std::vector<float>> activations;
    float distance = 0.f;
};
inline std::mutex& Mutex() { static std::mutex m; return m; }
inline std::unordered_map<unsigned int, Snapshot>& Snapshots() { static std::unordered_map<unsigned int, Snapshot> m; return m; }
inline void Publish(unsigned int entity, Snapshot snapshot) { std::lock_guard<std::mutex> lock(Mutex()); Snapshots()[entity]=std::move(snapshot); }
inline bool Read(unsigned int entity, Snapshot& out) { std::lock_guard<std::mutex> lock(Mutex()); auto it=Snapshots().find(entity); if(it==Snapshots().end()) return false; out=it->second; return true; }
inline void Clear(unsigned int entity) { std::lock_guard<std::mutex> lock(Mutex()); Snapshots().erase(entity); }
inline std::vector<std::vector<float>> Activations(const Neural::NeuralNetwork& net, const std::vector<float>& input) {
    std::vector<std::vector<float>> result{input}; auto values=input;
    for(const auto& layer:net.Layers()) {
        if(values.size()!=layer.Inputs()) throw std::runtime_error("Neural input dimensions mismatch");
        std::vector<float> next(layer.Outputs());
        for(size_t i=0;i<layer.Outputs();++i) {
            float sum=layer.Biases()[i];
            for(size_t j=0;j<values.size();++j) sum+=layer.Weights()(i,j)*values[j];
            next[i]=Neural::Activate(sum,layer.Function());
        }
        result.push_back(next); values=std::move(next);
    }
    return result;
}
// Reusable supervised training presets. The agent's active model is never changed
// by training; source and destination assets are separate and explicit.
enum class TrainingPreset { FollowXZ = 0, TrackY = 1, Pong = 2 };
inline const char* PresetLabel(TrainingPreset p) {
    switch (p) { case TrainingPreset::FollowXZ: return "Follow target (XZ)";
                 case TrainingPreset::TrackY: return "Track target (Y)";
                 default: return "Pong paddle (supervised)"; }
}
inline Neural::TrainingHistory TrainPreset(const std::string& outputName,
        const std::string& sourceName, TrainingPreset preset,
        int epochs, float learningRate, bool resume, bool overwrite) {
    const auto output = NeuralAssetPath(outputName); // also validates name
    if (resume && sourceName.empty()) throw std::runtime_error("Select a source model to resume training");
    if (std::filesystem::exists(output) && !overwrite)
        throw std::runtime_error("Output already exists. Choose another Save As name or enable Overwrite Output");
    Neural::NeuralNetwork net(42);
    const size_t nIn = preset == TrainingPreset::FollowXZ ? 2 : (preset == TrainingPreset::TrackY ? 3 : 6);
    const size_t nOut = preset == TrainingPreset::FollowXZ ? 2 : 1;
    if (resume) {
        net = Neural::NeuralNetwork::Load(NeuralAssetPath(sourceName).string());
        if (net.Layers().empty() || net.Layers().front().Inputs()!=nIn || net.Layers().back().Outputs()!=nOut)
            throw std::runtime_error("Source model dimensions do not match the selected training preset");
    } else if (preset == TrainingPreset::FollowXZ) {
        net.AddDense(2,8,Neural::Activation::Tanh);
        net.AddDense(8,2,Neural::Activation::Tanh);
    } else if (preset == TrainingPreset::TrackY) {
        net.AddDense(3,12,Neural::Activation::Tanh);
        net.AddDense(12,1,Neural::Activation::Tanh);
    } else {
        net.AddDense(6,12,Neural::Activation::Tanh);
        net.AddDense(12,8,Neural::Activation::Tanh);
        net.AddDense(8,1,Neural::Activation::Tanh);
    }
    std::vector<std::vector<float>> xs, ys;
    if (preset == TrainingPreset::FollowXZ) {
        for (int x=-8;x<=8;++x) for (int z=-8;z<=8;++z) {
            const float dx=x/8.f,dz=z/8.f,len=std::sqrt(dx*dx+dz*dz);
            xs.push_back({dx,dz});
            ys.push_back(len>1e-5f?std::vector<float>{dx/len,dz/len}:std::vector<float>{0.f,0.f});
        }
    } else if (preset == TrainingPreset::TrackY) {
        // Inputs: normalized target Y, self Y, relative Y. Output: vertical direction.
        for(int target=-10;target<=10;++target) for(int self=-10;self<=10;++self) {
            float t=target/10.f, p=self/10.f, d=t-p;
            xs.push_back({t,p,std::clamp(d,-1.f,1.f)});
            ys.push_back({std::clamp(d*3.f,-1.f,1.f)});
        }
    } else {
        // Inputs: ball X/Y, velocity X/Y, paddle Y, horizontal distance.
        // Labels use a deterministic intercept heuristic, not reinforcement learning.
        for(int i=0;i<6000;++i) {
            const float bx=static_cast<float>((i*37)%199-99)/100.f;
            const float by=static_cast<float>((i*53)%199-99)/100.f;
            const float vx=static_cast<float>((i*17)%161-80)/100.f;
            const float vy=static_cast<float>((i*29)%161-80)/100.f;
            const float py=static_cast<float>((i*71)%199-99)/100.f;
            const float dx=std::clamp((1.f-bx)*0.5f,0.f,1.f);
            const float time=vx>0.05f ? std::min(3.f,(1.f-bx)/vx) : 0.f;
            float intercept=by+vy*time;
            // Reflect off normalized +/-1 playfield bounds.
            while(intercept>1.f || intercept< -1.f) {
                if(intercept>1.f) intercept=2.f-intercept;
                if(intercept< -1.f) intercept=-2.f-intercept;
            }
            xs.push_back({bx,by,vx,vy,py,dx});
            ys.push_back({std::clamp((intercept-py)*3.f,-1.f,1.f)});
        }
    }
    Neural::TrainingOptions options;
    options.epochs=static_cast<size_t>(std::clamp(epochs,1,20000));
    options.learningRate=std::clamp(learningRate,0.00001f,0.1f);
    options.batchSize=16; options.adam=true; options.seed=42;
    auto history=net.Train(xs,ys,options);
    std::filesystem::create_directories(output.parent_path());
    net.Save(output.string());
    return history;
}
// Backward compatible entry point for the original V2.0.6 follow-agent demo.
inline Neural::TrainingHistory TrainDirectionModel(const std::string& name, int epochs, float learningRate,
                                                  bool resume=false, bool overwrite=false) {
    return TrainPreset(name, name, TrainingPreset::FollowXZ, epochs, learningRate, resume, overwrite);
}
} // namespace NoJob::NeuralAgent
