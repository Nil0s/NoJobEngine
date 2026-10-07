#include "Editor/Scripting/VisualStudioIntegration.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif

#include <cstdio>
#include <cstdlib>

namespace NoJob
{
    std::filesystem::path VisualStudioIntegration::FindVisualStudioExecutable()
    {
#ifdef _WIN32
        wchar_t* programFilesX86 = nullptr;
        std::size_t length = 0;
        if (_wdupenv_s(&programFilesX86, &length, L"ProgramFiles(x86)") != 0 ||
            !programFilesX86)
            return {};

        const std::filesystem::path vswhere =
            std::filesystem::path(programFilesX86) /
            "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
        free(programFilesX86);

        if (!std::filesystem::exists(vswhere))
            return {};

        const std::wstring command =
            L"\"" + vswhere.wstring() +
            L"\" -latest -products * -requires Microsoft.Component.MSBuild "
            L"-property productPath";

        FILE* pipe = _wpopen(command.c_str(), L"rt");
        if (!pipe)
            return {};

        wchar_t buffer[2048]{};
        std::wstring result;
        if (fgetws(buffer, static_cast<int>(std::size(buffer)), pipe))
            result = buffer;
        _pclose(pipe);

        while (!result.empty() &&
               (result.back() == L'\r' || result.back() == L'\n' ||
                result.back() == L' ' || result.back() == L'\t'))
            result.pop_back();

        const std::filesystem::path devenv(result);
        return std::filesystem::exists(devenv) ? devenv : std::filesystem::path{};
#else
        return {};
#endif
    }

    bool VisualStudioIntegration::OpenScript(
        const std::filesystem::path& scriptPath,
        const std::filesystem::path& projectRoot,
        const LogFunction& log)
    {
        const auto absoluteScript =
            std::filesystem::absolute(scriptPath).lexically_normal();

        if (!std::filesystem::exists(absoluteScript))
        {
            log("[Scripts] File not found: " + absoluteScript.string());
            return false;
        }

#ifdef _WIN32
        const auto visualStudio = FindVisualStudioExecutable();
        if (!visualStudio.empty())
        {
            const auto parameters =
                L"/Edit \"" + absoluteScript.wstring() + L"\"";
            const auto result = reinterpret_cast<std::intptr_t>(
                ShellExecuteW(
                    nullptr, L"open", visualStudio.wstring().c_str(),
                    parameters.c_str(), projectRoot.wstring().c_str(),
                    SW_SHOWNORMAL));

            if (result > 32)
            {
                log("[Scripts] Opened in existing Visual Studio instance: " +
                    absoluteScript.filename().string());
                return true;
            }
        }

        const auto result = reinterpret_cast<std::intptr_t>(
            ShellExecuteW(
                nullptr, L"open", absoluteScript.wstring().c_str(),
                nullptr, projectRoot.wstring().c_str(), SW_SHOWNORMAL));

        if (result > 32)
        {
            log("[Scripts] Opened with Windows file association: " +
                absoluteScript.filename().string());
            return true;
        }
#endif

        log("[Scripts] Could not open " + absoluteScript.string());
        return false;
    }
}
