#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NoJob
{
    class ProjectScriptManager
    {
    public:
        using LogFunction = std::function<void(std::string)>;

        static void SetLogger(LogFunction logger);
        static bool Load(const std::filesystem::path& projectRoot);
        static void Unload();
        static bool IsLoaded();
        static std::size_t LoadedScriptCount();

    private:
        static void RegisterScript(struct ScriptDefinition definition);
    };
}
