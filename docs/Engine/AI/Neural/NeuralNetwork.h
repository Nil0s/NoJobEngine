#pragma once
#include "NeuralMatrix.h"
#include "NeuralActivation.h"
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
namespace NoJob::Neural {
class DenseLayer {
public:
 DenseLayer(size_t inputs,size_t outputs,Activation activation,std::mt19937& rng);
 std::vector<float> Forward(const std::vector<float>&input)const;
 size_t Inputs()const{return weights_.Cols();} size_t Outputs()const{return weights_.Rows();}
 Activation Function()const{return activation_;}
 Matrix& Weights(){return weights_;} const Matrix& Weights()const{return weights_;}
 std::vector<float>& Biases(){return biases_;} const std::vector<float>& Biases()const{return biases_;}
private:Matrix weights_;std::vector<float> biases_;Activation activation_;
};
struct TrainingOptions {size_t epochs=1000;size_t batchSize=1;float learningRate=0.01f;bool adam=false;uint32_t seed=42;};
struct TrainingHistory {std::vector<float> epochLoss;};
class NeuralNetwork {
public:
 explicit NeuralNetwork(uint32_t seed=42):rng_(seed){}
 void AddDense(size_t inputs,size_t outputs,Activation activation);
 std::vector<float> Predict(const std::vector<float>&input)const;
 TrainingHistory Train(const std::vector<std::vector<float>>& inputs,const std::vector<std::vector<float>>& targets,const TrainingOptions& options={}, const std::function<bool(size_t,float)>& onEpoch={});
 float MeanSquaredError(const std::vector<std::vector<float>>& inputs,const std::vector<std::vector<float>>& targets)const;
 void Save(const std::string&path)const;
 static NeuralNetwork Load(const std::string&path);
 const std::vector<DenseLayer>& Layers()const{return layers_;}
 std::vector<DenseLayer>& Layers(){return layers_;}
private:std::vector<DenseLayer> layers_;std::mt19937 rng_;
};
}
