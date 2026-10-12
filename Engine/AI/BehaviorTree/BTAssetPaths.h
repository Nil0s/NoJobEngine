#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace NoJob {
namespace BTPathsDetail {
namespace fs = std::filesystem;

inline fs::path FindProjectRoot() {
    // The editor may run from out/build/x64-Debug. Never choose that build
    // folder merely because it contains an old Assets/AI/Default.njbt.
    std::error_code ec;
    fs::path base = fs::current_path(ec);
    if (ec) return {};
    for (;;) {
        if (fs::is_regular_file(base / "CMakeLists.txt", ec) &&
            fs::is_directory(base / "Engine", ec) &&
            fs::is_directory(base / "Editor", ec))
            return base;
        if (base == base.root_path() || base == base.parent_path()) break;
        base = base.parent_path();
    }
    return {};
}
inline bool IsTreeFile(const fs::directory_entry& entry) {
    std::error_code ec;
    if (!entry.is_regular_file(ec)) return false;
    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".njbt";
}
} // namespace BTPathsDetail

inline std::filesystem::path BTAssetDirectory() {
    namespace fs = std::filesystem;
    const auto root = BTPathsDetail::FindProjectRoot();
    if (!root.empty()) return root / "Assets" / "AI";

    // Fallback for standalone distributions without a CMake project root.
    std::error_code ec;
    const auto cwd = fs::current_path(ec);
    if (ec) return fs::path("Assets") / "AI";
    for (auto base = cwd; ; base = base.parent_path()) {
        const auto folder = base / "Assets" / "AI";
        if (fs::is_directory(folder, ec)) return folder;
        if (base == base.root_path() || base == base.parent_path()) break;
        base = base.parent_path();
    }
    return cwd / "Assets" / "AI";
}

inline bool IsValidBTAssetName(const std::string& name) {
    return !name.empty() && name != "." && name != ".." &&
           name.back() != '.' && name.back() != ' ' &&
           name.find_first_of("/\\:*?\"<>|") == std::string::npos;
}

inline std::vector<std::string> ListBTAssetNames() {
    namespace fs = std::filesystem;
    std::vector<std::string> names;
    const auto folder = BTAssetDirectory();
    std::error_code ec;
    if (!fs::is_directory(folder, ec)) return names;
    for (fs::directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (BTPathsDetail::IsTreeFile(*it))
            names.push_back(it->path().stem().string());
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

inline std::filesystem::path BTAssetPath(const std::string& name) {
    if (!IsValidBTAssetName(name))
        throw std::invalid_argument("Invalid behavior tree asset name");
    return BTAssetDirectory() / (name + ".njbt");
}
} // namespace NoJob
