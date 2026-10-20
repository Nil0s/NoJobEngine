#pragma once

#include "Engine/Scene/Entity.h"
#include <filesystem>
#include <functional>
#include <string>

namespace NoJob
{
    class ProjectAssetOperations
    {
    public:
        using LogFunction = std::function<void(std::string)>;

        static std::string SanitizeCppIdentifier(std::string name);
        static bool CreateCppScript(const std::filesystem::path& scriptsDirectory,
                                    const std::string& requestedName,
                                    const LogFunction& log = {});
        static std::filesystem::path CreatePrefab(Entity source,
                                                  const std::filesystem::path& directory,
                                                  const LogFunction& log = {});
    };
}
