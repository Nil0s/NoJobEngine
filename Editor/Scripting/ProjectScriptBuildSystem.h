/**
 * @file ProjectScriptBuildSystem.h
 * @brief Project-native script build orchestration and reload notifications.
 * @ingroup Scripting
 */
#pragma once
#include <filesystem>
#include <functional>
#include <string>

namespace NoJob
{
    /** @brief Manages asynchronous compilation and reload signaling for project scripts. */
    class ProjectScriptBuildSystem
    {
    public:
        using LogFunction = std::function<void(std::string)>;

        static void SetLogger(LogFunction logger);
        /** @brief Starts compilation for the specified project root.
         * @return True when the build was successfully started. */
        static bool Start(const std::filesystem::path& projectRoot);
        static bool IsRunning();
        /** @brief Consumes the pending reload notification after compilation. */
        static bool ConsumeReloadPending();
        static void Shutdown();
    };
}
