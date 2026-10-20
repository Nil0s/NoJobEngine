#pragma once
#include "BTActionNode.h"
#include "BTConditionNode.h"
#include "BTSequence.h"
#include "BTSelector.h"
#include "BTTree.h"
#include "BTDecorator.h"
#include "BTService.h"
#include "BTParallel.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_set>

namespace NoJob {
// V1.9 portable, versioned, line-oriented asset. Indentation is not significant.
// NODE <id> <parentId> <Sequence|Selector|Action|Condition> <nativeName-or-dash>
struct BTAssetNode {
    BTNodeID ID = 0, Parent = 0;
    std::string Kind = "Sequence", Native = "-";
    std::string Parameter = "-"; // Decorator observed key / Subtree asset path
    float Interval = 0.5f; // Service interval in seconds
};
class BTAsset {
public:
    std::vector<BTAssetNode> Nodes;
    static BTAsset Default() {
        return {{{1,0,"Selector","-"},{2,1,"Sequence","-"},
                 {3,2,"Condition","CanSeeTarget"},{4,2,"Action","ChaseTarget"},
                 {5,1,"Action","StopMoving"}}};
    }
    void Validate() const {
        if (Nodes.empty() || Nodes.size() > 4096) throw std::runtime_error("Invalid BT node count");
        std::unordered_set<BTNodeID> ids;
        std::size_t roots = 0;
        for (const auto& n : Nodes) {
            if (!n.ID || !ids.insert(n.ID).second) throw std::runtime_error("Duplicate/zero BT node ID");
            if (n.Parent == 0) ++roots;
            if (n.Kind != "Sequence" && n.Kind != "Selector" && n.Kind != "Action" && n.Kind != "Condition" &&
                n.Kind != "Decorator" && n.Kind != "Service" && n.Kind != "Parallel")
                throw std::runtime_error("Unknown BT node kind");
            if ((n.Kind == "Action" || n.Kind == "Condition" || n.Kind == "Decorator" || n.Kind == "Service") && (n.Native.empty() || n.Native == "-"))
                throw std::runtime_error("Missing native callback name");
        }
        if (roots != 1) throw std::runtime_error("BT requires exactly one root");
        for (const auto& n : Nodes) if (n.Parent && !ids.contains(n.Parent))
            throw std::runtime_error("BT parent not found");
        for (const auto& n : Nodes) {
            std::unordered_set<BTNodeID> visited;
            auto current = n.ID;
            while (current) {
                if (!visited.insert(current).second) throw std::runtime_error("BT cycle detected");
                const BTAssetNode* node = nullptr;
                for (const auto& candidate : Nodes) if (candidate.ID == current) {node=&candidate;break;}
                if (!node) throw std::runtime_error("BT missing ancestor");
                current = node->Parent;
            }
        }
        for (const auto& n : Nodes) {
            std::size_t count=0;
            for (const auto& child : Nodes) if (child.Parent==n.ID) ++count;
            if ((n.Kind=="Decorator" || n.Kind=="Service") && count!=1)
                throw std::runtime_error("Decorator/Service requires one child");
            if (n.Kind=="Parallel" && count!=2)
                throw std::runtime_error("Parallel requires exactly two children");
            if (n.Kind=="Service" && n.Interval<=0.f)
                throw std::runtime_error("Service interval must be positive");
        }
        for (const auto& n : Nodes) if (n.Parent) {
            for (const auto& parent : Nodes) if (parent.ID == n.Parent &&
                (parent.Kind == "Action" || parent.Kind == "Condition"))
                throw std::runtime_error("BT leaf cannot have children");
        }
    }
    void Save(const std::filesystem::path& path) const {
        Validate();
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot write BT asset: " + path.string());
        out << "NJOB_BT 2\n";
        for (const auto& n : Nodes) out << "NODE " << n.ID << ' ' << n.Parent << ' ' << n.Kind << ' ' << n.Native << ' ' << n.Parameter << ' ' << n.Interval << '\n';
        if (!out) throw std::runtime_error("BT asset write failed");
    }
    static BTAsset Load(const std::filesystem::path& path) {
        std::ifstream in(path);
        if (!in) throw std::runtime_error("Cannot read BT asset: " + path.string());
        std::string magic; int version = 0;
        if (!(in >> magic >> version) || magic != "NJOB_BT" || (version != 1 && version != 2))
            throw std::runtime_error("Unsupported BT asset version");
        BTAsset asset;
        std::string tag;
        while (in >> tag) {
            if (tag != "NODE") throw std::runtime_error("Unexpected BT asset token");
            BTAssetNode node;
            if (!(in >> node.ID >> node.Parent >> node.Kind >> node.Native)) throw std::runtime_error("Truncated BT asset");
            if (version == 2 && !(in >> node.Parameter >> node.Interval))
                throw std::runtime_error("Truncated BT v2 asset");
            asset.Nodes.push_back(std::move(node));
        }
        if (!in.eof()) throw std::runtime_error("Invalid BT asset syntax");
        asset.Validate();
        return asset;
    }
    std::shared_ptr<const BTTree> Build(std::shared_ptr<const BTNativeRegistry> registry) const {
        Validate();
        if (!registry) throw std::invalid_argument("BT native registry required");
        std::function<std::unique_ptr<BTNode>(BTNodeID)> make = [&](BTNodeID id) -> std::unique_ptr<BTNode> {
            const BTAssetNode* node = nullptr;
            for (const auto& n : Nodes) if (n.ID == id) { node = &n; break; }
            if (!node) throw std::runtime_error("BT node missing");
            if (node->Kind == "Decorator" || node->Kind == "Service") {
                BTNodeID child = 0;
                for (const auto& n : Nodes) if (n.Parent == id) { child = n.ID; break; }
                if (node->Kind == "Decorator")
                    return std::make_unique<BTDecorator>(id,registry,node->Native,
                        node->Parameter == "-" ? "" : node->Parameter, make(child));
                return std::make_unique<BTService>(id,registry,node->Native,node->Interval,make(child));
            }
            if (node->Kind == "Parallel") {
                std::vector<BTNodeID> children;
                for (const auto& n : Nodes) if (n.Parent == id) children.push_back(n.ID);
                return std::make_unique<BTParallel>(id,make(children[0]),make(children[1]));
            }
            if (node->Kind == "Action") return std::make_unique<BTActionNode>(id,registry,node->Native);
            if (node->Kind == "Condition") return std::make_unique<BTConditionNode>(id,registry,node->Native);
            if (node->Kind == "Sequence") {
                auto result = std::make_unique<BTSequence>(id);
                for (const auto& n : Nodes) if (n.Parent == id) result->AddChild(make(n.ID));
                return result;
            }
            auto result = std::make_unique<BTSelector>(id);
            for (const auto& n : Nodes) if (n.Parent == id) result->AddChild(make(n.ID));
            return result;
        };
        for (const auto& n : Nodes) if (!n.Parent) return std::make_shared<const BTTree>(make(n.ID));
        throw std::runtime_error("Missing BT root");
    }
};
} // namespace NoJob
