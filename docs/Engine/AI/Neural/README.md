# NoJobEngine — Native Neural Networks (development foundation)

This module is written in C++20 and uses only the standard library. Namespace: `NoJob::Neural`.

## Implemented
- Contiguous row-major Matrix, multiplication and transpose.
- Dense feedforward layers with Linear, ReLU, Sigmoid and Tanh.
- Multilayer forward inference.
- MSE loss, backpropagation, minibatch SGD and Adam.
- Deterministic initialization/training seeds.
- Versioned `.njn` text assets (architecture, weights, biases).
- Independent XOR, matrix, asset round-trip and invalid-input tests.

## Example
```cpp
#include "Engine/AI/Neural/NeuralNetwork.h"
using namespace NoJob::Neural;
NeuralNetwork net(7);
net.AddDense(2, 4, Activation::Tanh);
net.AddDense(4, 1, Activation::Sigmoid);
TrainingOptions options;
options.epochs = 3000;
options.batchSize = 4;
options.learningRate = 0.04f;
options.adam = true;
auto history = net.Train({{0,0},{0,1},{1,0},{1,1}}, {{0},{1},{1},{0}}, options);
net.Save("xor.njn");
auto restored = NeuralNetwork::Load("xor.njn");
auto output = restored.Predict({0,1});
```

## Standalone validation
```sh
g++ -std=c++20 -O2 Engine/AI/Neural/NeuralNetwork.cpp Engine/AI/Neural/NeuralTests.cpp -o neural_tests
./neural_tests
```

## Important limitations / remaining V2.0 scope
- **Not yet integrated** with NoJobEditor visual neural graph, inspector, AssetManager, Blackboard, Behavior Trees, Perception or runtime components.
- No cross-entropy loss or classification-specific training objective yet.
- Training runs synchronously; editor-safe worker jobs and cancellation are not implemented.
- `.njn` is an initial text asset format, not a hardened untrusted-input loader or final asset-pipeline format.
- No batch inference optimizations, GPU acceleration, gradient checking, or benchmarks.
- Doxygen documentation and public API review remain for the V2.0 finishing stage.
- Standalone C++ tests pass on Linux; full Windows/CMake/editor compilation was not run here.
