#include "NeuralNetwork.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <filesystem>
using namespace NoJob::Neural;
int main(){
 Matrix a(2,2);a(0,0)=1;a(0,1)=2;a(1,0)=3;a(1,1)=4;auto b=Matrix::Multiply(a,a);assert(b(0,0)==7&&b(1,1)==22);
 NeuralNetwork n(7);n.AddDense(2,4,Activation::Tanh);n.AddDense(4,1,Activation::Sigmoid);
 const std::vector<std::vector<float>> xs={{0,0},{0,1},{1,0},{1,1}},ys={{0},{1},{1},{0}};
 float before=n.MeanSquaredError(xs,ys);TrainingOptions o;o.epochs=3000;o.batchSize=4;o.learningRate=.04f;o.adam=true;auto history=n.Train(xs,ys,o);float after=n.MeanSquaredError(xs,ys);
 std::cout<<"XOR MSE "<<before<<" -> "<<after<<"\n";assert(after<0.02f);assert(history.epochLoss.size()==o.epochs);
 auto path=(std::filesystem::temp_directory_path()/"nojob_neural_test.njn").string();n.Save(path);auto loaded=NeuralNetwork::Load(path);for(auto&x:xs)assert(std::abs(n.Predict(x)[0]-loaded.Predict(x)[0])<1e-6f);std::filesystem::remove(path);
 bool rejected=false;try{n.Predict({1});}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
 std::cout<<"ALL NEURAL TESTS PASSED\n";
}
