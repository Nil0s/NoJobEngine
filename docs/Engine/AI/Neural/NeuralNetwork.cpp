#include "NeuralNetwork.h"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace NoJob::Neural {
DenseLayer::DenseLayer(size_t in,size_t out,Activation a,std::mt19937&rng):weights_(out,in),biases_(out,0.f),activation_(a){float limit=std::sqrt(6.f/static_cast<float>(in+out));std::uniform_real_distribution<float> dist(-limit,limit);for(auto&w:weights_.Data())w=dist(rng);}
std::vector<float> DenseLayer::Forward(const std::vector<float>&x)const{if(x.size()!=Inputs())throw std::invalid_argument("Dense input dimensions");std::vector<float> y(Outputs());for(size_t o=0;o<Outputs();++o){float v=biases_[o];for(size_t i=0;i<Inputs();++i)v+=weights_(o,i)*x[i];y[o]=Activate(v,activation_);}return y;}
void NeuralNetwork::AddDense(size_t in,size_t out,Activation a){if(!layers_.empty()&&layers_.back().Outputs()!=in)throw std::invalid_argument("Incompatible layers");layers_.emplace_back(in,out,a,rng_);}
std::vector<float> NeuralNetwork::Predict(const std::vector<float>&x)const{if(layers_.empty())throw std::logic_error("Empty network");auto v=x;for(const auto&l:layers_)v=l.Forward(v);return v;}
float NeuralNetwork::MeanSquaredError(const std::vector<std::vector<float>>&xs,const std::vector<std::vector<float>>&ys)const{if(xs.empty()||xs.size()!=ys.size())throw std::invalid_argument("Dataset dimensions");double sum=0;size_t n=0;for(size_t k=0;k<xs.size();++k){auto pred=Predict(xs[k]);if(pred.size()!=ys[k].size())throw std::invalid_argument("Target dimensions");for(size_t i=0;i<pred.size();++i){double d=pred[i]-ys[k][i];sum+=d*d;++n;}}return static_cast<float>(sum/n);}
TrainingHistory NeuralNetwork::Train(const std::vector<std::vector<float>>&xs,const std::vector<std::vector<float>>&ys,const TrainingOptions&opt,const std::function<bool(size_t,float)>&onEpoch){
 if(layers_.empty()||xs.empty()||xs.size()!=ys.size()||!opt.batchSize||!std::isfinite(opt.learningRate)||opt.learningRate<=0)throw std::invalid_argument("Invalid training setup");
 for(size_t k=0;k<xs.size();++k)if(xs[k].size()!=layers_.front().Inputs()||ys[k].size()!=layers_.back().Outputs())throw std::invalid_argument("Training sample shape");
 struct State{std::vector<float> mw,vw,mb,vb;};std::vector<State> state(layers_.size());
 for(size_t l=0;l<layers_.size();++l){auto& s=state[l];s.mw.resize(layers_[l].Weights().Data().size());s.vw=s.mw;s.mb.resize(layers_[l].Outputs());s.vb=s.mb;}
 std::vector<size_t> order(xs.size());std::iota(order.begin(),order.end(),0);std::mt19937 shuffle(opt.seed);TrainingHistory history;history.epochLoss.reserve(opt.epochs);size_t step=0;
 for(size_t epoch=0;epoch<opt.epochs;++epoch){std::shuffle(order.begin(),order.end(),shuffle);
 for(size_t start=0;start<order.size();start+=opt.batchSize){size_t count=std::min(opt.batchSize,order.size()-start);
 std::vector<std::vector<float>> gw(layers_.size()),gb(layers_.size());for(size_t l=0;l<layers_.size();++l){gw[l].resize(layers_[l].Weights().Data().size());gb[l].resize(layers_[l].Outputs());}
 for(size_t sample=start;sample<start+count;++sample){size_t k=order[sample];std::vector<std::vector<float>> acts;acts.reserve(layers_.size()+1);acts.push_back(xs[k]);for(const auto&layer:layers_)acts.push_back(layer.Forward(acts.back()));
 std::vector<float> delta(acts.back().size());for(size_t j=0;j<delta.size();++j)delta[j]=2.f*(acts.back()[j]-ys[k][j])/static_cast<float>(delta.size())*DerivativeFromOutput(acts.back()[j],layers_.back().Function());
 for(size_t l=layers_.size();l-->0;){auto& layer=layers_[l];for(size_t o=0;o<layer.Outputs();++o){gb[l][o]+=delta[o];for(size_t i=0;i<layer.Inputs();++i)gw[l][o*layer.Inputs()+i]+=delta[o]*acts[l][i];}
 if(l){std::vector<float> previous(layer.Inputs());for(size_t i=0;i<previous.size();++i){float sum=0;for(size_t o=0;o<layer.Outputs();++o)sum+=layer.Weights()(o,i)*delta[o];previous[i]=sum*DerivativeFromOutput(acts[l][i],layers_[l-1].Function());}delta=std::move(previous);}}
 }
 ++step;for(size_t l=0;l<layers_.size();++l){auto update=[&](std::vector<float>&params,const std::vector<float>&grads,std::vector<float>&m,std::vector<float>&v){for(size_t i=0;i<params.size();++i){float g=grads[i]/static_cast<float>(count);if(opt.adam){m[i]=.9f*m[i]+.1f*g;v[i]=.999f*v[i]+.001f*g*g;float mh=m[i]/(1.f-std::pow(.9f,static_cast<float>(step)));float vh=v[i]/(1.f-std::pow(.999f,static_cast<float>(step)));params[i]-=opt.learningRate*mh/(std::sqrt(vh)+1e-8f);}else params[i]-=opt.learningRate*g;}};
 auto&s=state[l];update(layers_[l].Weights().Data(),gw[l],s.mw,s.vw);update(layers_[l].Biases(),gb[l],s.mb,s.vb);}}
 float loss=MeanSquaredError(xs,ys);if(!std::isfinite(loss))throw std::runtime_error("Non-finite loss");history.epochLoss.push_back(loss);if(onEpoch && !onEpoch(epoch+1,loss))break;
 }return history;
}
void NeuralNetwork::Save(const std::string&path)const{if(layers_.empty())throw std::logic_error("Empty network");std::ofstream f(path,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("Cannot save neural asset");f<<"NJN 1 "<<layers_.size()<<'\n';f.precision(std::numeric_limits<float>::max_digits10);for(const auto&l:layers_){f<<l.Inputs()<<' '<<l.Outputs()<<' '<<static_cast<int>(l.Function())<<'\n';for(float x:l.Weights().Data())f<<x<<' ';f<<'\n';for(float x:l.Biases())f<<x<<' ';f<<'\n';}if(!f)throw std::runtime_error("Neural asset write failed");}
NeuralNetwork NeuralNetwork::Load(const std::string&path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot open neural asset");std::string magic;int version=0;size_t count=0;if(!(f>>magic>>version>>count)||magic!="NJN"||version!=1||!count||count>1024)throw std::runtime_error("Invalid neural asset header");NeuralNetwork net;for(size_t l=0;l<count;++l){size_t in=0,out=0;int activation=0;if(!(f>>in>>out>>activation)||!in||!out||in>100000||out>100000||in>10000000/out||activation<0||activation>3)throw std::runtime_error("Invalid neural layer");net.AddDense(in,out,static_cast<Activation>(activation));auto&layer=net.layers_.back();for(auto&w:layer.Weights().Data())if(!(f>>w)||!std::isfinite(w))throw std::runtime_error("Invalid weight");for(auto&b:layer.Biases())if(!(f>>b)||!std::isfinite(b))throw std::runtime_error("Invalid bias");}return net;}
}
