/** @file ObstacleLearningTests.cpp @brief Native baked reference and actual script regressions. */
#include "Engine/AI/Reinforcement/ObstacleNavBaseline.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/AI/Reinforcement/ObstacleRefinement.h"
#include "Engine/AI/Reinforcement/ObstacleWorldPolicy.h"
#include <iostream>
#include <fstream>
/** @brief Script registration exported by the same source compiled into the project DLL. */
extern "C" void NoJobRegister_RLObstacleAgent(void(*)(NoJob::ScriptDefinition));
/** @brief Register the actual obstacle script in the test host. */
void RegisterObstacle(NoJob::ScriptDefinition d){NoJob::ScriptRegistry::Register(std::move(d));}
/** @brief Require a condition in Debug and Release. */
void Require(bool condition,const char*message){if(!condition)throw std::runtime_error(message);}
/** @brief Evaluate all comparators and test the live script without any NavigationSystem. */
int main(int argc,char**argv){
 try{
    using namespace NoJob;using namespace NoJob::RL;
    const auto model=Neural::NeuralNetwork::Load("Assets/Neural/RLObstaclePolicy.njn");
    ObstacleNavBaseline native(std::filesystem::current_path());
    ObstacleReport settings;settings.policy="Assets/Neural/RLObstaclePolicy.njn";
    const auto report=EvaluateObstacles(model,settings,[&](GridCell a,GridCell b){return native.Run(a,b);});
    std::cout<<report.CSV()<<std::endl;
    Require(report.complete&&report.metrics[0].Rate()>=.9,"Learned policy below 90% acceptance");
    Require(report.metrics[0].Rate()>=report.metrics[1].Rate()+.3&&report.metrics[0].Rate()>=report.metrics[2].Rate()+.3,"No improvement over untrained/random");
    Require(report.metrics[0].collisions<report.metrics[2].collisions,"Learned policy did not reduce blocked attempts");
    if(argc>1){std::ofstream out(argv[1]);out<<report.CSV();Require(bool(out),"Report write failed");}
    ObstacleReport small;small.scenarios=10;
    Require(EvaluateObstacles(model,small).CSV()==EvaluateObstacles(model,small).CSV(),"Nondeterministic obstacle evaluation");
    Require(!EvaluateObstacles(model,small,{},[](size_t){return false;}).complete,"Cancelled report marked complete");
    const auto failures=FindObstacleFailures(model);
    int behindWallFailures=0;for(const auto&p:failures)if(p.goal.z==0&&p.goal.x>=6)++behindWallFailures;
    Require(behindWallFailures==0,"Policy still cycles behind the horizontal wall");
    std::cout<<"Core all-pairs audit: 5402 pairs; failures="<<failures.size()<<"; behind-horizontal-wall failures="<<behindWallFailures<<std::endl;
    const auto tracking=Neural::NeuralNetwork::Load("Assets/Neural/RLTrackingPolicy.njn");
    int exteriorCases=0;
    for(int z=0;z<9;++z)for(int x=0;x<9;++x){if(ObstacleEnvironment::Blocked({x,z}))continue;
        for(int gx=6;gx<=8;++gx)for(int gz:{-4,-11,-26}){GridCell p{x,z},goal{gx,gz};
            for(int n=0;n<192&&!(p==goal);++n){auto d=DecideObstacleWorld(model,tracking,p,goal);Require(ArenaSegmentClear(CellWorld(p),CellWorld(d.next)),"Exterior policy penetrated a wall");p=d.next;}
            Require(p==goal,"Exterior target behind horizontal wall unreachable");++exteriorCases;
        }
    }
    std::cout<<"PASS: "<<exteriorCases<<" exterior cases behind horizontal wall (-12.8, -24, -48) without navigation"<<std::endl;
    ObstacleEnvironment env;auto observation=env.Reset(42);Require(observation.size()==8,"Invalid sensors");
    env.SetEpisode({3,4},{8,4});auto blocked=env.Step(1);
    Require(env.Position()==GridCell{3,4}&&env.Collisions()==1&&blocked.reward<0,"Collision was hidden or rerouted");
    NoJobRegister_RLObstacleAgent(RegisterObstacle);
    Scene scene;
    for(const auto&w:ArenaWalls()){auto e=scene.CreateEntity("Learn wall");auto&t=e.GetComponent<TransformComponent>();t.Position=w.centre;t.Scale=w.size;e.AddComponent<BoxColliderComponent>();}
    Require(MatchesObstacleArena(scene),"Canonical geometry rejected");
    for(int z=0;z<9;++z)for(int x=0;x<9;++x)Require(SceneCellBlocked(scene,{x,z})==ObstacleEnvironment::Blocked({x,z}),"Runtime/training collision-map mismatch");
    auto agent=scene.CreateEntity("Learned cube");agent.GetComponent<TransformComponent>().Position=CellWorld({0,4});
    auto&script=agent.AddComponent<NativeScriptComponent>();script.ScriptName="RLObstacleAgent";script.Fields["ActionInterval"]=ScriptFieldValue::MakeFloat(.05f);
    auto target=scene.CreateEntity("RLTarget");target.GetComponent<TransformComponent>().Position=CellWorld({8,4});
    scene.OnRuntimeStart();
    auto reach=[&](GridCell goal){
        target.GetComponent<TransformComponent>().Position=CellWorld(goal);
        for(int t=0;t<900;++t){
            const glm::vec3 before(scene.GetWorldTransform(agent)[3]);scene.OnUpdate(1.f/60);
            const glm::vec3 after(scene.GetWorldTransform(agent)[3]);
            Require(ArenaSegmentClear(before,after),"Script penetrated a wall");
            Require(scene.GetNavigationSystem()==nullptr,"Learned script acquired a navigation fallback");
            if(glm::length(after-CellWorld(goal))<.01f)return;
        }throw std::runtime_error("Script failed to reach the demo goal");
    };
    reach({8,4});std::cout<<"PASS: actual learned script detour, no NavMesh, swept cube clearance"<<std::endl;
    auto*runtime=scene.FindRuntimeScript(agent.GetHandle(),"RLObstacleAgent");Require(runtime!=nullptr,"Actual script not instantiated");
    NeuralAgent::Snapshot snap;Require(runtime->GetNeuralDebugSnapshot(snap)&&snap.inputs.size()==8&&snap.outputs.size()==4,"Missing live Q/observation snapshot");
    reach({0,4});std::cout<<"PASS: actual script retarget after arrival"<<std::endl;
    target.GetComponent<TransformComponent>().Position=CellWorld({8,4});
    scene.OnUpdate(1.f/60);scene.OnUpdate(1.f/60);
    reach({0,4});std::cout<<"PASS: actual script retarget during interpolated movement"<<std::endl;
    reach({7,-4});reach({7,-11});reach({7,-26});
    Require(runtime->GetNeuralDebugSnapshot(snap)&&snap.inputs.size()==4&&snap.outputs.size()==6,"Missing exterior controller diagnostics");
    reach({-11,19});reach({8,4});
    std::cout<<"PASS: actual script exterior tracking, true goal arrival and return to core"<<std::endl;
    target.GetComponent<TransformComponent>().Position=CellWorld({4,4});scene.OnUpdate(1.f/60);
    const glm::vec3 stopped(scene.GetWorldTransform(agent)[3]);
    for(int t=0;t<60;++t)scene.OnUpdate(1.f/60);
    Require(glm::length(glm::vec3(scene.GetWorldTransform(agent)[3])-stopped)<.001f,"Blocked goal did not stop script");
    scene.DestroyEntity(target);scene.OnUpdate(1.f/60);Require(!runtime->GetNeuralDebugSnapshot(snap),"Deleted target left stale diagnostics");
    scene.OnRuntimeStop();
    std::cout<<"PASS: native comparison, metrics, sensors, cancellation, live snapshots and invalid target"<<std::endl;
    return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
}
