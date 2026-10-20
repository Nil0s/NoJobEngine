#pragma once
#include "Engine/Scene/ScriptRegistry.h"
#include <cmath>
#include <optional>
#include <cstdio>
#include "Engine/AI/Neural/NeuralAgentTraining.h"
#include <optional>
#include <algorithm>
#include <cstdio>
#include "Engine/AI/Neural/NeuralNetwork.h"
#include "Engine/AI/Neural/NeuralAssetPaths.h"

namespace NoJob
{
    class Rotator final : public Script
    {
    public:
        void OnUpdate(float deltaTime) override
        {
            auto& t=GetComponent<TransformComponent>();
            t.Rotation += GetVec3("Axis", {0.0f,1.0f,0.0f}) * GetFloat("Speed",1.0f) * deltaTime;
        }
    };

    class Oscillator final : public Script
    {
    public:
        void OnCreate() override { m_Origin=GetComponent<TransformComponent>().Position; m_Time=0.0f; }
        void OnUpdate(float deltaTime) override
        {
            m_Time += deltaTime;
            auto& t=GetComponent<TransformComponent>();
            const auto axis=GetVec3("Axis",{0.0f,1.0f,0.0f});
            t.Position=m_Origin + axis * (std::sin(m_Time*GetFloat("Frequency",1.0f))*GetFloat("Amplitude",1.0f));
        }
    private:
        glm::vec3 m_Origin{0.0f}; float m_Time=0.0f;
    };

    // V2.0.6 supervised learning example. Uses the existing NavAgent and a named .njn asset.
    // Training is deterministic and runs once when Play starts if TrainOnPlay is enabled.
    class NeuralTrainingAgent final : public Script
    {
    public:
        void OnCreate() override
        {
            SetAIDebugState("Neural: initializing");
            try
            {
                const auto& fields = ScriptComponent().Fields;
                const auto it = fields.find("ModelName");
                const std::string name = it != fields.end() &&
                    it->second.Type == ScriptFieldType::String ? it->second.String : "NeuralFollowerDemo";
                const auto path = NeuralAssetPath(name);
                if (GetBool("TrainOnPlay", false))
                {
                    const auto history=NeuralAgent::TrainDirectionModel(name, GetInt("Epochs",1500), GetFloat("LearningRate",0.01f));
                    std::printf("[NeuralTrainingAgent] trained %s, final MSE %.8f\n", name.c_str(), history.epochLoss.empty()?0.f:history.epochLoss.back());
                    m_Model.emplace(Neural::NeuralNetwork::Load(path.string()));
                }
                else m_Model.emplace(NoJob::Neural::NeuralNetwork::Load(path.string()));
                if (m_Model->Layers().front().Inputs()!=2 || m_Model->Layers().back().Outputs()!=2)
                    throw std::runtime_error("NeuralTrainingAgent requires a 2-input / 2-output model");
                SetAIDebugState("Neural: ready");
            }
            catch (const std::exception& e)
            {
                std::fprintf(stderr,"[NeuralTrainingAgent] %s\n",e.what());
                m_Model.reset();
                SetAIDebugState("Neural: model error");
            }
        }
        void OnDestroy() override { NeuralAgent::Clear(GetEntity().GetHandle()); }
        void OnUpdate(float dt) override
        {
            TickAIDebug(dt);
            if (!m_Model || !HasComponent<NavAgentComponent>()) return;
            Scene* scene=GetEntity().GetScene();
            if (!scene) return;
            const auto& fields=ScriptComponent().Fields;
            const auto it=fields.find("TargetTag");
            const std::string targetTag=it!=fields.end() && it->second.Type==ScriptFieldType::String
                ? it->second.String : "NeuralTarget";
            const glm::vec3 pos=GetComponent<TransformComponent>().Position;
            for (Entity target:scene->GetEntities())
            {
                if (target==GetEntity() || !target.HasComponent<TagComponent>() ||
                    target.GetComponent<TagComponent>().Tag!=targetTag) continue;
                const glm::vec3 delta=target.GetComponent<TransformComponent>().Position-pos;
                const float distance=std::sqrt(delta.x*delta.x+delta.z*delta.z);
                auto& nav=GetComponent<NavAgentComponent>();
                if (distance < std::max(0.1f,nav.StoppingDistance))
                {
                    nav.HasDestination=false;
                    SetAIDebugState("Neural: target reached");
                    return;
                }
                const std::vector<float> input{std::clamp(delta.x/10.f,-1.f,1.f),
                                               std::clamp(delta.z/10.f,-1.f,1.f)};
                const auto prediction=m_Model->Predict(input);
                NeuralAgent::Snapshot snapshot;
                snapshot.model=GetModelName(); snapshot.status="Inference";
                snapshot.inputs=input; snapshot.outputs=prediction; snapshot.distance=distance;
                snapshot.activations=NeuralAgent::Activations(*m_Model,input);
                NeuralAgent::Publish(GetEntity().GetHandle(),std::move(snapshot));
                if (prediction.size()!=2 || !std::isfinite(prediction[0]) || !std::isfinite(prediction[1]))
                {
                    SetAIDebugState("Neural: invalid prediction");
                    return;
                }
                glm::vec3 direction(prediction[0],0.f,prediction[1]);
                const float length=std::sqrt(direction.x*direction.x+direction.z*direction.z);
                if (length<0.05f) { SetAIDebugState("Neural: no direction"); return; }
                direction/=length;
                // NavMesh pathfinding remains the responsibility of NavAgent.
                nav.Destination=pos+direction*std::min(distance,3.f);
                nav.HasDestination=true;
                SetAIDebugState("Neural: following target");
                return;
            }
            SetAIDebugState("Neural: target not found");
        }
    private:
        std::string GetModelName() {
            const auto& fields=ScriptComponent().Fields;
            auto it=fields.find("ModelName");
            return it!=fields.end() && it->second.Type==ScriptFieldType::String ? it->second.String : "NeuralFollowerDemo";
        }
        std::optional<NoJob::Neural::NeuralNetwork> m_Model;
    };

    inline void RegisterBuiltinScripts()
    {
        static bool registered=false; if(registered) return; registered=true;
        ScriptRegistry::Register({"Rotator","Transform",{
            {"Speed",ScriptFieldValue::MakeFloat(1.0f)},
            {"Axis",ScriptFieldValue::MakeVec3({0.0f,1.0f,0.0f})}},
            []{return std::make_unique<Rotator>();}});
        ScriptRegistry::Register({"NeuralTrainingAgent","AI / Neural",{
            {"ModelName",ScriptFieldValue::MakeString("NeuralFollowerDemo")},
            {"TargetTag",ScriptFieldValue::MakeString("NeuralTarget")},
            {"TrainOnPlay",ScriptFieldValue::MakeBool(false)},
            {"Epochs",ScriptFieldValue::MakeInt(1500)},
            {"LearningRate",ScriptFieldValue::MakeFloat(0.01f)}},
            []{return std::make_unique<NeuralTrainingAgent>();}});
        ScriptRegistry::Register({"Oscillator","Transform",{
            {"Amplitude",ScriptFieldValue::MakeFloat(1.0f)},
            {"Frequency",ScriptFieldValue::MakeFloat(1.0f)},
            {"Axis",ScriptFieldValue::MakeVec3({0.0f,1.0f,0.0f})}},
            []{return std::make_unique<Oscillator>();}});
    }
}
