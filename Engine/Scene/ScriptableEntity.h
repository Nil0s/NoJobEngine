#pragma once
#include "Engine/Scene/Scene.h"

namespace NoJob
{
    class ScriptableEntity
    {
    public:
        virtual ~ScriptableEntity() = default;
        virtual void OnCreate() {}
        virtual void OnUpdate(float deltaTime) { (void)deltaTime; }
        virtual void OnDestroy() {}

        template<typename T>
        T& GetComponent() { return m_Entity.GetComponent<T>(); }

        Entity GetEntity() const { return m_Entity; }

    private:
        Entity m_Entity;
        friend class Scene;
    };
}
