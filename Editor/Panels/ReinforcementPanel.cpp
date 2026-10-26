/** @file ReinforcementPanel.cpp @brief ImGui RL training and diagnostics. @ingroup Panels */
#include "ReinforcementPanel.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <exception>
#include <cfloat>
namespace NoJob {
ReinforcementPanel::~ReinforcementPanel() { Stop(); }
void ReinforcementPanel::Stop() {
    cancel_=true;
    if(worker_.joinable()) worker_.join();
    running_=false;
}
void ReinforcementPanel::Start() {
    if(running_) return;
    Stop();
    if(episodes_<1 || maxSteps_<1 || batch_<1 || hidden_<1 ||
       learningRate_<=0 || gamma_<0 || gamma_>1) { status_="Invalid RL settings"; return; }
    std::filesystem::path path;
    try { path=NeuralAssetPath(modelName_); }
    catch(const std::exception& e) { status_=e.what(); return; }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rewards_.clear(); losses_.clear(); status_="Training...";
    }
    cancel_=false; completed_=0; running_=true;
    const int episodes=episodes_, maxSteps=maxSteps_, batch=batch_, hidden=hidden_;
    const float rate=learningRate_, gamma=gamma_;
    worker_=std::thread([this,path,episodes,maxSteps,batch,hidden,rate,gamma] {
        try {
            RL::Options options;
            options.batchSize=static_cast<size_t>(batch);
            options.hiddenUnits=static_cast<size_t>(hidden);
            options.learningRate=rate; options.gamma=gamma;
            options.warmup=std::max<size_t>(static_cast<size_t>(batch),64);
            RL::TrackingEnvironment env;
            RL::DQNAgent agent(env.ObservationSize(),env.ActionCount(),options);
            RL::Train(env,agent,static_cast<size_t>(episodes),static_cast<size_t>(maxSteps),42,
                [this](size_t episode,const RL::EpisodeMetrics& metrics) {
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        rewards_.push_back(metrics.totalReward);
                        losses_.push_back(metrics.loss);
                    }
                    completed_=static_cast<int>(episode);
                    return !cancel_.load();
                });
            if(!cancel_) {
                std::filesystem::create_directories(path.parent_path());
                agent.SavePolicy(path.string());
            }
            std::lock_guard<std::mutex> lock(mutex_);
            status_=cancel_?"Training cancelled":"Policy saved: "+path.string();
        } catch(const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_); status_=e.what();
        }
        running_=false;
    });
}
void ReinforcementPanel::Draw() {
    if(!open_) return;
    if(!ImGui::Begin("Reinforcement Learning",&open_)) { ImGui::End(); return; }
    ImGui::TextWrapped("Native Double DQN - discrete actions, replay memory, target network and episodic rewards.");
    ImGui::TextDisabled("Demo environment: one-dimensional target tracking (left / stay / right).");
    ImGui::InputText("Policy name",modelName_,sizeof(modelName_));
    ImGui::InputInt("Episodes",&episodes_);
    ImGui::InputInt("Max steps",&maxSteps_);
    ImGui::InputInt("Replay batch",&batch_);
    ImGui::InputInt("Hidden units",&hidden_);
    ImGui::InputFloat("Learning rate",&learningRate_,0,0,"%.5f");
    ImGui::SliderFloat("Discount gamma",&gamma_,0.f,1.f);
    if(!running_) { if(ImGui::Button("Train and save policy")) Start(); }
    else { if(ImGui::Button("Cancel training")) cancel_=true; }
    ImGui::SameLine(); ImGui::Text("Episode %d / %d",completed_.load(),episodes_);
    std::vector<float> rewards,losses; std::string status;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rewards=rewards_; losses=losses_; status=status_;
    }
    if(!rewards.empty()) {
        ImGui::PlotLines("Episode reward",rewards.data(),static_cast<int>(rewards.size()),0,nullptr,
                         FLT_MAX,FLT_MAX,ImVec2(0,130));
        ImGui::PlotLines("Training loss",losses.data(),static_cast<int>(losses.size()),0,nullptr,
                         FLT_MAX,FLT_MAX,ImVec2(0,130));
        const int window=std::min<int>(20,static_cast<int>(rewards.size()));
        float sum=0.f;
        for(int i=static_cast<int>(rewards.size())-window;i<static_cast<int>(rewards.size());++i) sum+=rewards[i];
        ImGui::Text("Last %d episodes average reward: %.3f",window,sum/window);
    }
    ImGui::TextWrapped("%s",status.c_str());
    ImGui::End();
}
}
