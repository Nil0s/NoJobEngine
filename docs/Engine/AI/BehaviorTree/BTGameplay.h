#pragma once
#include "BTAsset.h"
#include "BTAssetPaths.h"
#include "BTScriptBridge.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/AI/Navigation/NavigationSystem.h"
#include "Engine/AI/Neural/NeuralAgentTraining.h"
#include <optional>
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace NoJob {
inline std::shared_ptr<BTNativeRegistry> CreateBTGameplayRegistry() {
    auto r = std::make_shared<BTNativeRegistry>();
    r->RegisterCondition("CanSeeTarget", [](BTContext& ctx) {
        auto* s=BTGetScript(ctx);
        if (!s || !s->GetEntity().GetScene()) return false;
        const auto targets=s->GetEntity().GetScene()->GetPerceivedTargets(s->GetEntity().GetHandle());
        for (const auto& target : targets) if (target.IsVisible) {
            ctx.Blackboard().Set("TargetHandle", target.EntityHandle);
            ctx.Blackboard().Set("TargetPosition", target.LastKnownPosition);
            return true;
        }
        return false;
    });
    r->RegisterCondition("HasRememberedTarget", [](BTContext& ctx) {
        auto* s=BTGetScript(ctx);
        if (!s || !s->GetEntity().GetScene()) return false;
        auto last=s->GetEntity().GetScene()->GetLastSeenTarget(s->GetEntity().GetHandle());
        if (!last) return false;
        ctx.Blackboard().Set("TargetPosition",last->Position);
        return true;
    });
    r->RegisterAction("ChaseTarget", [](BTContext& ctx, float) {
        auto* script = BTGetScript(ctx);
        if (!script || !script->HasComponent<NavAgentComponent>() ||
            !script->HasComponent<TransformComponent>()) return BTStatus::Failure;
        const auto entity = script->GetEntity();
        auto* scene = entity.GetScene();
        if (!scene) return BTStatus::Failure;
        auto& nav = script->GetComponent<NavAgentComponent>();
        if (!nav.Enabled) return BTStatus::Failure;

        // A running Sequence does not re-evaluate CanSeeTarget. Refresh the
        // moving target here, otherwise the Blackboard position goes stale.
        glm::vec3 destination{};
        bool hasDestination = false;
        if (const auto* handle = ctx.Blackboard().TryGet<std::uint32_t>("TargetHandle")) {
            if (scene->IsValid(*handle)) {
                for (const auto& perceived : scene->GetPerceivedTargets(entity.GetHandle())) {
                    if (perceived.EntityHandle == *handle && perceived.IsVisible) {
                        destination = perceived.LastKnownPosition;
                        ctx.Blackboard().Set("TargetPosition", destination);
                        hasDestination = true;
                        break;
                    }
                }
            }
        }
        // Continue to the last known position when the target is not visible.
        if (!hasDestination) {
            if (const auto* last = ctx.Blackboard().TryGet<glm::vec3>("TargetPosition")) {
                destination = *last;
                hasDestination = true;
            }
        }
        if (!hasDestination) return BTStatus::Failure;

        const glm::vec3 current = glm::vec3(scene->GetWorldTransform(entity)[3]);
        glm::vec3 delta = destination - current;
        delta.y = 0.0f;
        const float stoppingDistance = std::max(nav.StoppingDistance, 0.05f);
        if (glm::dot(delta, delta) <= stoppingDistance * stoppingDistance) {
            nav.HasDestination = false;
            return BTStatus::Success;
        }

        // Scene's navigation update detects changed destinations and repaths.
        // Re-arm navigation even if it finished the previous destination.
        nav.Destination = destination;
        nav.HasDestination = true;
        ctx.SetCancellation(ctx.ActiveNode(), [](BTContext& c) {
            if (auto* owner = BTGetScript(c); owner && owner->HasComponent<NavAgentComponent>())
                owner->GetComponent<NavAgentComponent>().HasDestination = false;
        });
        return BTStatus::Running;
    });
    // Native patrol task: four waypoints around the agent's spawn point.
    // Runtime progress is stored per agent in the Blackboard.
    r->RegisterAction("Patrol", [](BTContext& ctx, float dt) {
        auto* script = BTGetScript(ctx);
        if (!script || !script->HasComponent<NavAgentComponent>() ||
            !script->HasComponent<TransformComponent>()) return BTStatus::Failure;
        auto* scene = script->GetEntity().GetScene();
        if (!scene) return BTStatus::Failure;
        auto& nav = script->GetComponent<NavAgentComponent>();
        if (!nav.Enabled) return BTStatus::Failure;
        const glm::vec3 position = glm::vec3(scene->GetWorldTransform(script->GetEntity())[3]);
        auto& bb = ctx.Blackboard();
        if (!bb.Contains("PatrolOrigin")) bb.Set("PatrolOrigin", position);
        const glm::vec3 origin = bb.Get<glm::vec3>("PatrolOrigin");
        const float radius = std::max(bb.GetOr<float>("PatrolRadius", 3.f), 0.25f);
        const float pause = std::max(bb.GetOr<float>("PatrolWaitSeconds", 0.4f), 0.f);
        int index = bb.GetOr<int>("PatrolIndex", 0) % 4;
        if (index < 0) index += 4;
        const glm::vec3 offsets[4] = {
            {radius,0.f,0.f}, {radius,0.f,radius}, {0.f,0.f,radius}, {0.f,0.f,0.f}
        };
        const glm::vec3 destination = origin + offsets[index];
        glm::vec3 difference = destination - position;
        difference.y = 0.f;
        const float stoppingDistance = std::max(nav.StoppingDistance, 0.15f);
        if (glm::dot(difference,difference) <= stoppingDistance*stoppingDistance) {
            nav.HasDestination = false;
            const float elapsed = bb.GetOr<float>("PatrolWaitElapsed", 0.f) + std::max(dt,0.f);
            if (elapsed >= pause) {
                bb.Set("PatrolIndex", (index+1)%4);
                bb.Set("PatrolWaitElapsed", 0.f);
            } else bb.Set("PatrolWaitElapsed", elapsed);
        } else {
            bb.Set("PatrolWaitElapsed", 0.f);
            nav.Destination = destination;
            nav.HasDestination = true;
        }
        ctx.SetCancellation(ctx.ActiveNode(), [](BTContext& c) {
            if (auto* owner = BTGetScript(c); owner && owner->HasComponent<NavAgentComponent>())
                owner->GetComponent<NavAgentComponent>().HasDestination = false;
        });
        return BTStatus::Running;
    });
    r->RegisterAction("StopMoving", [](BTContext& ctx,float) {
        auto* s=BTGetScript(ctx);
        if (!s || !s->HasComponent<NavAgentComponent>()) return BTStatus::Failure;
        s->GetComponent<NavAgentComponent>().HasDestination=false;
        return BTStatus::Success;
    });
    r->RegisterAction("MoveForward", [](BTContext& ctx,float dt) {
        auto* s=BTGetScript(ctx);
        if (!s || !s->HasComponent<TransformComponent>()) return BTStatus::Failure;
        s->GetComponent<TransformComponent>().Position.x += dt;
        return BTStatus::Running;
    });
    return r;
}
// Shared immutable tree asset; each script instance owns its own BT runtime and Blackboard.
class BTAssetAgent final : public BTScriptController {
public:
    std::string BehaviorTreeAsset = "Default";
    float PatrolRadius = 3.0f;
    float PatrolWaitSeconds = 0.4f;
    std::string NeuralModelName = "";
    std::string NeuralTargetTag = "NeuralTarget";
protected:
    std::shared_ptr<const BTTree> CreateTree() override {
        auto registry = CreateBTGameplayRegistry();
        // V2.0.6: The BT owns navigation. Do not attach NeuralTrainingAgent to
        // the same entity: two scripts writing NavAgent would compete.
        registry->RegisterCondition("Neural.HasTarget", [this](BTContext& ctx) {
            auto* scene = GetEntity().GetScene();
            if (!scene) return false;
            for (Entity target : scene->GetEntities()) {
                if (target == GetEntity() || !target.HasComponent<TagComponent>() ||
                    !target.HasComponent<TransformComponent>() ||
                    target.GetComponent<TagComponent>().Tag != NeuralTargetTag) continue;
                ctx.Blackboard().Set("NeuralTargetPosition", target.GetComponent<TransformComponent>().Position);
                return true;
            }
            return false;
        });
        registry->RegisterAction("Neural.FollowTarget", [this](BTContext& ctx, float) {
            if (!HasComponent<NavAgentComponent>() || !HasComponent<TransformComponent>()) return BTStatus::Failure;
            auto& nav = GetComponent<NavAgentComponent>();
            if (!nav.Enabled) return BTStatus::Failure;
            if (!m_NeuralModel) {
                if (NeuralModelName.empty()) return BTStatus::Failure;
                try {
                    m_NeuralModel.emplace(Neural::NeuralNetwork::Load(NeuralAssetPath(NeuralModelName).string()));
                    if (m_NeuralModel->Layers().front().Inputs() != 2 ||
                        m_NeuralModel->Layers().back().Outputs() != 2)
                        throw std::runtime_error("Neural.FollowTarget requires a 2->2 model");
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "[Neural BT] %s\n", e.what());
                    m_NeuralModel.reset();
                    return BTStatus::Failure;
                }
            }
            auto* scene = GetEntity().GetScene();
            if (!scene) return BTStatus::Failure;
            glm::vec3 destination{};
            bool found = false;
            for (Entity target : scene->GetEntities()) {
                if (target == GetEntity() || !target.HasComponent<TagComponent>() ||
                    !target.HasComponent<TransformComponent>() ||
                    target.GetComponent<TagComponent>().Tag != NeuralTargetTag) continue;
                destination = target.GetComponent<TransformComponent>().Position;
                found = true;
                break;
            }
            if (!found) { nav.HasDestination = false; NeuralAgent::Clear(GetEntity().GetHandle()); return BTStatus::Failure; }
            const glm::vec3 pos = GetComponent<TransformComponent>().Position;
            const glm::vec3 delta = destination - pos;
            const float distance = std::sqrt(delta.x*delta.x + delta.z*delta.z);
            ctx.Blackboard().Set("NeuralTargetPosition", destination);
            ctx.Blackboard().Set("NeuralTargetDistance", distance);
            if (distance <= std::max(0.1f, nav.StoppingDistance)) {
                nav.HasDestination = false;
                return BTStatus::Success;
            }
            const std::vector<float> inputs{std::clamp(delta.x / 10.f, -1.f, 1.f),
                                            std::clamp(delta.z / 10.f, -1.f, 1.f)};
            const auto outputs = m_NeuralModel->Predict(inputs);
            if (outputs.size() != 2 || !std::isfinite(outputs[0]) || !std::isfinite(outputs[1]))
                return BTStatus::Failure;
            const glm::vec3 direction(outputs[0], 0.f, outputs[1]);
            const float magnitude = std::sqrt(direction.x*direction.x + direction.z*direction.z);
            if (magnitude < 0.05f) return BTStatus::Failure;
            ctx.Blackboard().Set("NeuralInputX", inputs[0]);
            ctx.Blackboard().Set("NeuralInputZ", inputs[1]);
            ctx.Blackboard().Set("NeuralOutputX", outputs[0]);
            ctx.Blackboard().Set("NeuralOutputZ", outputs[1]);
            NeuralAgent::Snapshot snap;
            snap.model = NeuralModelName; snap.status = "BT Neural.FollowTarget";
            snap.inputs = inputs; snap.outputs = outputs; snap.distance = distance;
            snap.activations = NeuralAgent::Activations(*m_NeuralModel, inputs);
            NeuralAgent::Publish(GetEntity().GetHandle(), std::move(snap));
            nav.Destination = pos + direction / magnitude * std::min(distance, 3.f);
            nav.HasDestination = true;
            ctx.SetCancellation(ctx.ActiveNode(), [](BTContext& c) {
                if (auto* owner = BTGetScript(c); owner && owner->HasComponent<NavAgentComponent>())
                    owner->GetComponent<NavAgentComponent>().HasDestination = false;
            });
            return BTStatus::Running;
        });
        // Bind callbacks from sibling Native Scripts before constructing the tree.
        // Scene creates all script instances before calling OnCreate().
        std::fprintf(stderr, "[BT TRACE] BTAssetAgent::CreateTree entity=%u before scene bindings\n", GetEntity().GetHandle());
        std::fflush(stderr);
        if (auto* scene = GetEntity().GetScene()) {
            std::fprintf(stderr, "[BT TRACE] Scene pointer=%p; calling RegisterEntityBehaviorTreeBindings\n", static_cast<void*>(scene));
            std::fflush(stderr);
            scene->RegisterEntityBehaviorTreeBindings(GetEntity().GetHandle(), *registry);
        } else {
            std::fprintf(stderr, "[BT TRACE] ERROR: GetScene() returned nullptr\n");
        }
        std::fprintf(stderr, "[BT TRACE] BTAssetAgent::CreateTree after bindings: Guard.Attack=%d Guard.HasTarget=%d\n",
            registry->HasAction("Guard.Attack") ? 1 : 0, registry->HasCondition("Guard.HasTarget") ? 1 : 0);
        std::fflush(stderr);
        std::fprintf(stderr, "[BT DIAG] BTAssetAgent entity %u tree '%s': Guard.Attack registered=%d\n",
            GetEntity().GetHandle(), BehaviorTreeAsset.c_str(),
            registry->HasAction("Guard.Attack") ? 1 : 0);
        const auto btAsset = BTAsset::Load(BTAssetPath(BehaviorTreeAsset));
        // Fail with actionable diagnostics rather than an opaque missing callback.
        for (const auto& node : btAsset.Nodes) {
            if (node.Kind == "Action" && !registry->HasAction(node.Native))
                throw std::runtime_error("BT asset '" + BehaviorTreeAsset + "': action '" + node.Native +
                    "' is not registered for this entity. Check that its Native Script "
                    "(e.g. GuardCombatBT) is attached, enabled, and loaded in the project DLL.");
            if (node.Kind == "Condition" && !registry->HasCondition(node.Native))
                throw std::runtime_error("BT asset '" + BehaviorTreeAsset + "': condition '" + node.Native +
                    "' is not registered for this entity. Check Native Scripts and project DLL.");
        }
        return btAsset.Build(std::move(registry));
    }
    void OnTreeStarted(BTContext& ctx) override {
        m_NeuralModel.reset();
        NeuralAgent::Clear(GetEntity().GetHandle());
        ctx.Blackboard().Set("PatrolRadius", PatrolRadius);
        ctx.Blackboard().Set("PatrolWaitSeconds", PatrolWaitSeconds);
    }
    void OnTreeStopped(BTContext&) override { m_NeuralModel.reset(); NeuralAgent::Clear(GetEntity().GetHandle()); }
private:
    std::optional<Neural::NeuralNetwork> m_NeuralModel;
};
inline void RegisterBTAssetAgent() {
    ScriptRegistry::Register({"BTAssetAgent", "AI",
        {MakeMemberFieldDefinition<BTAssetAgent, std::string>("BehaviorTreeAsset", &BTAssetAgent::BehaviorTreeAsset, std::string("Default")),
         MakeMemberFieldDefinition<BTAssetAgent, float>("PatrolRadius", &BTAssetAgent::PatrolRadius, 3.0f),
         MakeMemberFieldDefinition<BTAssetAgent, float>("PatrolWaitSeconds", &BTAssetAgent::PatrolWaitSeconds, 0.4f),
         MakeMemberFieldDefinition<BTAssetAgent, std::string>("NeuralModelName", &BTAssetAgent::NeuralModelName, std::string("")),
         MakeMemberFieldDefinition<BTAssetAgent, std::string>("NeuralTargetTag", &BTAssetAgent::NeuralTargetTag, std::string("NeuralTarget"))},
        [] { return std::make_unique<BTAssetAgent>(); }});
}
} // namespace NoJob
