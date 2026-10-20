#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace NoJob
{
    class VisualStudioIntegration
    {
    public:
        using LogFunction = std::function<void(std::string)>;

        static bool OpenScript(
            const std::filesystem::path& scriptPath,
            const std::filesystem::path& projectRoot,
            const LogFunction& log);

    private:
        static std::filesystem::path FindVisualStudioExecutable();
    };
}
