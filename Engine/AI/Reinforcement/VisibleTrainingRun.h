/** @file VisibleTrainingRun.h @brief Isolated training job, telemetry, controls and durable outputs. */
#pragma once
#include "ObstacleRefinement.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <chrono>
#include <cctype>
namespace NoJob::RL {
/** @brief Independent run parameters; each run writes a new directory. */
struct VisibleTrainingConfig {
    std::string name="obstacle";
    int episodes=6000,hidden=64,batch=32;
    float rate=.0005f,gamma=.97f;
    uint32_t seed=42;
    bool refinement=true,obstacles=true;
};
/** @brief Thread-safe snapshot copied by the trainer UI. */
struct VisibleTrainingSnapshot {
    TrainingStep frame;
    uint64_t transitions=0;
    int baseEpisodes=0,totalEpisodes=0;
    bool obstacles=true;
    std::string status="Ready",state="Idle";
    std::filesystem::path directory,model;
    std::vector<float> rewards,losses;
    std::vector<GridCell> trail;
};
/** @brief One worker with pause/step/cancel; no active editor scene or shared model is mutated. */
class VisibleTrainingRun {
public:
    /** @brief Stop and join safely on window close. */
    ~VisibleTrainingRun(){Cancel();Join();}
    /** @brief Whether this instance currently owns an active training worker. */
    bool Running()const{return running_;}
    /** @brief Copy presentation data without exposing model weights across threads. */
    VisibleTrainingSnapshot Snapshot()const{std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
    /** @brief Control a bounded optional display delay, in milliseconds per transition. */
    void SetDelay(int value){delay_=std::clamp(value,0,250);wake_.notify_all();}
    /** @brief Pause before the next environment transition. */
    void Pause(bool value){paused_=value;wake_.notify_all();}
    /** @brief Inspect pause state for UI feedback. */
    bool Paused()const{return paused_;}
    /** @brief Permit exactly one further transition while paused. */
    void Step(){++tokens_;wake_.notify_all();}
    /** @brief Request cancellation; partial history remains, but no final policy is saved. */
    void Cancel(){cancel_=true;paused_=false;wake_.notify_all();}
    /** @brief Join a completed or cancelled worker. */
    void Join(){if(worker_.joinable())worker_.join();}
    /** @brief Start a run in a collision-resistant, per-process-safe directory. */
    void Start(const std::filesystem::path& outputRoot,VisibleTrainingConfig config){
        if(running_)throw std::runtime_error("A run is already active in this window");Join();
        if(config.episodes<1||config.episodes>1000000||config.hidden<4||config.hidden>512||config.batch<1||config.batch>512||
           !std::isfinite(config.rate)||config.rate<=0||!std::isfinite(config.gamma)||config.gamma<0||config.gamma>1)
            throw std::invalid_argument("Invalid training settings");
        cancel_=false;paused_=false;tokens_=0;
        {std::lock_guard<std::mutex> lock(mutex_);snapshot_={};snapshot_.state="Running";snapshot_.status="Preparing isolated run...";snapshot_.baseEpisodes=config.episodes;snapshot_.totalEpisodes=config.episodes+((config.obstacles&&config.refinement)?3000:0);snapshot_.obstacles=config.obstacles;}
        running_=true;
        worker_=std::thread([this,outputRoot,config]{Work(outputRoot,config);});
    }
private:
    /** @brief Publish actual transition data and honor visual speed, pause and cancellation. */
    bool Publish(const TrainingStep& frame){
        {std::lock_guard<std::mutex> lock(mutex_);
            if(snapshot_.frame.episode!=frame.episode||snapshot_.frame.phase!=frame.phase)snapshot_.trail.clear();
            snapshot_.frame=frame;++snapshot_.transitions;
            snapshot_.trail.push_back(frame.position);if(snapshot_.trail.size()>256)snapshot_.trail.erase(snapshot_.trail.begin());
        }
        std::unique_lock<std::mutex> lock(control_);
        wake_.wait(lock,[&]{return cancel_||!paused_||tokens_>0;});
        if(cancel_)return false;
        if(paused_&&tokens_>0)--tokens_;
        const int delay=delay_;if(delay>0)wake_.wait_for(lock,std::chrono::milliseconds(delay),[&]{return bool(cancel_);});
        return !cancel_;
    }
    /** @brief Run reward-based training, log episodes, then save a policy only on completion. */
    void Work(const std::filesystem::path& root,const VisibleTrainingConfig& config){
        try{
            std::filesystem::create_directories(root);
            std::string name;for(unsigned char c:config.name)name+=std::isalnum(c)||c=='_'||c=='-'?char(c):'_';
            if(name.empty())name="obstacle";if(name.size()>48)name.resize(48);
            std::filesystem::path directory;
            for(int attempt=0;attempt<100;++attempt){
                const auto time=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                directory=root/("run_"+name+"_"+std::to_string(time)+"_"+std::to_string(std::random_device{}()));
                if(std::filesystem::create_directory(directory))break;directory.clear();
            }
            if(directory.empty())throw std::runtime_error("Cannot allocate a unique run directory");
            {std::lock_guard<std::mutex> lock(mutex_);snapshot_.directory=directory;snapshot_.status="Training core DQN...";}
            std::ofstream info(directory/"run.txt");info<<"Task: "<<(config.obstacles?"obstacle core 9x9":"axis target tracking")<<"\nBase episodes: "<<config.episodes<<"\nRefinement episodes: "<<((config.obstacles&&config.refinement)?3000:0)
                <<"\nSeed: "<<config.seed<<"\nHidden units: "<<config.hidden<<"\nBatch: "<<config.batch<<"\nLearning rate: "<<config.rate<<"\nGamma: "<<config.gamma<<"\n";info.close();
            if(!info)throw std::runtime_error("Cannot write run configuration");
            std::ofstream history(directory/"episodes.csv");history<<"phase,episode,reward,epsilon,loss,steps\n";
            if(!history)throw std::runtime_error("Cannot create episode history");
            auto record=[&](int phase,size_t n,const EpisodeMetrics&m){
                history<<phase<<','<<n<<','<<m.totalReward<<','<<m.epsilon<<','<<m.loss<<','<<m.steps<<'\n';history.flush();
                if(!history)throw std::runtime_error("Cannot append episode history");
                std::lock_guard<std::mutex> lock(mutex_);snapshot_.rewards.push_back(m.totalReward);snapshot_.losses.push_back(m.loss);
                if(snapshot_.rewards.size()>2048){snapshot_.rewards.erase(snapshot_.rewards.begin());snapshot_.losses.erase(snapshot_.losses.begin());}
            };
            auto options=config.obstacles?ObstacleOptions():Options{};options.seed=config.seed;options.hiddenUnits=config.hidden;options.batchSize=config.batch;
            options.warmup=std::max<size_t>(128,config.batch);options.learningRate=config.rate;options.gamma=config.gamma;
            ObstacleEnvironment core;TrackingEnvironment axis;
            Environment&environment=config.obstacles?static_cast<Environment&>(core):static_cast<Environment&>(axis);
            DQNAgent agent(environment.ObservationSize(),environment.ActionCount(),options);
            const int maxSteps=config.obstacles?64:40;
            for(int ep=0;ep<config.episodes&&!cancel_;++ep){
                auto observations=environment.Reset(config.seed+ep);EpisodeMetrics m;
                for(int t=0;t<maxSteps;++t){
                    const auto q=agent.Policy().Predict(observations);const int action=agent.Act(observations);auto r=environment.Step(action);
                    agent.Observe({observations,action,r.reward,r.observation,r.terminated});
                    TrainingStep frame;frame.episode=ep+1;frame.step=t+1;frame.action=action;frame.obstacles=config.obstacles;
                    if(config.obstacles){frame.position=core.Position();frame.goal=core.Goal();frame.collisions=core.Collisions();}
                    else{frame.axisPosition=r.observation[0];frame.axisGoal=r.observation[0]+r.observation[1];}
                    frame.reward=r.reward;m.totalReward+=r.reward;frame.totalReward=m.totalReward;frame.terminal=r.terminated;
                    frame.epsilon=agent.Epsilon();frame.loss=agent.LastLoss();frame.observations=observations;frame.q=q;
                    observations=r.observation;m.steps=t+1;if(!Publish(frame)||r.terminated)break;
                }
                agent.EndEpisode();m.epsilon=agent.Epsilon();m.loss=agent.LastLoss();record(0,ep+1,m);
            }
            std::optional<Neural::NeuralNetwork> policy;
            if(!cancel_){
                if(config.obstacles&&config.refinement){
                    {std::lock_guard<std::mutex> lock(mutex_);snapshot_.status="Refining stalled core cases...";}
                    policy=RefineObstaclePolicy(agent.Policy(),[&](size_t n,const EpisodeMetrics&m){record(1,n,m);return !cancel_;},[&](const TrainingStep&f){return Publish(f);});
                }else policy=agent.Policy();
            }
            if(cancel_||!policy){std::lock_guard<std::mutex> lock(mutex_);snapshot_.state="Cancelled";snapshot_.status="Cancelled. Episode history retained; final policy not saved.";}
            else{
                const auto path=directory/"policy.njn";policy->Save(path.string());
                std::lock_guard<std::mutex> lock(mutex_);snapshot_.model=path;snapshot_.state="Completed";snapshot_.status="Completed. Policy and episode history saved in this run folder.";
            }
        }catch(const std::exception&e){std::lock_guard<std::mutex> lock(mutex_);snapshot_.state="Failed";snapshot_.status=e.what();}
        running_=false;
    }
    mutable std::mutex mutex_;std::mutex control_;std::condition_variable wake_;
    VisibleTrainingSnapshot snapshot_;std::thread worker_;
    std::atomic<bool> running_{false},cancel_{false},paused_{false};
    std::atomic<int> delay_{0};std::atomic<unsigned>tokens_{0};
};
}
