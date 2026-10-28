/** @file main.cpp @brief Independent native RL training window, one process per run. */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Engine/AI/Reinforcement/VisibleTrainingRun.h"
#include "Engine/AI/Reinforcement/TrainingProcess.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "Engine/Core/ApplicationIcon.h"
#include "Engine/Core/ProjectPaths.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <cfloat>
#ifdef _WIN32
#include <shellapi.h>
#endif
using namespace NoJob;
using namespace NoJob::RL;
namespace {
/** @brief Fail a headless check in every build configuration. */
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
/** @brief Check concurrent output isolation and actual pause/step/cancel behavior without graphics. */
int SelfTest(const std::filesystem::path& output){
    std::filesystem::create_directories(output);std::ofstream log(output/"self-test.txt");
    try{
        VisibleTrainingConfig config;config.episodes=8;config.hidden=8;config.batch=4;config.refinement=false;config.name="same_name";
        VisibleTrainingRun first,second;first.Start(output,config);config.seed=43;config.obstacles=false;second.Start(output,config);first.Join();second.Join();
        const auto a=first.Snapshot(),b=second.Snapshot();
        Require(a.state=="Completed"&&b.state=="Completed","Concurrent runs did not finish");
        Require(a.directory!=b.directory&&a.model!=b.model,"Concurrent runs shared an output path");
        Require(a.transitions>0&&b.transitions>0,"No actual step telemetry");
        Require(Neural::NeuralNetwork::Load(a.model.string()).Layers().front().Inputs()==8,"Saved policy dimensions invalid");
        Require(Neural::NeuralNetwork::Load(b.model.string()).Layers().front().Inputs()==2,"Tracking run dimensions invalid");
        Require(std::filesystem::is_regular_file(a.directory/"episodes.csv"),"Missing episode history");
        log<<"PASS: concurrent runs, unique paths, real telemetry, saved policy/history\n";
        VisibleTrainingRun controlled;config.episodes=1000;controlled.SetDelay(5);controlled.Start(output,config);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));controlled.Pause(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));const auto paused=controlled.Snapshot().transitions;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));Require(controlled.Snapshot().transitions==paused,"Pause advanced the environment");
        controlled.Step();std::this_thread::sleep_for(std::chrono::milliseconds(80));Require(controlled.Snapshot().transitions==paused+1,"Single-step did not advance exactly once");
        controlled.Cancel();controlled.Join();const auto cancelled=controlled.Snapshot();
        Require(cancelled.state=="Cancelled"&&cancelled.model.empty(),"Cancel saved a final policy");
        Require(!std::filesystem::exists(cancelled.directory/"policy.njn"),"Cancelled run produced final model");
        log<<"PASS: pause, exactly one step, cancellation, partial history retained\n";return 0;
    }catch(const std::exception&e){log<<"FAIL: "<<e.what()<<'\n';return 1;}
}
/** @brief Draw the latest real transition in the core training arena. */
void DrawArena(const VisibleTrainingSnapshot& state){
    const float size=std::min(ImGui::GetContentRegionAvail().x,430.f),cell=size/9.f;
    if(!state.obstacles){
        const ImVec2 o=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("##AxisArena",ImVec2(size,220));auto*draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(o,ImVec2(o.x+size,o.y+210),IM_COL32(33,43,56,255),6.f);
        draw->AddLine(ImVec2(o.x+20,o.y+105),ImVec2(o.x+size-20,o.y+105),IM_COL32(95,112,133,255),3.f);
        auto x=[&](float p){return o.x+20+(p+1.f)*.5f*(size-40);};
        draw->AddCircleFilled(ImVec2(x(state.frame.axisGoal),o.y+105),13,IM_COL32(245,155,58,255));
        draw->AddRectFilled(ImVec2(x(state.frame.axisPosition)-10,o.y+95),ImVec2(x(state.frame.axisPosition)+10,o.y+115),IM_COL32(74,157,247,255),3.f);
        ImGui::TextWrapped("Actual one-axis environment: blue agent, orange sampled goal. Actions are left / hold / right.");return;
    }
    const ImVec2 origin=ImGui::GetCursorScreenPos();auto*draw=ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##TrainingArena",ImVec2(size,size));
    auto point=[&](GridCell p){return ImVec2(origin.x+(p.x+.5f)*cell,origin.y+(8-p.z+.5f)*cell);};
    for(int z=0;z<9;++z)for(int x=0;x<9;++x){
        const ImVec2 lo(origin.x+x*cell,origin.y+(8-z)*cell),hi(lo.x+cell,lo.y+cell);
        draw->AddRectFilled(lo,hi,ObstacleEnvironment::Blocked({x,z})?IM_COL32(139,63,61,255):IM_COL32(33,43,56,255));
        draw->AddRect(lo,hi,IM_COL32(63,75,90,255));
    }
    for(size_t i=1;i<state.trail.size();++i)draw->AddLine(point(state.trail[i-1]),point(state.trail[i]),IM_COL32(75,170,215,150),2.f);
    const auto goal=point(state.frame.goal),agent=point(state.frame.position);
    draw->AddCircleFilled(goal,cell*.29f,IM_COL32(245,155,58,255));
    draw->AddRectFilled(ImVec2(agent.x-cell*.23f,agent.y-cell*.23f),ImVec2(agent.x+cell*.23f,agent.y+cell*.23f),IM_COL32(74,157,247,255),3.f);
    ImGui::TextDisabled("Blue: actual agent   Orange: sampled goal   Red: solid cells");
    ImGui::TextWrapped("This view shows real training steps, including exploration and blocked attempts. It is not a playback of the final policy.");
}
/** @brief Present independent controls, live environment and training curves. */
int Window(const std::filesystem::path& project,bool obstacles){
    if(!glfwInit())throw std::runtime_error("GLFW initialization failed");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow*window=glfwCreateWindow(1120,780,"NJE RL Trainer - independent window",nullptr,nullptr);
    if(!window){glfwTerminate();throw std::runtime_error("Cannot create training window");}
    #ifdef _WIN32
    glfwSetWindowTitle(window,("NJE RL Trainer - process "+std::to_string(GetCurrentProcessId())).c_str());
#endif
    ApplyApplicationIcon(window);
    glfwMakeContextCurrent(window);glfwSwapInterval(1);
    if(!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)))throw std::runtime_error("OpenGL loader failed");
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::StyleColorsDark();ImGui::GetStyle().FrameRounding=4.f;
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330 core");
    VisibleTrainingRun run;VisibleTrainingConfig config;config.obstacles=obstacles;
    int task=obstacles?1:0;
    if(!obstacles){config.episodes=1600;config.hidden=32;config.rate=.001f;config.refinement=false;}
    char name[64]="obstacle",asset[128]="";int seed=42,delay=0;std::string feedback;
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
        const auto*viewport=ImGui::GetMainViewport();ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("RL Trainer",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("NJE RL Trainer");ImGui::SameLine();ImGui::TextDisabled("Independent process - visible RL training");
        if(ImGui::Button("Open another training window")){std::string error;if(!LaunchTrainingWindow(project,error,task))feedback=error;}
        ImGui::SameLine();ImGui::TextDisabled("Each run keeps its own model and history.");
        const bool busy=run.Running();ImGui::BeginDisabled(busy);
        const char*tasks[]={"Target tracking (X/Z axis policy)","Obstacle core (9x9)"};
        if(ImGui::Combo("Task",&task,tasks,2)){config.obstacles=task==1;config.episodes=task==1?6000:1600;config.hidden=task==1?64:32;config.rate=task==1?.0005f:.001f;config.refinement=task==1;}
        ImGui::SetNextItemWidth(190);ImGui::InputText("Run name",name,sizeof(name));ImGui::SameLine();ImGui::SetNextItemWidth(100);ImGui::InputInt("Seed",&seed);
        ImGui::SetNextItemWidth(130);ImGui::InputInt("Base episodes",&config.episodes);ImGui::SameLine();ImGui::SetNextItemWidth(100);ImGui::InputInt("Hidden",&config.hidden);ImGui::SameLine();ImGui::SetNextItemWidth(100);ImGui::InputInt("Batch",&config.batch);
        ImGui::SetNextItemWidth(140);ImGui::InputFloat("Learning rate",&config.rate,0,0,"%.6f");ImGui::SameLine();ImGui::SetNextItemWidth(130);ImGui::SliderFloat("Gamma",&config.gamma,0,1);ImGui::SameLine();ImGui::BeginDisabled(!config.obstacles);ImGui::Checkbox("Refine stalled cases (+3000 episodes)",&config.refinement);ImGui::EndDisabled();
        if(ImGui::Button("Start training")){try{config.name=name;config.seed=static_cast<uint32_t>(seed);run.Start(project/"TrainingRuns",config);
#ifdef _WIN32
                glfwSetWindowTitle(window,("NJE RL Trainer - "+config.name+" - process "+std::to_string(GetCurrentProcessId())).c_str());
#endif
                feedback.clear();asset[0]=0;}catch(const std::exception&e){feedback=e.what();}}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(!busy);
        if(ImGui::Button(run.Paused()?"Resume":"Pause"))run.Pause(!run.Paused());ImGui::SameLine();
        ImGui::BeginDisabled(!run.Paused());if(ImGui::Button("Single step"))run.Step();ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel run"))run.Cancel();ImGui::EndDisabled();
        ImGui::SetNextItemWidth(260);if(ImGui::SliderInt("Visual delay (ms per step)",&delay,0,250))run.SetDelay(delay);ImGui::SameLine();ImGui::TextDisabled("0 = unrestricted; increase to watch decisions.");
        auto state=run.Snapshot();if(state.state=="Idle")state.obstacles=config.obstacles;
        const int globalEpisode=state.frame.episode+(state.frame.phase?state.baseEpisodes:0);
        const float progress=state.state=="Completed"?1.f:state.totalEpisodes?float(globalEpisode)/state.totalEpisodes:0.f;
        ImGui::ProgressBar(std::clamp(progress,0.f,1.f),ImVec2(-1,0),(std::to_string(globalEpisode)+" / "+std::to_string(state.totalEpisodes)+" episodes").c_str());
        ImGui::Text("%s%s | %s",state.state.c_str(),run.Paused()?" / Paused":"",state.status.c_str());
        ImGui::Separator();
        if(ImGui::BeginTable("##TrainerLayout",2,ImGuiTableFlags_Resizable)){
            ImGui::TableSetupColumn("Environment",ImGuiTableColumnFlags_WidthStretch,.48f);ImGui::TableSetupColumn("Learning",ImGuiTableColumnFlags_WidthStretch,.52f);
            ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);DrawArena(state);
            ImGui::TableSetColumnIndex(1);
            const auto&f=state.frame;ImGui::Text("%s episode %d | step %d / %d",f.phase?"Refinement":"Base",f.episode,f.step,state.obstacles?64:40);
            ImGui::Text("Epsilon %.4f   Loss %.6f",f.epsilon,f.loss);ImGui::Text("Reward %.3f   Episode reward %.3f",f.reward,f.totalReward);
            ImGui::Text("Action %d | Blocked attempts %d",f.action,f.collisions);ImGui::TextDisabled(state.obstacles?"Actions: -X, +X, -Z, +Z":"Actions: left, hold, right");ImGui::Text("Actual transitions: %llu",static_cast<unsigned long long>(state.transitions));
            if(!state.rewards.empty())ImGui::PlotLines("Episode reward",state.rewards.data(),static_cast<int>(state.rewards.size()),0,nullptr,FLT_MAX,FLT_MAX,ImVec2(0,105));
            if(!state.losses.empty())ImGui::PlotLines("Training loss",state.losses.data(),static_cast<int>(state.losses.size()),0,nullptr,FLT_MAX,FLT_MAX,ImVec2(0,105));
            if(ImGui::TreeNode("Latest observations and Q values")){
                for(size_t i=0;i<f.observations.size();++i)ImGui::Text("Observation [%zu] %.4f",i,f.observations[i]);
                for(size_t i=0;i<f.q.size();++i)ImGui::Text("Q [%zu] %.4f%s",i,f.q[i],int(i)==f.action?"  <- chosen":"");ImGui::TreePop();
            }
            ImGui::TextWrapped(state.obstacles?"Known 9x9 obstacle core. Exterior motion uses the separately trained tracking actor.":"One-axis target tracking. The resulting axis policy can be reused in X/Z.");ImGui::TextDisabled("Plots show the latest 2048 episodes; CSV keeps the full history.");
            ImGui::EndTable();
        }
        if(!state.directory.empty()){
            ImGui::Separator();ImGui::TextWrapped("Run folder: %s",state.directory.string().c_str());
#ifdef _WIN32
            if(ImGui::Button("Open run folder"))ShellExecuteW(nullptr,L"open",state.directory.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#endif
        }
        if(state.state=="Completed"){
            if(asset[0]==0)std::snprintf(asset,sizeof(asset),state.obstacles?"RLObstacle_%s":"RLTracking_%s",state.directory.filename().string().substr(0,90).c_str());
            ImGui::SetNextItemWidth(330);ImGui::InputText("Export asset name",asset,sizeof(asset));ImGui::SameLine();
            if(ImGui::Button("Export to Assets/Neural")){
                try{if(!IsValidNeuralAssetName(asset))throw std::runtime_error("Invalid asset name");const auto out=project/"Assets/Neural"/(std::string(asset)+".njn");
                    std::filesystem::create_directories(out.parent_path());std::filesystem::copy_file(state.model,out);
                    feedback="Exported "+std::string(asset)+". Select this asset in the agent inspector and restart Play. Existing assets are never overwritten.";
                }catch(const std::exception&e){feedback=e.what();}
            }
        }
        if(!feedback.empty())ImGui::TextWrapped("%s",feedback.c_str());
        ImGui::End();ImGui::Render();int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.06f,.075f,.095f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(window);
    }
    run.Cancel();run.Join();ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
}
/** @brief Parse native arguments and dispatch graphics-free tests or the interactive window. */
int Run(const std::vector<std::wstring>& args){
    auto project=Paths::InitializeProjectWorkspace();
    if(project.empty())project=std::filesystem::current_path();
    auto output=project/"TrainingRuns/SelfTest";bool self=false,obstacles=true;
    for(size_t i=1;i<args.size();++i){if(args[i]==L"--project"&&i+1<args.size())project=args[++i];else if(args[i]==L"--output-root"&&i+1<args.size())output=args[++i];else if(args[i]==L"--self-test")self=true;else if(args[i]==L"--task"&&i+1<args.size())obstacles=args[++i]!=L"tracking";}
    if(self)return SelfTest(output);
    try{return Window(project,obstacles);}catch(const std::exception&e){
#ifdef _WIN32
        MessageBoxA(nullptr,e.what(),"NJE Trainer",MB_OK|MB_ICONERROR);
#endif
        return 1;
    }
}
}
#ifdef _WIN32
/** @brief Windows GUI entry point; no extra console window is created. */
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){int count=0;LPWSTR*values=CommandLineToArgvW(GetCommandLineW(),&count);std::vector<std::wstring>args;for(int i=0;i<count;++i)args.emplace_back(values[i]);LocalFree(values);return Run(args);}
#else
int main(int argc,char**argv){std::vector<std::wstring>args;for(int i=0;i<argc;++i)args.push_back(std::filesystem::path(argv[i]).wstring());return Run(args);}
#endif
