/**
 * @file NeuralNetwork.h
 * @brief Dense neural network inference, training and serialization.
 * @ingroup Neural
 */
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
/** @brief Fully connected neural network layer with an activation function. */
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
/** @brief Parameters controlling supervised neural network optimization. */
struct TrainingOptions {size_t epochs=1000;size_t batchSize=1;float learningRate=0.01f;bool adam=false;uint32_t seed=42;};
/** @brief Per-epoch loss values returned by a training run. */
struct TrainingHistory {std::vector<float> epochLoss;};
/** @brief CPU-based feed-forward neural network with native C++ training. */
class NeuralNetwork {
public:
 explicit NeuralNetwork(uint32_t seed=42):rng_(seed){}
 /** @brief Appends a dense layer to the network.
  * @param inputs Number of expected input features.
  * @param outputs Number of output neurons.
  * @param activation Activation function used by this layer. */
 void AddDense(size_t inputs,size_t outputs,Activation activation);
 /** @brief Evaluates the network without changing its parameters.
  * @param input Input feature vector, matching the first layer width.
  * @return Output activations from the final layer. */
 std::vector<float> Predict(const std::vector<float>&input)const;
 /** @brief Trains on paired input and target samples.
  * @param inputs Training observations.
  * @param targets Expected outputs for each observation.
  * @param options Training hyperparameters.
  * @param onEpoch Optional progress callback; its return value can control continuation.
  * @return Recorded training losses. */
 TrainingHistory Train(const std::vector<std::vector<float>>& inputs,const std::vector<std::vector<float>>& targets,const TrainingOptions& options={}, const std::function<bool(size_t,float)>& onEpoch={});
 float MeanSquaredError(const std::vector<std::vector<float>>& inputs,const std::vector<std::vector<float>>& targets)const;
 /** @brief Serializes the network to a model asset.
  * @param path Destination file path. */
 void Save(const std::string&path)const;
 /** @brief Loads a serialized network.
  * @param path Source model path.
  * @return Restored network. */
 static NeuralNetwork Load(const std::string&path);
 const std::vector<DenseLayer>& Layers()const{return layers_;}
 std::vector<DenseLayer>& Layers(){return layers_;}
private:std::vector<DenseLayer> layers_;std::mt19937 rng_;
};
}
