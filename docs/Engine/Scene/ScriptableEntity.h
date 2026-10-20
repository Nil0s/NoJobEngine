#pragma once
#include "Engine/Scene/Scene.h"
#include <string>
#include <vector>
#include <algorithm>

namespace NoJob
{
    class BTNativeRegistry;
    // Native C++ gameplay base class. Instances only exist while the Scene is
    // running; serialized state lives in NativeScriptComponent.
    class ScriptableEntity
    {
    public:
        virtual ~ScriptableEntity() = default;
        virtual void OnCreate() {}
        virtual void OnUpdate(float deltaTime) { (void)deltaTime; }
        virtual void OnDestroy() {}

        // Register reusable BT action/condition names. Never capture this in callbacks:
        // use BTRegisterScriptAction / BTRegisterScriptCondition instead.
        virtual void RegisterBehaviorTreeBindings(BTNativeRegistry&) const {}

        // Read-only gameplay AI diagnostics; independent of any decision framework.
        struct AIStateTransition
        {
            std::string From;
            std::string To;
            float RuntimeSeconds = 0.0f;
        };
        const std::string& GetAIDebugState() const { return m_AIDebugState; }
        const std::string& GetAIDebugPreviousState() const { return m_AIDebugPreviousState; }
        float GetAIDebugTimeInState() const { return m_AIDebugTimeInState; }
        float GetAIDebugRuntimeSeconds() const { return m_AIDebugRuntimeSeconds; }
        const std::vector<AIStateTransition>& GetAIDebugTransitions() const { return m_AIDebugTransitions; }

        template<typename T>
        T& GetComponent() { return m_Entity.GetComponent<T>(); }

        template<typename T>
        bool HasComponent() const { return m_Entity.HasComponent<T>(); }

        Entity GetEntity() const { return m_Entity; }

    protected:
        void TickAIDebug(float dt)
        {
            if (dt > 0.0f)
            {
                m_AIDebugRuntimeSeconds += dt;
                m_AIDebugTimeInState += dt;
            }
        }
        void SetAIDebugState(const char* state)
        {
            if (!state || m_AIDebugState == state) return;
            if (!m_AIDebugState.empty())
            {
                m_AIDebugPreviousState = m_AIDebugState;
                m_AIDebugTransitions.push_back({m_AIDebugState, state, m_AIDebugRuntimeSeconds});
                if (m_AIDebugTransitions.size() > 16)
                    m_AIDebugTransitions.erase(m_AIDebugTransitions.begin());
            }
            m_AIDebugState = state;
            m_AIDebugTimeInState = 0.0f;
        }
        NativeScriptComponent& ScriptComponent()
        {
            return m_BoundScript ? *m_BoundScript : m_Entity.GetComponent<NativeScriptComponent>();
        }

        float GetFloat(const std::string& name, float fallback = 0.0f) const
        {
            const auto& fields = (m_BoundScript ? *m_BoundScript : m_Entity.GetComponent<NativeScriptComponent>()).Fields;
            auto it=fields.find(name); return it==fields.end()?fallback:it->second.Float;
        }
        int GetInt(const std::string& name, int fallback = 0) const
        {
            const auto& fields = (m_BoundScript ? *m_BoundScript : m_Entity.GetComponent<NativeScriptComponent>()).Fields;
            auto it=fields.find(name); return it==fields.end()?fallback:it->second.Int;
        }
        bool GetBool(const std::string& name, bool fallback = false) const
        {
            const auto& fields = (m_BoundScript ? *m_BoundScript : m_Entity.GetComponent<NativeScriptComponent>()).Fields;
            auto it=fields.find(name); return it==fields.end()?fallback:it->second.Bool;
        }
        glm::vec3 GetVec3(const std::string& name, glm::vec3 fallback = {}) const
        {
            const auto& fields = (m_BoundScript ? *m_BoundScript : m_Entity.GetComponent<NativeScriptComponent>()).Fields;
            auto it=fields.find(name); return it==fields.end()?fallback:it->second.Vec3;
        }

    private:
        std::string m_AIDebugState;
        std::string m_AIDebugPreviousState;
        float m_AIDebugTimeInState = 0.0f;
        float m_AIDebugRuntimeSeconds = 0.0f;
        std::vector<AIStateTransition> m_AIDebugTransitions;
        Entity m_Entity;
        NativeScriptComponent* m_BoundScript = nullptr;
        friend class Scene;
    };

    // V1.4 public-facing name. Keep ScriptableEntity as a compatibility alias
    // for code written during the early native-scripting prototype.
    using Script = ScriptableEntity;
}
