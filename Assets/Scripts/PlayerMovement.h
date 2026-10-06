#pragma once

#include "Engine/Scene/ScriptableEntity.h"

namespace NoJob
{
    class PlayerMovement final : public Script
    {
    public:
        float Speed = 5.0f;
        float JumpForce = 8.0f;
        bool CanJump = true;
        glm::vec3 Direction{ 1.0f, 0.0f, 0.0f };

        void OnCreate() override;
        void OnUpdate(float deltaTime) override;
        void OnDestroy() override;
    };
}