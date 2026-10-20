#pragma once
#include "Engine/AI/BehaviorTree/BTAssetPaths.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace NoJob {
inline std::filesystem::path NeuralAssetDirectory() {
    // Use the same project-root resolution as Behavior Trees.
    const auto root = BTPathsDetail::FindProjectRoot();
    if (!root.empty()) return root / "Assets" / "Neural";
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (ec) return std::filesystem::path("Assets") / "Neural";
    for (auto base = cwd;; base = base.parent_path()) {
        const auto folder = base / "Assets" / "Neural";
        if (std::filesystem::is_directory(folder, ec)) return folder;
        if (base == base.root_path() || base == base.parent_path()) break;
    }
    return cwd / "Assets" / "Neural";
}
inline bool IsValidNeuralAssetName(const std::string& name) {
    return IsValidBTAssetName(name);
}
inline std::filesystem::path NeuralAssetPath(const std::string& name) {
    if (!IsValidNeuralAssetName(name))
        throw std::invalid_argument("Invalid neural network name");
    return NeuralAssetDirectory() / (name + ".njn");
}
inline std::vector<std::string> ListNeuralAssetNames() {
    std::vector<std::string> names;
    const auto folder = NeuralAssetDirectory();
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) return names;
    for (std::filesystem::directory_iterator it(folder, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".njn") names.push_back(it->path().stem().string());
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}
} // namespace NoJob
