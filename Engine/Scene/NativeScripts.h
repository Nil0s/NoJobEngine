#pragma once
#include "Engine/Scene/ScriptableEntity.h"

namespace NoJob
{
    class Rotator final : public ScriptableEntity
    {
    public:
        explicit Rotator(float speed) : m_Speed(speed) {}
        void OnUpdate(float deltaTime) override
        {
            GetComponent<TransformComponent>().Rotation.y += m_Speed * deltaTime;
        }
    private:
        float m_Speed = 1.0f;
    };
}
