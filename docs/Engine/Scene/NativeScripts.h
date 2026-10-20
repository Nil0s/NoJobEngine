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
                    const std::string output = GetStringField("TrainingOutputName", "NeuralFollowerNew");
                    const std::string source = GetStringField("ResumeSourceName", "");
                    const auto history=NeuralAgent::TrainPreset(output, source, NeuralAgent::TrainingPreset::FollowXZ, GetInt("Epochs",1500), GetFloat("LearningRate",0.01f), GetBool("ResumeTraining",false), GetBool("OverwriteModel",false));
                    std::printf("[NeuralTrainingAgent] trained %s, final MSE %.8f\n", output.c_str(), history.epochLoss.empty()?0.f:history.epochLoss.back());
                    m_Model.emplace(Neural::NeuralNetwork::Load(NeuralAssetPath(output).string()));
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
        std::string GetStringField(const char* key, const char* fallback) {
            const auto& f=ScriptComponent().Fields; auto it=f.find(key);
            return it!=f.end() && it->second.Type==ScriptFieldType::String ? it->second.String : fallback;
        }
        std::string GetModelName() {
            const auto& fields=ScriptComponent().Fields;
            auto it=fields.find("ModelName");
            return it!=fields.end() && it->second.Type==ScriptFieldType::String ? it->second.String : "NeuralFollowerDemo";
        }
        std::optional<NoJob::Neural::NeuralNetwork> m_Model;
    };

    // Configurable neural agent: observation/action bindings are editor properties,
    // independent of Behavior Trees and of the navigation system.
    // ObservationPreset: 0 = relative XZ, 1 = target Y/self Y/delta Y,
    // 2 = Pong (target X/Y, target velocity X/Y, self Y, horizontal distance).
    // ActionMode: 0 = NavAgent XZ, 1 = Transform Y, 2 = Transform XZ.
    class GenericNeuralAgent final : public Script {
    public:
        void OnCreate() override {
            m_Model.reset(); m_HasPrevious=false;
            const std::string name=GetStringField("ActiveModel");
            try {
                if (name.empty()) throw std::runtime_error("Select an ActiveModel");
                m_Model.emplace(Neural::NeuralNetwork::Load(NeuralAssetPath(name).string()));
                const int preset=std::clamp(GetInt("ObservationPreset",0),0,2);
                const size_t inputs=preset==0?2:(preset==1?3:6);
                const size_t outputs=preset==0?2:1;
                if(m_Model->Layers().empty() || m_Model->Layers().front().Inputs()!=inputs ||
                   m_Model->Layers().back().Outputs()!=outputs)
                    throw std::runtime_error("Model input/output dimensions do not match ObservationPreset");
                SetAIDebugState("Neural: ready");
            } catch(const std::exception& e) {
                std::fprintf(stderr,"[GenericNeuralAgent] %s\n",e.what());
                SetAIDebugState("Neural: model error"); m_Model.reset();
            }
        }
        void OnDestroy() override { NeuralAgent::Clear(GetEntity().GetHandle()); }
        void OnUpdate(float dt) override {
            TickAIDebug(dt);
            if(!m_Model || dt<=0.f || !HasComponent<TransformComponent>()) return;
            auto* scene=GetEntity().GetScene(); if(!scene) return;
            const auto tag=GetStringField("TargetTag");
            Entity target{};
            for(Entity e:scene->GetEntities()) {
                if(e==GetEntity() || !e.HasComponent<TagComponent>() || !e.HasComponent<TransformComponent>()) continue;
                if(e.GetComponent<TagComponent>().Tag==tag) { target=e; break; }
            }
            if(!target) { SetAIDebugState("Neural: target missing"); NeuralAgent::Clear(GetEntity().GetHandle()); return; }
            auto& own=GetComponent<TransformComponent>();
            const glm::vec3 pos=own.Position;
            const glm::vec3 tp=target.GetComponent<TransformComponent>().Position;
            const glm::vec3 delta=tp-pos;
            const float scale=std::max(0.01f,GetFloat("ObservationScale",10.f));
            const int preset=std::clamp(GetInt("ObservationPreset",0),0,2);
            const int action=std::clamp(GetInt("ActionMode",0),0,2);
            std::vector<float> inputs;
            if(preset==0) {
                inputs={std::clamp(delta.x/scale,-1.f,1.f),std::clamp(delta.z/scale,-1.f,1.f)};
            } else if(preset==1) {
                inputs={std::clamp(tp.y/scale,-1.f,1.f),std::clamp(pos.y/scale,-1.f,1.f),
                        std::clamp(delta.y/scale,-1.f,1.f)};
            } else {
                const glm::vec3 velocity=m_HasPrevious ? (tp-m_PreviousTarget)/dt : glm::vec3(0.f);
                inputs={std::clamp(tp.x/scale,-1.f,1.f),std::clamp(tp.y/scale,-1.f,1.f),
                        std::clamp(velocity.x/scale,-1.f,1.f),std::clamp(velocity.y/scale,-1.f,1.f),
                        std::clamp(pos.y/scale,-1.f,1.f),std::clamp(std::abs(delta.x)/scale,0.f,1.f)};
            }
            m_PreviousTarget=tp; m_HasPrevious=true;
            try {
                const auto outputs=m_Model->Predict(inputs);
                if(outputs.size()!=(preset==0?2u:1u)) return;
                for(float x:outputs) if(!std::isfinite(x)) return;
                NeuralAgent::Snapshot snap;
                snap.model=GetStringField("ActiveModel"); snap.status="Generic neural inference";
                snap.inputs=inputs; snap.outputs=outputs; snap.distance=glm::length(delta);
                snap.activations=NeuralAgent::Activations(*m_Model,inputs);
                NeuralAgent::Publish(GetEntity().GetHandle(),std::move(snap));
                const float speed=std::max(0.f,GetFloat("MovementSpeed",3.f));
                if(action==0) {
                    if(outputs.size()!=2 || !HasComponent<NavAgentComponent>()) return;
                    auto& nav=GetComponent<NavAgentComponent>();
                    if(!nav.Enabled) return;
                    const glm::vec3 direction(outputs[0],0.f,outputs[1]);
                    const float length=glm::length(direction);
                    if(length<0.05f) {nav.HasDestination=false;return;}
                    nav.Destination=pos+(direction/length)*std::min(glm::length(delta),3.f);
                    nav.HasDestination=true;
                } else if(action==1) {
                    if(outputs.size()!=1) return;
                    own.Position.y+=std::clamp(outputs[0],-1.f,1.f)*speed*dt;
                } else {
                    if(outputs.size()!=2) return;
                    own.Position.x+=std::clamp(outputs[0],-1.f,1.f)*speed*dt;
                    own.Position.z+=std::clamp(outputs[1],-1.f,1.f)*speed*dt;
                }
                SetAIDebugState("Neural: inference active");
            } catch(const std::exception& e) {
                std::fprintf(stderr,"[GenericNeuralAgent] Inference: %s\n",e.what());
                SetAIDebugState("Neural: inference error");
            }
        }
    private:
        std::string GetStringField(const char* key) {
            const auto& fields=ScriptComponent().Fields;
            const auto it=fields.find(key);
            return it!=fields.end() && it->second.Type==ScriptFieldType::String?it->second.String:std::string{};
        }
        std::optional<Neural::NeuralNetwork> m_Model;
        glm::vec3 m_PreviousTarget{0.f}; bool m_HasPrevious=false;
    };

    // Simple editor demo ball. No NavMesh required; this is a tracking example,
    // not a complete scored Pong game with paddle collision rules.
    class PongBallDemo final : public Script {
    public:
        void OnUpdate(float dt) override {
            auto& t=GetComponent<TransformComponent>();
            t.Position.x+=m_Direction.x*GetFloat("Speed",4.f)*dt;
            t.Position.y+=m_Direction.y*GetFloat("Speed",4.f)*dt;
            const float boundX=std::max(1.f,GetFloat("BoundX",8.f));
            const float boundY=std::max(1.f,GetFloat("BoundY",4.f));
            if(t.Position.x>boundX) {t.Position.x=boundX;m_Direction.x=-std::abs(m_Direction.x);}
            if(t.Position.x< -boundX) {t.Position.x=-boundX;m_Direction.x=std::abs(m_Direction.x);}
            if(t.Position.y>boundY) {t.Position.y=boundY;m_Direction.y=-std::abs(m_Direction.y);}
            if(t.Position.y< -boundY) {t.Position.y=-boundY;m_Direction.y=std::abs(m_Direction.y);}
        }
    private:
        glm::vec3 m_Direction{.83f,.56f,0.f};
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
            {"TrainingOutputName",ScriptFieldValue::MakeString("NeuralFollowerNew")},
            {"ResumeSourceName",ScriptFieldValue::MakeString("")},
            {"TrainingPreset",ScriptFieldValue::MakeInt(0)},
            {"TrainOnPlay",ScriptFieldValue::MakeBool(false)},
            {"ResumeTraining",ScriptFieldValue::MakeBool(false)},
            {"OverwriteModel",ScriptFieldValue::MakeBool(false)},
            {"Epochs",ScriptFieldValue::MakeInt(1500)},
            {"LearningRate",ScriptFieldValue::MakeFloat(0.01f)}},
            []{return std::make_unique<NeuralTrainingAgent>();}});
        ScriptRegistry::Register({"GenericNeuralAgent","AI / Neural",{
            {"ActiveModel",ScriptFieldValue::MakeString("")},
            {"TargetTag",ScriptFieldValue::MakeString("NeuralTarget")},
            {"ObservationPreset",ScriptFieldValue::MakeInt(0)},
            {"ActionMode",ScriptFieldValue::MakeInt(0)},
            {"ObservationScale",ScriptFieldValue::MakeFloat(10.f)},
            {"MovementSpeed",ScriptFieldValue::MakeFloat(3.f)},
            {"TrainingOutputName",ScriptFieldValue::MakeString("NewNeuralModel")},
            {"ResumeSourceName",ScriptFieldValue::MakeString("")},
            {"TrainingPreset",ScriptFieldValue::MakeInt(0)},
            {"ResumeTraining",ScriptFieldValue::MakeBool(false)},
            {"OverwriteModel",ScriptFieldValue::MakeBool(false)},
            {"Epochs",ScriptFieldValue::MakeInt(1500)},
            {"LearningRate",ScriptFieldValue::MakeFloat(0.01f)}},
            []{return std::make_unique<GenericNeuralAgent>();}});
        ScriptRegistry::Register({"PongBallDemo","AI / Neural",{
            {"Speed",ScriptFieldValue::MakeFloat(4.f)},
            {"BoundX",ScriptFieldValue::MakeFloat(8.f)},
            {"BoundY",ScriptFieldValue::MakeFloat(4.f)}},
            []{return std::make_unique<PongBallDemo>();}});
        ScriptRegistry::Register({"Oscillator","Transform",{
            {"Amplitude",ScriptFieldValue::MakeFloat(1.0f)},
            {"Frequency",ScriptFieldValue::MakeFloat(1.0f)},
            {"Axis",ScriptFieldValue::MakeVec3({0.0f,1.0f,0.0f})}},
            []{return std::make_unique<Oscillator>();}});
    }
}
