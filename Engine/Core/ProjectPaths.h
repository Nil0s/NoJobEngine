/** @file ProjectPaths.h @brief Resolve source projects and relocated portable Alpha folders. */
#pragma once
#include <filesystem>
namespace NoJob::Paths {
/** @brief Recognize a project by its assets and project descriptor, without source directories. */
inline bool IsProject(const std::filesystem::path& directory) {
    std::error_code ec;
    if(!std::filesystem::is_directory(directory/"Assets",ec))return false;
    for(std::filesystem::directory_iterator it(directory,std::filesystem::directory_options::skip_permission_denied,ec),end;
        it!=end&&!ec;it.increment(ec))
        if(it->is_regular_file(ec)&&it->path().extension()==".nojobproject")return true;
    return false;
}
/** @brief Identify the portable distribution independently of the calling working directory. */
inline bool IsPortable(const std::filesystem::path& directory) {
    std::error_code ec;
    return (std::filesystem::is_regular_file(directory/"NJE.alpha",ec)||
            std::filesystem::is_regular_file(directory/"NoJobEngine.alpha",ec))&&IsProject(directory);
}
/** @brief Find the nearest project ancestor; return empty when no project exists. */
inline std::filesystem::path WalkUp(std::filesystem::path directory) {
    for(;!directory.empty();directory=directory.parent_path()) {
        if(IsProject(directory))return directory.lexically_normal();
        if(directory==directory.parent_path())break;
    }
    return {};
}
/** @brief Resolve with explicit origins so portable precedence can be regression-tested. */
inline std::filesystem::path FindProjectRootFrom(const std::filesystem::path& working,
                                               const std::filesystem::path& executableDirectory) {
    const auto besideExecutable=WalkUp(executableDirectory);
    if(!besideExecutable.empty()&&IsPortable(besideExecutable))return besideExecutable;
    if(const auto fromWorking=WalkUp(working);!fromWorking.empty())return fromWorking;
    return besideExecutable;
}
/** @brief Return the current executable's directory on Windows. */
std::filesystem::path ExecutableDirectory();
/** @brief Detect an installer-managed application beside the current executable. */
bool IsInstalledApplication();
/** @brief Return per-user application data; NJE_USER_HOME is an explicit test override. */
std::filesystem::path UserDataDirectory();
/** @brief Copy missing sample files without replacing any existing user content. */
std::filesystem::path EnsureWorkspaceFrom(const std::filesystem::path& application,const std::filesystem::path& userData);
/** @brief Prepare the installed user's workspace or return the portable/source project. */
std::filesystem::path InitializeProjectWorkspace();
/** @brief Keep installed preferences outside the application and its templates. */
std::filesystem::path SettingsDirectory();
/** @brief Check running installed trainer/runtime instances before an update. */
bool OtherInstalledAppsRunning();
/** @brief Locate a project even after moving the entire Alpha package. */
inline std::filesystem::path FindProjectRoot() {
    if(IsInstalledApplication())return UserDataDirectory()/"Projects"/"Default";
    std::error_code ec;const auto cwd=std::filesystem::current_path(ec);
    return FindProjectRootFrom(ec?std::filesystem::path{}:cwd,ExecutableDirectory());
}
/** @brief Source-only operations require the engine/editor checkout and build files. */
inline bool HasSourceTools(const std::filesystem::path& root) {
    std::error_code ec;
    return std::filesystem::is_regular_file(root/"CMakeLists.txt",ec)&&
        std::filesystem::is_directory(root/"Engine",ec)&&std::filesystem::is_directory(root/"Editor",ec);
}
}
