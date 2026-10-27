/** @file ReinforcementPanel.cpp @brief ImGui RL training and diagnostics. @ingroup Panels */
#include "ReinforcementPanel.h"
#include "RLPolicyFiles.h"
#include "Editor/Scene/SceneFileDialog.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <exception>
#include <cfloat>
#include <fstream>
#include <chrono>
#include <memory>
#include <cstdio>
#include "Engine/AI/Reinforcement/ObstacleNavBaseline.h"
#include "Engine/AI/Reinforcement/ObstacleRefinement.h"
#include "Engine/AI/Reinforcement/TrainingProcess.h"
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
    evaluating_=false; reportStatus_.clear();
    if(episodes_<1 || maxSteps_<1 || batch_<1 || hidden_<1 ||
       learningRate_<=0 || gamma_<0 || gamma_>1) { status_="Invalid RL settings"; return; }
    std::filesystem::path path;
    try { path=NeuralAssetPath(modelName_); }
    catch(const std::exception& e) { status_=e.what(); return; }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rewards_.clear(); losses_.clear(); status_="Training..."; report_={}; obstacleReport_={};
    }
    cancel_=false; completed_=0; running_=true;
    const int episodes=episodes_, maxSteps=maxSteps_, batch=batch_, hidden=hidden_;
    const float rate=learningRate_, gamma=gamma_;
    const bool obstacles=task_==1;
    worker_=std::thread([this,path,episodes,maxSteps,batch,hidden,rate,gamma,obstacles] {
        try {
            RL::Options options=obstacles?RL::ObstacleOptions():RL::Options{};
            options.batchSize=static_cast<size_t>(batch);
            options.hiddenUnits=static_cast<size_t>(hidden);
            options.learningRate=rate; options.gamma=gamma;
            options.warmup=std::max<size_t>(static_cast<size_t>(batch),128);
            std::unique_ptr<RL::Environment> env;
            if(obstacles)env=std::make_unique<RL::ObstacleEnvironment>();else env=std::make_unique<RL::TrackingEnvironment>();
            RL::DQNAgent agent(env->ObservationSize(),env->ActionCount(),options);
            RL::Train(*env,agent,static_cast<size_t>(episodes),static_cast<size_t>(maxSteps),42,
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
                if(obstacles){
                    {std::lock_guard<std::mutex> lock(mutex_);status_="Refining stalled core cases (3000 reward-driven episodes)...";}
                    auto refined=RL::RefineObstaclePolicy(agent.Policy(),[this,episodes](size_t n,const RL::EpisodeMetrics&m){
                        {std::lock_guard<std::mutex> lock(mutex_);rewards_.push_back(m.totalReward);losses_.push_back(m.loss);}
                        completed_=episodes+static_cast<int>(n);return !cancel_.load();
                    });
                    if(refined&&!cancel_)refined->Save(path.string());
                }else agent.SavePolicy(path.string());
            }
            std::lock_guard<std::mutex> lock(mutex_);
            status_=cancel_?"Training cancelled":"Policy saved: "+path.string();
        } catch(const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_); status_=e.what();
        }
        running_=false;
    });
}
void ReinforcementPanel::SelectEvaluationPolicy(const std::filesystem::path& path) {
    evaluationPolicy_=path;
    evaluationTask_=-1;
    evaluating_=false; completed_=0; reportStatus_.clear();
    std::lock_guard<std::mutex> lock(mutex_);
    report_={}; obstacleReport_={}; status_.clear();
    try { evaluationTask_=RLPolicyEvaluationTask(Neural::NeuralNetwork::Load(path.string())); }
    catch(const std::exception& e) { status_=e.what(); }
}
void ReinforcementPanel::RefreshPolicies() {
    policyFiles_=ListRLPolicyFiles(NeuralAssetDirectory().parent_path().parent_path());
    if(evaluationPolicy_.empty()) {
        const auto preferred=NeuralAssetPath(task_==1?"RLObstaclePolicy":"RLTrackingPolicy");
        if(std::find(policyFiles_.begin(),policyFiles_.end(),preferred)!=policyFiles_.end()) evaluationPolicy_=preferred;
        else if(!policyFiles_.empty()) evaluationPolicy_=policyFiles_.front();
    }
    if(!evaluationPolicy_.empty()) SelectEvaluationPolicy(evaluationPolicy_);
    policiesLoaded_=true;
}
void ReinforcementPanel::Evaluate() {
    if(running_) return;
    Stop();
    std::filesystem::path path, project;
    Neural::NeuralNetwork model;
    try {
        if(evaluationPolicy_.empty()) throw std::runtime_error("Select an evaluation model first.");
        path=evaluationPolicy_;
        // Re-read once at launch and give the worker this immutable snapshot.
        model=Neural::NeuralNetwork::Load(path.string());
        evaluationTask_=RLPolicyEvaluationTask(model);
        project=NeuralAssetDirectory().parent_path().parent_path();
        reportDirectory_=project/"Reports"/"RL";
    } catch(const std::exception& e) { status_=e.what(); return; }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        report_={}; obstacleReport_={}; status_="Evaluating saved policy...";
    }
    reportStatus_.clear(); evaluating_=true; completed_=0; cancel_=false; running_=true;
    const bool obstacles=evaluationTask_==1;
    worker_=std::thread([this,path,project,obstacles,model=std::move(model)] {
        try {
            if(obstacles){
                if(model.Layers().empty()||model.Layers().front().Inputs()!=8||model.Layers().back().Outputs()!=4)
                    throw std::runtime_error("Obstacle policy requires 8 inputs and 4 Q outputs");
                RL::ObstacleNavBaseline baseline(project);
                RL::ObstacleReport settings;settings.policy=path.string();
                auto result=RL::EvaluateObstacles(model,settings,[&](RL::GridCell a,RL::GridCell b){return baseline.Run(a,b);},
                    [this](size_t n){completed_=static_cast<int>(n);return !cancel_.load();});
                std::lock_guard<std::mutex> lock(mutex_);
                if(!result.complete||cancel_)status_="Obstacle evaluation cancelled; no report published.";
                else{obstacleReport_=std::move(result);status_="Obstacle comparison complete: learned / untrained / random / native NavMesh.";}
            }else{
            RL::TrackingReport settings;
            settings.policy=path.string();
            auto result=RL::EvaluateTracking(model,settings,[this](size_t count) {
                completed_=static_cast<int>(count); return !cancel_.load();
            });
            std::lock_guard<std::mutex> lock(mutex_);
            if(!result.complete || cancel_) status_="Evaluation cancelled; no report published.";
            else { report_=std::move(result); status_="Evaluation complete (saved policy; scene unchanged)."; }
            }
        } catch(const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_); status_=e.what();
        }
        running_=false;
    });
}
void ReinforcementPanel::SaveReport() {
    std::string csv;bool obstacle=false;
    { std::lock_guard<std::mutex> lock(mutex_);
      if(obstacleReport_.complete){csv=obstacleReport_.CSV();obstacle=true;}
      else if(report_.complete)csv=report_.CSV(); }
    if(csv.empty())return;
    try {
        std::filesystem::create_directories(reportDirectory_);
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto path=reportDirectory_/(std::string(obstacle?"obstacles_":"tracking_")+std::to_string(stamp)+".csv");
        std::ofstream out(path,std::ios::binary);
        if(!out) throw std::runtime_error("Cannot create evaluation report");
        out<<csv; out.close();
        if(!out) throw std::runtime_error("Cannot finish writing evaluation report");
        reportStatus_="Report saved: "+path.string();
    } catch(const std::exception& e) { reportStatus_=e.what(); }
}
void ReinforcementPanel::Draw() {
    if(!open_) return;
    if(!ImGui::Begin("Reinforcement Learning",&open_)) { ImGui::End(); return; }
    ImGui::TextWrapped("Native Double DQN - discrete actions, replay memory, target network and episodic rewards.");
    ImGui::TextWrapped(task_==1?"Learn raw cardinal actions from rewards and live obstacle sensors. Fixed 9x9 arena; no planner guides the policy.":"Train one axis; evaluate the shared policy in X/Z, including relocated targets.");
    if(ImGui::Button("Open Training Window")){
        std::string error;
        if(RL::LaunchTrainingWindow(NeuralAssetDirectory().parent_path().parent_path(),error,task_))reportStatus_="Training window opened. Each window is an independent process with its own outputs.";
        else reportStatus_=error;
    }
    ImGui::SameLine();ImGui::TextDisabled("Live simulation, pause/step, independent parallel runs");
    const bool busy=running_.load();
    ImGui::BeginDisabled(busy);
    const char* tasks[]={"Target tracking (X/Z)","Learned obstacle avoidance (fixed arena)"};
    if(ImGui::Combo("Training task",&task_,tasks,2)){
        std::snprintf(modelName_,sizeof(modelName_),"%s",task_==1?"RLObstaclePolicy":"RLTrackingPolicy");
        episodes_=task_==1?6000:1600;maxSteps_=task_==1?64:40;hidden_=task_==1?64:32;
        batch_=32;learningRate_=task_==1?.0005f:.001f;gamma_=.97f;
        {std::lock_guard<std::mutex> lock(mutex_);report_={};obstacleReport_={};rewards_.clear();losses_.clear();status_.clear();}
        reportStatus_.clear();

    }
    ImGui::InputText("Training output name",modelName_,sizeof(modelName_));
    ImGui::InputInt(task_==1?"Base episodes (+3000 refinement)":"Episodes",&episodes_);
    ImGui::InputInt("Max steps",&maxSteps_);
    ImGui::InputInt("Replay batch",&batch_);
    ImGui::InputInt("Hidden units",&hidden_);
    ImGui::InputFloat("Learning rate",&learningRate_,0,0,"%.5f");
    ImGui::SliderFloat("Discount gamma",&gamma_,0.f,1.f);
    if(ImGui::Button("Train in editor")) Start();
    ImGui::SeparatorText("Evaluate a saved model");
    if(!policiesLoaded_) RefreshPolicies();
    const auto project=NeuralAssetDirectory().parent_path().parent_path();
    const auto policyLabel=[&](const std::filesystem::path& path) {
        return path.lexically_relative(project).generic_string();
    };
    const std::string preview=evaluationPolicy_.empty()?"Select a .njn file":policyLabel(evaluationPolicy_);
    if(ImGui::BeginCombo("Evaluation model",preview.c_str())) {
        for(const auto& path:policyFiles_) {
            const bool selected=path==evaluationPolicy_;
            if(ImGui::Selectable(policyLabel(path).c_str(),selected)) SelectEvaluationPolicy(path);
            if(selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if(ImGui::Button("Refresh models")) RefreshPolicies();
    ImGui::SameLine();
    if(ImGui::Button("Browse...")) {
        const auto selected=SceneFileDialog::OpenNeuralPolicy(evaluationPolicy_.empty()?project/"TrainingRuns":evaluationPolicy_.parent_path());
        if(!selected.empty()) {
            SelectEvaluationPolicy(selected);
            if(std::find(policyFiles_.begin(),policyFiles_.end(),selected)==policyFiles_.end()) policyFiles_.push_back(selected);
        }
    }
    if(!evaluationPolicy_.empty()) ImGui::TextWrapped("Selected file: %s",evaluationPolicy_.string().c_str());
    if(evaluationTask_>=0) ImGui::TextWrapped("Detected evaluation: %s",evaluationTask_==1?"Learned obstacle avoidance (8 inputs / 4 actions)":"Target tracking X/Z (2 inputs / 3 actions)");
    ImGui::TextWrapped("Evaluation is detected automatically from the selected model. Training task only controls new training runs.");
    ImGui::BeginDisabled(running_.load() || evaluationPolicy_.empty() || evaluationTask_<0);
    if(ImGui::Button("Evaluate learning")) Evaluate();
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if(running_) { if(ImGui::Button("Cancel current job")) cancel_=true; }
    if(evaluating_) ImGui::Text("Evaluation scenarios: %d / 500",completed_.load());
    else ImGui::Text("Training episode: %d / %d",completed_.load(),episodes_+(task_==1?3000:0));
    if((evaluationTask_>=0?evaluationTask_:task_)==1)ImGui::TextWrapped("Evaluation: 500 separately seeded start/goal episodes on the fixed arena, 64 actions each. Includes the real baked NavMesh with the same 1.6-unit movement budget per decision. Blocked attempts are counted; no learned-action mask or fallback is used.");
    else ImGui::TextWrapped("Evaluation reads the policy saved on disk. All comparators use 500 held-out X/Z scenarios, two targets per scenario and 40 steps per target. No scene entities are moved.");
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
    RL::TrackingReport result;
    { std::lock_guard<std::mutex> lock(mutex_); result=report_; }
    if(result.complete) {
        ImGui::SeparatorText("Learning evaluation - X/Z");
        ImGui::TextWrapped("Evaluated asset: %s",result.policy.c_str());
        if(ImGui::BeginTable("##RLComparison",4,ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Policy"); ImGui::TableSetupColumn("Success");
            ImGui::TableSetupColumn("Arrival steps"); ImGui::TableSetupColumn("Final distance");
            ImGui::TableHeadersRow();
            const char* labels[]={"Saved policy","Untrained network","Random actions"};
            for(size_t i=0;i<result.metrics.size();++i) {
                const auto& m=result.metrics[i];
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(labels[i]);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f%% (%zu/%zu)",100.*m.SuccessRate(),m.successes,m.phases);
                ImGui::TableSetColumnIndex(2);
                if(m.successes) ImGui::Text("%.2f",m.MeanArrivalSteps()); else ImGui::TextUnformatted("N/A");
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f",m.MeanDistance());
            }
            ImGui::EndTable();
        }
        const double saved=result.metrics[0].SuccessRate();
        const bool passes=saved>=.9 && saved>=result.metrics[1].SuccessRate()+.3 && saved>=result.metrics[2].SuccessRate()+.3;
        ImGui::TextWrapped("%s: >=90%% success and >=30 percentage points above both baselines.",passes?"Threshold met":"Threshold not met");
        ImGui::TextWrapped("Arrival steps average successful phases only; failures are reflected in success rate. Distance is normalized Euclidean X/Z, averaged over all phases. Arrival hold (<0.08 per axis) is shared by all comparators. This evaluates a saved policy, not online learning in Play.");
        ImGui::BeginDisabled(running_.load());
        if(ImGui::Button("Save CSV report")) SaveReport();
        ImGui::EndDisabled();
    }
    RL::ObstacleReport obstacle;
    {std::lock_guard<std::mutex> lock(mutex_);obstacle=obstacleReport_;}
    if(obstacle.complete){
        ImGui::SeparatorText("Learned obstacle avoidance - fixed arena");
        ImGui::TextWrapped("Evaluated asset: %s",obstacle.policy.c_str());
        if(ImGui::BeginTable("##ObstacleComparison",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)){
            for(const char*label:{"Policy","Success","Blocked attempts","Arrival steps","Final distance"})ImGui::TableSetupColumn(label);
            ImGui::TableHeadersRow();const char*names[]={"Learned DQN","Untrained network","Random actions","Native NavMesh"};
            for(size_t i=0;i<obstacle.metrics.size();++i){const auto&m=obstacle.metrics[i];
                ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(names[i]);
                ImGui::TableSetColumnIndex(1);ImGui::Text("%.1f%%",m.Rate()*100.);
                ImGui::TableSetColumnIndex(2);ImGui::Text("%d",m.collisions);
                ImGui::TableSetColumnIndex(3);if(m.successes)ImGui::Text("%.2f",double(m.arrivalSteps)/m.successes);else ImGui::TextUnformatted("N/A");
                ImGui::TableSetColumnIndex(4);ImGui::Text("%.3f",m.episodes?m.distance/m.episodes:0.);
            }ImGui::EndTable();
        }
        const bool pass=obstacle.metrics[0].Rate()>=.9&&obstacle.metrics[0].Rate()>=obstacle.metrics[1].Rate()+.3&&obstacle.metrics[0].Rate()>=obstacle.metrics[2].Rate()+.3;
        ImGui::TextWrapped("%s: >=90%% success and >=30 points above both untrained/random baselines. NavMesh is a separate classical reference, not a training teacher.",pass?"Learning threshold met":"Learning threshold not met");
        ImGui::TextWrapped("Core-only benchmark (original 9x9). Open-floor policy composition is tested separately. Known fixed walls; evaluation seeds are separate but finite start/goal pairs can recur. Arrival steps exclude failures. Blocked attempts are rejected moves, not physical wall penetration. Distances are world units. CSV also includes travelled path length.");
        ImGui::BeginDisabled(running_.load());if(ImGui::Button("Save CSV report##obstacles"))SaveReport();ImGui::EndDisabled();
    }
    if(!reportStatus_.empty()) ImGui::TextWrapped("%s",reportStatus_.c_str());
    ImGui::End();
}
}

