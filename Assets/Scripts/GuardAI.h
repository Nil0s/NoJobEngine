#pragma once
#include "Engine/Scene/ScriptableEntity.h"

namespace NoJob
{
    class GuardAI final : public Script
    {
    public:
        float ChaseStoppingDistance = 1.5f;

        void OnCreate() override;
        void OnUpdate(float deltaTime) override;
        void OnDestroy() override;
    };
}
