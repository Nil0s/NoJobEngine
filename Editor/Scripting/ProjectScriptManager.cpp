#include "Editor/Scripting/ProjectScriptManager.h"
#include "Engine/Asset/AssetManager.h"
#include "Engine/Scene/ScriptRegistry.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <system_error>
#include <utility>

namespace NoJob
{
    namespace
    {
#ifdef _WIN32
        HMODULE s_Module = nullptr;
#endif
        std::vector<std::string> s_Names;
        ProjectScriptManager::LogFunction s_Log;

        void Log(std::string message)
        {
            if (s_Log) s_Log(std::move(message));
        }
    }

    void ProjectScriptManager::SetLogger(LogFunction logger)
    {
        s_Log = std::move(logger);
    }

    void ProjectScriptManager::RegisterScript(ScriptDefinition definition)
    {
        s_Names.push_back(definition.Name);
        ScriptRegistry::Register(std::move(definition));
    }

    void ProjectScriptManager::Unload()
    {
        for (const auto& name : s_Names)
            ScriptRegistry::Unregister(name);
        s_Names.clear();

#ifdef _WIN32
        if (s_Module)
        {
            FreeLibrary(s_Module);
            s_Module = nullptr;
        }
#endif
    }

    bool ProjectScriptManager::Load(const std::filesystem::path& projectRoot)
    {
        Unload();
#ifndef _WIN32
        (void)projectRoot;
        Log("[Scripts] Project script DLL loading is currently implemented for Windows.");
        return false;
#else
        std::filesystem::path dll;
        const auto outputDirectory = projectRoot / "out" / "ProjectScripts";
        std::error_code ec;
        std::filesystem::file_time_type newestTime{};

        if (std::filesystem::exists(outputDirectory, ec))
        {
            for (auto it = std::filesystem::recursive_directory_iterator(
                     outputDirectory,
                     std::filesystem::directory_options::skip_permission_denied, ec);
                 !ec && it != std::filesystem::recursive_directory_iterator(); ++it)
            {
                if (!it->is_regular_file(ec)) continue;
                const auto candidate = it->path();
                if (candidate.extension() != ".dll") continue;
                const std::string stem = candidate.stem().string();
                if (stem.rfind("NoJobProjectScripts_", 0) != 0) continue;

                const auto writeTime = std::filesystem::last_write_time(candidate, ec);
                if (ec) { ec.clear(); continue; }
                if (dll.empty() || writeTime > newestTime)
                {
                    dll = candidate;
                    newestTime = writeTime;
                }
            }
        }

        if (dll.empty())
        {
            Log("[Scripts] Versioned ProjectScripts DLL not found under: " +
                outputDirectory.string());
            return false;
        }

        Log("[Scripts] Loading versioned DLL: " + dll.string());
        s_Module = LoadLibraryW(dll.wstring().c_str());
        if (!s_Module)
        {
            Log("[Scripts] LoadLibrary failed (Win32 error " +
                std::to_string(GetLastError()) + ").");
            return false;
        }

        using RegisterProjectScriptFn = void(*)(void(*)(ScriptDefinition));
        const auto scripts = AssetManager::GetAssetsDirectory() / "Scripts";
        if (std::filesystem::exists(scripts, ec))
        {
            for (const auto& entry : std::filesystem::directory_iterator(scripts, ec))
            {
                if (entry.path().extension() != ".cpp") continue;
                const auto symbol = "NoJobRegister_" + entry.path().stem().string();
                auto fn = reinterpret_cast<RegisterProjectScriptFn>(
                    GetProcAddress(s_Module, symbol.c_str()));
                if (fn) fn(&ProjectScriptManager::RegisterScript);
            }
        }

        Log("[Scripts] Loaded " + std::to_string(s_Names.size()) + " project script(s).");
        return true;
#endif
    }

    bool ProjectScriptManager::IsLoaded()
    {
#ifdef _WIN32
        return s_Module != nullptr;
#else
        return false;
#endif
    }

    std::size_t ProjectScriptManager::LoadedScriptCount()
    {
        return s_Names.size();
    }
}
