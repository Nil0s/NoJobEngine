#include "GuardAI.h"
#include "Engine/Scene/ScriptRegistry.h"

namespace NoJob
{
    void GuardAI::OnCreate()
    {
    }

    void GuardAI::OnUpdate(float deltaTime)
    {
        (void)deltaTime;

        if (!HasComponent<NavAgentComponent>() ||
            !HasComponent<PerceptionComponent>())
            return;

        Scene* scene = GetEntity().GetScene();
        if (!scene)
            return;

        auto& agent = GetComponent<NavAgentComponent>();

        // First test: follow the first visible target.
        const auto targets =
            scene->GetPerceivedTargets(GetEntity().GetHandle());

        for (const auto& target : targets)
        {
            if (!target.IsVisible)
                continue;

            agent.Enabled = true;
            agent.StoppingDistance = ChaseStoppingDistance;
            agent.Destination = target.LastKnownPosition;
            agent.HasDestination = true;

            return;
        }

        // No visible target: stop requesting movement.
        agent.HasDestination = false;
    }

    void GuardAI::OnDestroy()
    {
    }

    NOJOB_REGISTER_SCRIPT(GuardAI, "AI",
        NOJOB_FIELD(GuardAI, ChaseStoppingDistance))
}