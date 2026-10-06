#include "PlayerMovement.h"
#include "Engine/Scene/ScriptRegistry.h"

namespace NoJob
{
    void PlayerMovement::OnCreate()
    {
    }

    void PlayerMovement::OnUpdate(float deltaTime)
    {
        (void)deltaTime;
        auto& transform = GetComponent<TransformComponent>();
        transform.Position += Direction * Speed * deltaTime;
    }

    void PlayerMovement::OnDestroy()
    {
    }

    NOJOB_REGISTER_SCRIPT(PlayerMovement, "Gameplay",
        NOJOB_FIELD(PlayerMovement, Speed),
        NOJOB_FIELD(PlayerMovement, JumpForce),
        NOJOB_FIELD(PlayerMovement, CanJump),
        NOJOB_FIELD(PlayerMovement, Direction))
}