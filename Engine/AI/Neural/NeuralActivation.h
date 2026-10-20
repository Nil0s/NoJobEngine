#pragma once
#include <cmath>
#include <stdexcept>
namespace NoJob::Neural {
enum class Activation {Linear,ReLU,Sigmoid,Tanh};
inline float Activate(float x,Activation a){switch(a){case Activation::Linear:return x;case Activation::ReLU:return x>0?x:0;case Activation::Sigmoid:return x>=0?1.f/(1.f+std::exp(-x)):std::exp(x)/(1.f+std::exp(x));case Activation::Tanh:return std::tanh(x);}throw std::invalid_argument("Activation");}
inline float DerivativeFromOutput(float y,Activation a){switch(a){case Activation::Linear:return 1;case Activation::ReLU:return y>0?1.f:0.f;case Activation::Sigmoid:return y*(1-y);case Activation::Tanh:return 1-y*y;}throw std::invalid_argument("Activation");}
}
