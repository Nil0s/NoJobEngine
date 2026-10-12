#include "../BTSelector.h"
#include "../BTNode.h"
struct MutableNode : NoJob::BTNode { NoJob::BTStatus status; MutableNode(NoJob::BTNodeID id, NoJob::BTStatus s):BTNode(id),status(s){} NoJob::BTStatus Tick(NoJob::BTContext&, float) const override {return status;} };
#include <cassert>
#include <memory>
using namespace NoJob;
int main() {
  BTContext ctx;
  BTSelector selector(1);
  auto high = std::make_unique<MutableNode>(2, BTStatus::Failure);
  auto* highPtr = high.get();
  selector.AddChild(std::move(high));
  selector.AddChild(std::make_unique<MutableNode>(3, BTStatus::Running));
  assert(selector.Tick(ctx, 0.016f) == BTStatus::Running);
  assert(ctx.GetNodeState(1).CurrentChild == 1);
  highPtr->status = BTStatus::Running;
  assert(selector.Tick(ctx, 0.016f) == BTStatus::Running);
  assert(ctx.GetNodeState(1).CurrentChild == 0);
}
