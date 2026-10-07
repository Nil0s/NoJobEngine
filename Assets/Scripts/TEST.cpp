#include "TEST.h"
#include "Engine/Scene/ScriptRegistry.h"

namespace NoJob
{
    void TEST::OnCreate()
    {
    }

    void TEST::OnUpdate(float deltaTime)
    {
        (void)deltaTime;
    }

    void TEST::OnDestroy()
    {
    }

}

NOJOB_REGISTER_SCRIPT(TEST, "Gameplay")
