/** @file RLPolicyFiles.h @brief Discover exported and per-run evaluation policies. */
#pragma once
#include "Engine/AI/Neural/NeuralNetwork.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>
namespace NoJob {
/** @brief Infer the supported benchmark from model dimensions, independent of names and training UI. */
inline int RLPolicyEvaluationTask(const Neural::NeuralNetwork& model) {
    if(!model.Layers().empty()) {
        const auto inputs=model.Layers().front().Inputs();
        const auto outputs=model.Layers().back().Outputs();
        if(inputs==2 && outputs==3) return 0;
        if(inputs==8 && outputs==4) return 1;
    }
    throw std::runtime_error("Unsupported evaluation model: expected tracking (2 inputs / 3 actions) or obstacles (8 inputs / 4 actions).");
}

/** @brief List full policy paths; preserve run directories to distinguish identical filenames. */
inline std::vector<std::filesystem::path> ListRLPolicyFiles(const std::filesystem::path& project) {
    std::vector<std::filesystem::path> paths;
    for (const auto& folder : {project / "Assets" / "Neural", project / "TrainingRuns"}) {
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(folder,
            std::filesystem::directory_options::skip_permission_denied, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            auto ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".njn") paths.push_back(it->path().lexically_normal());
        }
    }
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    return paths;
}
}
