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
inline Neural::TrainingHistory TrainDirectionModel(const std::string& name, int epochs, float learningRate) {
    const auto path=NeuralAssetPath(name);
    Neural::NeuralNetwork net(42);
    net.AddDense(2,8,Neural::Activation::Tanh);
    net.AddDense(8,2,Neural::Activation::Tanh);
    std::vector<std::vector<float>> xs,ys;
    for(int x=-8;x<=8;++x) for(int z=-8;z<=8;++z) {
        const float dx=x/8.f,dz=z/8.f,length=std::sqrt(dx*dx+dz*dz);
        xs.push_back({dx,dz});
        ys.push_back(length>1e-5f ? std::vector<float>{dx/length,dz/length} : std::vector<float>{0.f,0.f});
    }
    Neural::TrainingOptions options;
    options.epochs=static_cast<size_t>(std::clamp(epochs,1,20000));
    options.learningRate=std::clamp(learningRate,0.00001f,0.1f);
    options.batchSize=16; options.adam=true; options.seed=42;
    auto history=net.Train(xs,ys,options);
    std::filesystem::create_directories(path.parent_path());
    net.Save(path.string());
    return history;
}
} // namespace NoJob::NeuralAgent
