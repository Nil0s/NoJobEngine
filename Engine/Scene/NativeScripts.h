#pragma once
#include "Engine/Scene/ScriptRegistry.h"
#include <cmath>

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

    inline void RegisterBuiltinScripts()
    {
        static bool registered=false; if(registered) return; registered=true;
        ScriptRegistry::Register({"Rotator","Transform",{
            {"Speed",ScriptFieldValue::MakeFloat(1.0f)},
            {"Axis",ScriptFieldValue::MakeVec3({0.0f,1.0f,0.0f})}},
            []{return std::make_unique<Rotator>();}});
        ScriptRegistry::Register({"Oscillator","Transform",{
            {"Amplitude",ScriptFieldValue::MakeFloat(1.0f)},
            {"Frequency",ScriptFieldValue::MakeFloat(1.0f)},
            {"Axis",ScriptFieldValue::MakeVec3({0.0f,1.0f,0.0f})}},
            []{return std::make_unique<Oscillator>();}});
    }
}
