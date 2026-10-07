#pragma once
#include "Engine/Scene/ScriptableEntity.h"

namespace NoJob
{
    class TEST final : public Script
    {
    public:
        void OnCreate() override;
        void OnUpdate(float deltaTime) override;
        void OnDestroy() override;
    };
}
