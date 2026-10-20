#pragma once
#include <filesystem>
#include <functional>
#include <string>

namespace NoJob
{
    class ProjectScriptBuildSystem
    {
    public:
        using LogFunction = std::function<void(std::string)>;

        static void SetLogger(LogFunction logger);
        static bool Start(const std::filesystem::path& projectRoot);
        static bool IsRunning();
        static bool ConsumeReloadPending();
        static void Shutdown();
    };
}
