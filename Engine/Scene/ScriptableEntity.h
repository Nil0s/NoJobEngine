#pragma once
#include "Engine/Scene/Scene.h"

namespace NoJob
{
    // Native C++ gameplay base class. Instances only exist while the Scene is
    // running; serialized state lives in NativeScriptComponent.
    class ScriptableEntity
    {
    public:
        virtual ~ScriptableEntity() = default;
        virtual void OnCreate() {}
        virtual void OnUpdate(float deltaTime) { (void)deltaTime; }
        virtual void OnDestroy() {}

        template<typename T>
        T& GetComponent() { return m_Entity.GetComponent<T>(); }

        template<typename T>
        bool HasComponent() const { return m_Entity.HasComponent<T>(); }

        Entity GetEntity() const { return m_Entity; }

    protected:
        NativeScriptComponent& ScriptComponent()
        {
            return m_Entity.GetComponent<NativeScriptComponent>();
        }

        float GetFloat(const std::string& name, float fallback = 0.0f) const
        {
            const auto& fields = m_Entity.GetComponent<NativeScriptComponent>().Fields;
            auto it=fields.find(name); return it==fields.end()?fallback:it->second.Float;
        }
        int GetInt(const std::string& name, int fallback = 0) const
        {
            const auto& fields = m_Entity.GetComponent<NativeScriptComponent>().Fields;
            auto it=fields.find(name); return it==fields.end()?fallback:it->second.Int;
        }
        bool GetBool(const std::string& name, bool fallback = false) const
        {
            const auto& fields = m_Entity.GetComponent<NativeScriptComponent>().Fields;
            auto it=fields.find(name); return it==fields.end()?fallback:it->second.Bool;
        }
        glm::vec3 GetVec3(const std::string& name, glm::vec3 fallback = {}) const
        {
            const auto& fields = m_Entity.GetComponent<NativeScriptComponent>().Fields;
            auto it=fields.find(name); return it==fields.end()?fallback:it->second.Vec3;
        }

    private:
        Entity m_Entity;
        friend class Scene;
    };

    // V1.4 public-facing name. Keep ScriptableEntity as a compatibility alias
    // for code written during the early native-scripting prototype.
    using Script = ScriptableEntity;
}
