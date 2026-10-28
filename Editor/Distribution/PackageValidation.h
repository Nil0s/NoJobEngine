/** @file PackageValidation.h @brief Check the relocated Alpha using its actual shipped assets and DLL. */
#pragma once
#include "Editor/Scripting/ProjectScriptManager.h"
#include "Engine/AI/Reinforcement/ObstacleArena.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/Scene/ScriptableEntity.h"
#include <fstream>
#include <cmath>
namespace NoJob {
/** @brief Validate package files, inference and live project-script behavior; write a local diagnostic. */
inline int ValidateAlphaPackage(const std::filesystem::path& root) {
    const auto application=Paths::IsInstalledApplication()?Paths::ExecutableDirectory():root;
    std::ofstream report(root/"PackageValidation.txt");
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    try {
        require(bool(report),"Cannot write diagnostics; extract to a writable folder.");
        for(const auto* file:{"Assets/Models/testplane_100x100.obj","Assets/Scenes/RLObstacleLearningDemo.nojobscene"})
            require(std::filesystem::is_regular_file(root/file),"A required package file is missing.");
        require(std::filesystem::is_regular_file(application/"NJETrainer.exe")||std::filesystem::is_regular_file(application/"NoJobTrainer.exe"),"The trainer executable is missing.");
        for(const auto* name:{"RLObstaclePolicy","RLTrackingPolicy","NeuralFollowerDemo","PongBrainDemo"}) {
            const auto model=Neural::NeuralNetwork::Load(NeuralAssetPath(name).string());
            require(!model.Layers().empty(),"Empty shipped model.");
            const auto prediction=model.Predict(std::vector<float>(model.Layers().front().Inputs(),0.f));
            for(float value:prediction)require(std::isfinite(value),"Non-finite model inference.");
        }
        require(ProjectScriptManager::Load(root),"Cannot load the packaged project-script DLL.");
        for(const auto* name:{"RLObstacleAgent","RLTargetPatrol","RLTrackingAgent","PongAgent"})
            require(ScriptRegistry::Find(name)!=nullptr,"A shipped project script was not registered.");
        {
            Scene scene;
            for(const auto& wall:RL::ArenaWalls()) {
                auto e=scene.CreateEntity("Wall");auto& t=e.GetComponent<TransformComponent>();
                t.Position=wall.centre;t.Scale=wall.size;e.AddComponent<BoxColliderComponent>();
            }
            auto agent=scene.CreateEntity("RLAgentCube");agent.GetComponent<TransformComponent>().Position=RL::CellWorld({0,4});
            agent.AddComponent<NativeScriptComponent>().ScriptName="RLObstacleAgent";
            auto target=scene.CreateEntity("RLTarget");const auto start=RL::CellWorld({8,4});
            target.GetComponent<TransformComponent>().Position=start;
            target.AddComponent<NativeScriptComponent>().ScriptName="RLTargetPatrol";
            scene.OnRuntimeStart();
            for(int frame=0;frame<900;++frame)scene.OnUpdate(1.f/60.f);
            NeuralAgent::Snapshot snapshot;
            auto* instance=scene.FindRuntimeScript(agent.GetHandle(),"RLObstacleAgent");
            require(instance&&instance->GetNeuralDebugSnapshot(snapshot),"The packaged agent did not produce live diagnostics.");
            require(snapshot.inputs.size()==8&&snapshot.outputs.size()==4,"Unexpected packaged agent dimensions.");
            require(glm::length(glm::vec3(scene.GetWorldTransform(target)[3])-start)>1.f,"The automatic target did not move.");
            scene.OnRuntimeStop();
        }
        ProjectScriptManager::Unload();
        report<<"PASS: relocated project, required files, four models, packaged DLL registration, 15s live RL and moving target.\n";
        return 0;
    }catch(const std::exception& e){ProjectScriptManager::Unload();report<<"FAIL: "<<e.what()<<'\n';return 1;}
}
}
