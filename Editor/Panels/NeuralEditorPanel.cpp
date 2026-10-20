#include "NeuralEditorPanel.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include <fstream>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace NoJob {
namespace {
constexpr const char* kActivationNames[] = {"Linear", "ReLU", "Sigmoid", "Tanh"};
const std::vector<std::vector<float>> kX = {{0,0},{0,1},{1,0},{1,1}};
const std::vector<std::vector<float>> kY = {{0},{1},{1},{0}};
void CopyError(char (&out)[512], const std::string& text) { std::snprintf(out, sizeof(out), "%s", text.c_str()); }
}
NeuralEditorPanel::NeuralEditorPanel() { Build(); }
NeuralEditorPanel::~NeuralEditorPanel() { StopTraining(); }
void NeuralEditorPanel::StopTraining() { cancel_ = true; if(worker_.joinable()) worker_.join(); running_ = false; }
void NeuralEditorPanel::FinishWorker() {
    if(running_) return;
    if(worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(workerMutex_);
    if(resultReady_) { network_ = std::move(trained_); resultReady_ = false; dirty_=true; }
    if(!workerError_.empty()) { CopyError(error_, workerError_); workerError_.clear(); }
}
void NeuralEditorPanel::Build() {
    if(running_) return;
    try {
        if(widths_.size()<2 || widths_.size()!=activations_.size()+1) throw std::invalid_argument("Invalid architecture");
        Neural::NeuralNetwork net(42);
        for(size_t i=1;i<widths_.size();++i) {
            if(widths_[i-1]<1 || widths_[i-1]>256 || widths_[i]<1 || widths_[i]>256) throw std::invalid_argument("Neuron count must be 1..256");
            net.AddDense(static_cast<size_t>(widths_[i-1]), static_cast<size_t>(widths_[i]), static_cast<Neural::Activation>(activations_[i-1]));
        }
        network_ = std::move(net); inputs_.assign(widths_.front(), 0.f);
        ResetTrainingSamples();
        lastPrediction_.clear(); lastActivations_.clear(); losses_.clear(); error_[0]=0; dirty_=true;
    } catch(const std::exception& e) { CopyError(error_,e.what()); }
}
void NeuralEditorPanel::StartTraining() {
    if(running_) return;
    FinishWorker();
    if(network_.Layers().empty()) { CopyError(error_, "Build a network first"); return; }
    const size_t in = network_.Layers().front().Inputs(), out = network_.Layers().back().Outputs();
    if (trainingX_.empty() || trainingX_.size()!=trainingY_.size()) { CopyError(error_, "Add training samples first"); return; }
    for(size_t i=0;i<trainingX_.size();++i) if(trainingX_[i].size()!=in || trainingY_[i].size()!=out) {
        CopyError(error_, "Dataset dimensions do not match the active network. Reset dataset after Rebuild."); return;
    }
    if(epochs_<1 || batchSize_<1 || learningRate_<=0.f) { CopyError(error_,"Invalid training parameters"); return; }
    error_[0]=0; cancel_=false; currentEpoch_=0; losses_.clear();
    Neural::NeuralNetwork copy = network_;
    const Neural::TrainingOptions opt{static_cast<size_t>(epochs_),static_cast<size_t>(batchSize_),learningRate_,adam_,42};
    running_=true;
    const auto samplesX = trainingX_, samplesY = trainingY_;
    worker_=std::thread([this, net=std::move(copy),opt,samplesX,samplesY]() mutable {
        try {
            net.Train(samplesX,samplesY,opt,[this](size_t epoch,float loss) {
                { std::lock_guard<std::mutex> lock(workerMutex_); losses_.push_back(loss); }
                currentEpoch_=static_cast<int>(epoch);
                return !cancel_.load();
            });
            if(!cancel_) { std::lock_guard<std::mutex> lock(workerMutex_); trained_=std::move(net); resultReady_=true; }
        } catch(const std::exception& e) { std::lock_guard<std::mutex> lock(workerMutex_); workerError_=e.what(); }
        running_=false;
    });
}
void NeuralEditorPanel::DrawGraph() {
    ImGui::TextUnformatted("Network topology (click a layer to inspect)");
    ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 285.f);
    if(canvasSize.x<200.f) canvasSize.x=200.f;
    ImGui::InvisibleButton("##neural_canvas",canvasSize);
    const ImVec2 origin=ImGui::GetItemRectMin();
    auto* draw=ImGui::GetWindowDrawList();
    if(network_.Layers().empty()) { ImGui::TextDisabled("No network available"); return; }
    // Always render the active network, not uncommitted architecture edits.
    std::vector<int> graphWidths{static_cast<int>(network_.Layers().front().Inputs())};
    for(const auto& layer : network_.Layers()) graphWidths.push_back(static_cast<int>(layer.Outputs()));
    if(graphWidths != widths_) ImGui::TextDisabled("Unsaved architecture edits: click Rebuild network to apply");
    const float dx=canvasSize.x/static_cast<float>(graphWidths.size()+1);
    const float centerY=origin.y+canvasSize.y*.5f;
    auto pos=[&](size_t layer,int neuron) { const int count=std::min(graphWidths[layer],12);return ImVec2(origin.x+dx*(layer+1),centerY+(neuron-(count-1)*.5f)*std::min(26.f,245.f/std::max(1,count))); };
    for(size_t l=1;l<graphWidths.size();++l) {
        const auto& w=network_.Layers()[l-1].Weights();
        const int prev=std::min(graphWidths[l-1],12),curr=std::min(graphWidths[l],12);
        for(int j=0;j<curr;++j)for(int i=0;i<prev;++i) {
            float weight=w(static_cast<size_t>(j),static_cast<size_t>(i));
            ImU32 col=weight>=0?IM_COL32(80,190,135,125):IM_COL32(235,105,100,125);
            draw->AddLine(pos(l-1,i),pos(l,j),col,std::clamp(std::abs(weight),.6f,3.5f));
        }
    }
    for(size_t l=0;l<graphWidths.size();++l) {
        int count=std::min(graphWidths[l],12);
        for(int j=0;j<count;++j) {
            ImVec2 p=pos(l,j);
            float a=0.f;
            if(l<lastActivations_.size() && j<static_cast<int>(lastActivations_[l].size())) a=lastActivations_[l][j];
            const int intensity=static_cast<int>(std::clamp((a+1.f)*.5f,0.f,1.f)*140.f);
            draw->AddCircleFilled(p,8.f,IM_COL32(60+intensity,110+intensity/2,205,255));
            draw->AddCircle(p,8.f,l>0 && selectedLayer_==static_cast<int>(l-1)?IM_COL32(255,215,90,255):IM_COL32(215,220,235,255),0,1.5f);
        }
        char title[48];std::snprintf(title,sizeof(title),"%s (%d)",l==0?"Input":(l+1==graphWidths.size()?"Output":"Hidden"),graphWidths[l]);
        draw->AddText(ImVec2(origin.x+dx*(l+1)-35.f,origin.y+8.f),IM_COL32(215,220,235,255),title);
        if(l>0 && ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
            ImVec2 mouse=ImGui::GetMousePos();if(std::abs(mouse.x-(origin.x+dx*(l+1)))<dx*.35f) selectedLayer_=static_cast<int>(l-1);
        }
    }
    ImGui::TextDisabled("Green: positive weight | Red: negative weight | Thickness: magnitude | Max 12 visible neurons per layer");
}
void NeuralEditorPanel::DrawInspector() {
    if(ImGui::CollapsingHeader("Architecture / Layer Editor",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(running_);
        ImGui::InputInt("Input neurons",&widths_[0]); widths_[0]=std::clamp(widths_[0],1,256);
        for(size_t l=1;l<widths_.size();++l) {
            ImGui::PushID(static_cast<int>(l));ImGui::Separator();
            ImGui::Text("Layer %d",static_cast<int>(l));ImGui::SameLine();
            if(widths_.size()>2 && ImGui::SmallButton("Remove")) { widths_.erase(widths_.begin()+static_cast<std::ptrdiff_t>(l)); activations_.erase(activations_.begin()+static_cast<std::ptrdiff_t>(l-1)); ImGui::PopID(); break; }
            ImGui::InputInt("Neurons",&widths_[l]);widths_[l]=std::clamp(widths_[l],1,256);
            ImGui::Combo("Activation",&activations_[l-1],kActivationNames,4);
            ImGui::PopID();
        }
        if(widths_.size()<12 && ImGui::Button("Add hidden layer")) { widths_.insert(widths_.end()-1,4);activations_.insert(activations_.end()-1,3); }
        ImGui::SameLine();if(ImGui::Button("Rebuild network")) Build();
        ImGui::EndDisabled();
        ImGui::TextDisabled("Architecture edits take effect after Rebuild (resets weights).");
    }
    if(ImGui::CollapsingHeader("Selected Layer / Weights",ImGuiTreeNodeFlags_DefaultOpen) && !network_.Layers().empty()) {
        selectedLayer_=std::clamp(selectedLayer_,0,static_cast<int>(network_.Layers().size()-1));
        ImGui::SliderInt("Inspect layer",&selectedLayer_,0,static_cast<int>(network_.Layers().size()-1));
        auto& layer=network_.Layers()[selectedLayer_];
        ImGui::Text("Dense: %zu -> %zu | %s",layer.Inputs(),layer.Outputs(),kActivationNames[static_cast<int>(layer.Function())]);
        ImGui::BeginChild("##weight_table",ImVec2(0,170),true);
        const int rows=static_cast<int>(std::min<size_t>(layer.Outputs(),16)),cols=static_cast<int>(std::min<size_t>(layer.Inputs(),12));
        if(ImGui::BeginTable("Weights",cols+2,ImGuiTableFlags_Borders|ImGuiTableFlags_ScrollX)) {
            ImGui::TableSetupColumn("Neuron");for(int i=0;i<cols;++i) {char label[24];std::snprintf(label,sizeof(label),"w%d",i);ImGui::TableSetupColumn(label);}
            ImGui::TableSetupColumn("Bias");ImGui::TableHeadersRow();
            for(int r=0;r<rows;++r) {ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%d",r);
                for(int c=0;c<cols;++c){ImGui::TableSetColumnIndex(c+1);ImGui::Text("%.3f",layer.Weights()(r,c));}
                ImGui::TableSetColumnIndex(cols+1);ImGui::Text("%.3f",layer.Biases()[r]);}
            ImGui::EndTable();
        }
        ImGui::EndChild();
        if(layer.Inputs()>12 || layer.Outputs()>16) ImGui::TextDisabled("Weight preview truncated for large layers.");
    }
}
void NeuralEditorPanel::ResetTrainingSamples() {
    trainingX_.clear(); trainingY_.clear();
    if(network_.Layers().empty()) return;
    const size_t in = network_.Layers().front().Inputs(), out = network_.Layers().back().Outputs();
    if(in==2 && out==1) { trainingX_=kX; trainingY_=kY; }
    else { trainingX_.push_back(std::vector<float>(in,0.f)); trainingY_.push_back(std::vector<float>(out,0.f)); }
}
void NeuralEditorPanel::DrawTraining() {
    if(!ImGui::CollapsingHeader("Native Training / Loss",ImGuiTreeNodeFlags_DefaultOpen))return;
    ImGui::BeginDisabled(running_);
    ImGui::InputInt("Epochs",&epochs_);epochs_=std::clamp(epochs_,1,1000000);
    ImGui::InputInt("Batch size",&batchSize_);batchSize_=std::clamp(batchSize_,1,4096);
    ImGui::InputFloat("Learning rate",&learningRate_,0.001f,0.01f,"%.5f");
    ImGui::Checkbox("Adam optimizer",&adam_);
    const size_t in = network_.Layers().empty()?0:network_.Layers().front().Inputs();
    const size_t out = network_.Layers().empty()?0:network_.Layers().back().Outputs();
    ImGui::Text("Training dataset: %zu samples | %zu inputs -> %zu outputs",trainingX_.size(),in,out);
    if(ImGui::Button("Add sample") && in && out && trainingX_.size()<4096) {
        trainingX_.push_back(std::vector<float>(in,0.f)); trainingY_.push_back(std::vector<float>(out,0.f));
    }
    ImGui::SameLine();
    if(ImGui::Button("Reset dataset")) ResetTrainingSamples();
    if(in==2 && out==1) { ImGui::SameLine(); if(ImGui::Button("Load XOR samples")) {trainingX_=kX;trainingY_=kY;} }
    ImGui::BeginChild("##training_samples",ImVec2(0,185),true);
    for(size_t row=0;row<trainingX_.size();++row) {
        ImGui::PushID(static_cast<int>(row));
        ImGui::Separator(); ImGui::Text("Sample %zu",row+1); ImGui::SameLine();
        if(ImGui::SmallButton("Delete") && trainingX_.size()>1) {
            trainingX_.erase(trainingX_.begin()+static_cast<std::ptrdiff_t>(row));
            trainingY_.erase(trainingY_.begin()+static_cast<std::ptrdiff_t>(row));
            ImGui::PopID(); break;
        }
        for(size_t i=0;i<trainingX_[row].size();++i) {
            ImGui::PushID(static_cast<int>(i)); ImGui::SetNextItemWidth(90);
            ImGui::InputFloat("Input",&trainingX_[row][i],0,0,"%.3f");
            if(i+1<trainingX_[row].size()) ImGui::SameLine(); ImGui::PopID();
        }
        for(size_t i=0;i<trainingY_[row].size();++i) {
            ImGui::PushID(static_cast<int>(i)); ImGui::SetNextItemWidth(90);
            ImGui::InputFloat("Target",&trainingY_[row][i],0,0,"%.3f");
            if(i+1<trainingY_[row].size()) ImGui::SameLine(); ImGui::PopID();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if(ImGui::Button("Train dataset")) StartTraining();
    ImGui::EndDisabled();
    if(running_) {ImGui::SameLine();if(ImGui::Button("Stop")) cancel_=true;}
    ImGui::ProgressBar(epochs_?static_cast<float>(currentEpoch_.load())/epochs_:0.f,ImVec2(-1,0));
    std::vector<float> snapshot; {std::lock_guard<std::mutex> lock(workerMutex_);snapshot=losses_;}
    if(!snapshot.empty()) {
        const float maxLoss=std::max(.01f,*std::max_element(snapshot.begin(),snapshot.end()));
        ImGui::PlotLines("MSE",snapshot.data(),static_cast<int>(snapshot.size()),0,nullptr,0.f,maxLoss,ImVec2(-1,130));
        ImGui::Text("Epoch %d / %d | MSE %.8f",currentEpoch_.load(),epochs_,snapshot.back());
    }
    ImGui::TextDisabled("XOR is an optional 2-input/1-output demo; other architectures use editable training samples.");
}
void NeuralEditorPanel::DrawPredictions() {
    if(!ImGui::CollapsingHeader("Neural Inspector / Inference",ImGuiTreeNodeFlags_DefaultOpen))return;
    if(inputs_.size()!=network_.Layers().front().Inputs()) inputs_.assign(network_.Layers().front().Inputs(),0.f);
    for(size_t i=0;i<inputs_.size() && i<16;++i){ImGui::PushID(static_cast<int>(i));char label[32];std::snprintf(label,sizeof(label),"Input %zu",i);ImGui::InputFloat(label,&inputs_[i]);ImGui::PopID();}
    if(inputs_.size()>16) ImGui::TextDisabled("Showing first 16 inputs; others are zero.");
    if(ImGui::Button("Run inference")) {
        try {lastActivations_.clear();lastActivations_.push_back(inputs_);auto value=inputs_;
            for(const auto& layer:network_.Layers()){value=layer.Forward(value);lastActivations_.push_back(value);}
            lastPrediction_=value;error_[0]=0;
        } catch(const std::exception& e){CopyError(error_,e.what());}
    }
    for(size_t i=0;i<lastPrediction_.size();++i)ImGui::Text("Output %zu: %.6f",i,lastPrediction_[i]);
    for(size_t l=0;l<lastActivations_.size();++l) {
        if(ImGui::TreeNode(reinterpret_cast<void*>(static_cast<uintptr_t>(l+1)),"Layer %zu activations",l)) {
            for(size_t j=0;j<lastActivations_[l].size() && j<32;++j)ImGui::Text("Neuron %zu: %.5f",j,lastActivations_[l][j]);
            ImGui::TreePop();
        }
    }
    if(network_.Layers().front().Inputs()==2 && network_.Layers().back().Outputs()==1) {
        if(ImGui::BeginTable("XOR results",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Input");ImGui::TableSetupColumn("Expected");ImGui::TableSetupColumn("Predicted");ImGui::TableHeadersRow();
            for(size_t i=0;i<kX.size();++i){const auto pred=network_.Predict(kX[i]);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Text("%.0f, %.0f",kX[i][0],kX[i][1]);ImGui::TableNextColumn();ImGui::Text("%.0f",kY[i][0]);ImGui::TableNextColumn();ImGui::Text("%.5f",pred[0]);}
            ImGui::EndTable();
        }
    }
}
void NeuralEditorPanel::RememberAsset(const std::string& name) const {
    std::error_code ec;
    std::filesystem::create_directories(NeuralAssetDirectory(), ec);
    if (ec) return;
    std::ofstream out(NeuralAssetDirectory() / ".last-opened-neural", std::ios::trunc);
    if (out) out << name << '\n';
}
void NeuralEditorPanel::LoadNamed(const std::string& name) {
    if (running_) throw std::runtime_error("Stop training before loading another network");
    auto loaded = Neural::NeuralNetwork::Load(NeuralAssetPath(name).string());
    if (loaded.Layers().empty()) throw std::runtime_error("Neural asset has no layers");
    std::vector<int> widths;
    std::vector<int> activations;
    widths.push_back(static_cast<int>(loaded.Layers().front().Inputs()));
    for (const auto& layer : loaded.Layers()) {
        widths.push_back(static_cast<int>(layer.Outputs()));
        activations.push_back(static_cast<int>(layer.Function()));
    }
    FinishWorker();
    network_ = std::move(loaded);
    widths_ = std::move(widths);
    activations_ = std::move(activations);
    inputs_.assign(widths_.front(), 0.f);
    ResetTrainingSamples();
    selectedLayer_ = 0;
    losses_.clear(); lastActivations_.clear(); lastPrediction_.clear();
    currentName_ = name;
    std::snprintf(assetName_, sizeof(assetName_), "%s", name.c_str());
    dirty_ = false;
    error_[0] = 0;
    RememberAsset(name);
    assetMessage_ = "Loaded " + NeuralAssetPath(name).string();
}
void NeuralEditorPanel::SaveNamed(const std::string& name, bool saveAs) {
    if (running_) throw std::runtime_error("Stop training before saving");
    if (network_.Layers().size()+1 != widths_.size()) throw std::runtime_error("Rebuild network before saving architecture changes");
    for(size_t i=1;i<widths_.size();++i)
        if(network_.Layers()[i-1].Inputs()!=static_cast<size_t>(widths_[i-1]) ||
           network_.Layers()[i-1].Outputs()!=static_cast<size_t>(widths_[i]) ||
           static_cast<int>(network_.Layers()[i-1].Function())!=activations_[i-1])
            throw std::runtime_error("Rebuild network before saving architecture changes");
    const auto path = NeuralAssetPath(name); // validates the name
    if (saveAs && name != currentName_ && std::filesystem::exists(path))
        throw std::runtime_error("A neural network with that name already exists");
    std::filesystem::create_directories(path.parent_path());
    network_.Save(path.string());
    currentName_ = name;
    std::snprintf(assetName_, sizeof(assetName_), "%s", name.c_str());
    dirty_ = false;
    error_[0] = 0;
    RememberAsset(name);
    assetMessage_ = "Saved " + path.string();
}
void NeuralEditorPanel::RestoreLastAsset() {
    if (restoreAttempted_) return;
    restoreAttempted_ = true;
    const auto available = ListNeuralAssetNames();
    std::string preferred;
    { std::ifstream in(NeuralAssetDirectory() / ".last-opened-neural"); std::getline(in, preferred); }
    if (std::find(available.begin(), available.end(), preferred) == available.end())
        preferred = available.empty() ? std::string{} : available.front();
    if (!preferred.empty()) {
        try { LoadNamed(preferred); }
        catch (const std::exception& e) { assetMessage_ = e.what(); }
    }
}
void NeuralEditorPanel::Draw() {
    FinishWorker();
    if(!open_)return;
    RestoreLastAsset();
    if(!ImGui::Begin("Neural Network Editor & Inspector",&open_)) {ImGui::End();return;}
    ImGui::TextUnformatted("NoJobEngine V2.0.4 / V2.0.5 | Native C++ neural networks");
    ImGui::Separator();
    ImGui::Text("Current asset: %s%s", currentName_.c_str(), dirty_ ? " *" : "");
    ImGui::BeginDisabled(running_);
    const auto available = ListNeuralAssetNames();
    if (ImGui::BeginCombo("Saved networks", currentName_.c_str())) {
        for (const auto& name : available) {
            if (ImGui::Selectable(name.c_str(), name == currentName_)) {
                try { LoadNamed(name); } catch(const std::exception& e) { CopyError(error_, e.what()); }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::InputText("Network name", assetName_, sizeof(assetName_));
    if (ImGui::Button("Load")) {
        try { LoadNamed(assetName_); } catch(const std::exception& e) { CopyError(error_, e.what()); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        try { SaveNamed(currentName_, false); } catch(const std::exception& e) { CopyError(error_, e.what()); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        try { SaveNamed(assetName_, true); } catch(const std::exception& e) { CopyError(error_, e.what()); }
    }
    ImGui::SameLine();
    if (ImGui::Button("New")) {
        widths_ = {2,4,1}; activations_ = {3,2};
        currentName_ = "Untitled";
        std::snprintf(assetName_, sizeof(assetName_), "%s", currentName_.c_str());
        Build(); dirty_ = true;
        assetMessage_ = "New network. Enter a name and click Save As.";
    }
    ImGui::EndDisabled();
    if (!assetMessage_.empty()) ImGui::TextWrapped("%s", assetMessage_.c_str());
    if(error_[0]) ImGui::TextColored(ImVec4(1.f,.45f,.4f,1.f),"%s",error_);
    DrawGraph();
    if(ImGui::BeginTabBar("NeuralTabs")) {
        if(ImGui::BeginTabItem("Architecture & Weights")){DrawInspector();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Training")){DrawTraining();ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Inference & Activations")){DrawPredictions();ImGui::EndTabItem();}
        ImGui::EndTabBar();
    }
    ImGui::End();
}
}
