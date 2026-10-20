/**
 * @file BTTrace.h
 * @brief BTTrace declarations for the Behavior Trees subsystem.
 * @ingroup BehaviorTree
 */
#pragma once
#include "BTContext.h"
#include "BTNode.h"
#include <unordered_map>
namespace NoJob {
// A snapshot of the last tick of each node, owned by the agent.
struct BTNodeTrace { BTStatus Status = BTStatus::Failure; std::size_t TickCount = 0; };
}
