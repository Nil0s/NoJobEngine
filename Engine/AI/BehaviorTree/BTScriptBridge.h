#pragma once
#include "Engine/AI/BehaviorTree/BTInstance.h"
#include "Engine/AI/BehaviorTree/BTNativeRegistry.h"
#include "Engine/Scene/ScriptableEntity.h"
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <functional>
#include <glm/vec3.hpp>

namespace NoJob {
// Type-safe access to the owning script. The pointer is valid only during Play.
inline ScriptableEntity* BTGetScript(BTContext& context) noexcept {
    return context.GetScript();
}

// Bind by script NAME, never by instance pointer. The entity-local script is
// resolved on each tick, so trees remain reusable and reloads do not retain
// dangling script pointers. Missing/disabled scripts fail gracefully.
template<class T>
void BTRegisterScriptAction(BTNativeRegistry& registry, std::string actionName,
                            std::string scriptName, BTStatus (T::*method)(BTContext&, float)) {
    registry.RegisterAction(std::move(actionName), [name = std::move(scriptName), method](BTContext& ctx, float dt) {
        auto* owner = BTGetScript(ctx);
        auto* scene = owner ? owner->GetEntity().GetScene() : nullptr;
        auto* script = scene ? dynamic_cast<T*>(scene->FindRuntimeScript(owner->GetEntity().GetHandle(), name)) : nullptr;
        return script ? (script->*method)(ctx, dt) : BTStatus::Failure;
    });
}
template<class T>
void BTRegisterScriptCondition(BTNativeRegistry& registry, std::string conditionName,
                               std::string scriptName, bool (T::*method)(BTContext&)) {
    registry.RegisterCondition(std::move(conditionName), [name = std::move(scriptName), method](BTContext& ctx) {
        auto* owner = BTGetScript(ctx);
        auto* scene = owner ? owner->GetEntity().GetScene() : nullptr;
        auto* script = scene ? dynamic_cast<T*>(scene->FindRuntimeScript(owner->GetEntity().GetHandle(), name)) : nullptr;
        return script && (script->*method)(ctx);
    });
}

// Value-only snapshot. The editor never iterates BTContext's live containers.
struct BTDebugSnapshot {
    int LastResult = 1;
    struct Node { std::uint64_t ID; int Status; std::size_t Ticks; std::string Label; int Depth; bool Active; };
    struct Event { std::uint64_t Tick; std::uint64_t Node; std::string Kind; std::string Detail; };
    std::vector<Node> Nodes;
    std::vector<Event> Events;
    std::optional<std::uint32_t> TargetHandle;
    std::optional<glm::vec3> TargetPosition;
};
// Native script base class. Scene already invokes OnCreate/OnUpdate/OnDestroy.
// Derived classes build or receive a shared immutable BTTree in CreateTree().
class BTScriptController : public Script {
public:
    void OnCreate() final {
        m_Instance.reset();
        auto tree = CreateTree();
        if (!tree) throw std::runtime_error("BTScriptController: CreateTree returned null");
        m_Instance = std::make_unique<BTInstance>(std::move(tree));
        m_Instance->Context().BindScript(this);
        OnTreeStarted(m_Instance->Context());
        CaptureDebugSnapshot();
    }

    void OnUpdate(float deltaTime) final {
        if (m_Instance) {
            m_LastStatus = m_Instance->Tick(deltaTime);
            CaptureDebugSnapshot();
        }
    }

    void OnDestroy() final {
        if (!m_Instance) return;
        // Reset nodes while the script/entity is still valid.
        m_Instance->Reset();
        OnTreeStopped(m_Instance->Context());
        m_Instance->Context().UnbindScript();
        m_Instance.reset();
        m_DebugSnapshot = {};
    }

    // A copy of the last completed tick, safe to read without touching BTContext.
    const BTDebugSnapshot& GetDebugSnapshot() const noexcept { return m_DebugSnapshot; }

    BTInstance* GetBehaviorTreeInstance() noexcept { return m_Instance.get(); }
    const BTInstance* GetBehaviorTreeInstance() const noexcept { return m_Instance.get(); }
    BTStatus GetLastBehaviorTreeStatus() const noexcept { return m_LastStatus; }

protected:
    virtual std::shared_ptr<const BTTree> CreateTree() = 0;
    virtual void OnTreeStarted(BTContext&) {}
    virtual void OnTreeStopped(BTContext&) {}

private:
    void CaptureDebugSnapshot() {
        if (!m_Instance) return;
        BTDebugSnapshot next;
        next.LastResult = static_cast<int>(m_LastStatus);
        const auto& ctx = m_Instance->Context();
        std::function<void(const BTNode&, int)> visit = [&](const BTNode& node, int depth) {
            const auto status = ctx.LastStatuses().find(node.GetID());
            const auto ticks = ctx.TickCounters().find(node.GetID());
            next.Nodes.push_back({node.GetID(),
                status == ctx.LastStatuses().end() ? -1 : status->second,
                ticks == ctx.TickCounters().end() ? 0 : ticks->second,
                node.DebugLabel(), depth, ctx.CurrentStatuses().contains(node.GetID())});
            std::vector<const BTNode*> children;
            node.CollectChildren(children);
            for (const auto* child : children) if (child) visit(*child, depth + 1);
        };
        visit(m_Instance->Tree().Root(), 0);
        for (const auto& event : ctx.Events())
            next.Events.push_back({event.Tick, event.Node, event.Kind, event.Detail});
        if (const auto* target = ctx.Blackboard().TryGet<std::uint32_t>("TargetHandle"))
            next.TargetHandle = *target;
        if (const auto* position = ctx.Blackboard().TryGet<glm::vec3>("TargetPosition"))
            next.TargetPosition = *position;
        m_DebugSnapshot = std::move(next);
    }
    BTDebugSnapshot m_DebugSnapshot;
    std::unique_ptr<BTInstance> m_Instance;
    BTStatus m_LastStatus = BTStatus::Failure;
};
} // namespace NoJob
