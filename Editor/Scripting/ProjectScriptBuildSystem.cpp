#include "Editor/Scripting/ProjectScriptBuildSystem.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace NoJob
{
    namespace
    {
        ProjectScriptBuildSystem::LogFunction s_Log;
        std::mutex s_StateMutex;
        std::thread s_Worker;
        std::atomic_bool s_Running{ false };
        std::atomic_bool s_ReloadPending{ false };
        std::uint64_t s_Generation = 0;

        void Log(std::string message)
        {
            if (s_Log) s_Log(std::move(message));
        }

        std::filesystem::path FindCMakeExecutable()
        {
#ifdef _WIN32
            wchar_t pathBuffer[32768]{};
            const DWORD found = SearchPathW(nullptr, L"cmake.exe", nullptr,
                static_cast<DWORD>(std::size(pathBuffer)), pathBuffer, nullptr);
            if (found > 0 && found < std::size(pathBuffer)) return pathBuffer;

            wchar_t* programFilesX86 = nullptr;
            std::size_t envLength = 0;
            if (_wdupenv_s(&programFilesX86, &envLength, L"ProgramFiles(x86)") == 0 && programFilesX86)
            {
                const auto vswhere = std::filesystem::path(programFilesX86) /
                    "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
                free(programFilesX86);
                if (std::filesystem::exists(vswhere))
                {
                    const std::wstring command = L"\"" + vswhere.wstring() +
                        L"\" -latest -products * -property installationPath";
                    FILE* pipe = _wpopen(command.c_str(), L"rt");
                    if (pipe)
                    {
                        wchar_t buffer[4096]{};
                        std::wstring installPath;
                        if (fgetws(buffer, static_cast<int>(std::size(buffer)), pipe)) installPath = buffer;
                        _pclose(pipe);
                        while (!installPath.empty() &&
                               (installPath.back()==L'\r' || installPath.back()==L'\n' ||
                                installPath.back()==L' ' || installPath.back()==L'\t'))
                            installPath.pop_back();
                        if (!installPath.empty())
                        {
                            const auto bundled = std::filesystem::path(installPath) /
                                "Common7" / "IDE" / "CommonExtensions" / "Microsoft" /
                                "CMake" / "CMake" / "bin" / "cmake.exe";
                            if (std::filesystem::exists(bundled)) return bundled;
                        }
                    }
                }
            }

            wchar_t* programFiles = nullptr;
            envLength = 0;
            if (_wdupenv_s(&programFiles, &envLength, L"ProgramFiles") == 0 && programFiles)
            {
                const auto standalone = std::filesystem::path(programFiles) / "CMake" / "bin" / "cmake.exe";
                free(programFiles);
                if (std::filesystem::exists(standalone)) return standalone;
            }
#endif
            return {};
        }

        int RunProcess(const std::filesystem::path& executable,
                       const std::vector<std::wstring>& arguments,
                       const std::string& prefix)
        {
#ifdef _WIN32
            SECURITY_ATTRIBUTES security{};
            security.nLength = sizeof(SECURITY_ATTRIBUTES);
            security.bInheritHandle = TRUE;
            HANDLE readPipe = nullptr, writePipe = nullptr;
            if (!CreatePipe(&readPipe, &writePipe, &security, 0)) { Log("[Scripts] CreatePipe failed."); return -1; }
            SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

            std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
            for (const auto& arg : arguments)
            {
                commandLine += L" \"";
                for (wchar_t c : arg) { if (c == L'\"') commandLine += L'\\'; commandLine += c; }
                commandLine += L"\"";
            }
            std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
            mutableCommand.push_back(L'\0');

            STARTUPINFOW startup{}; startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdOutput = writePipe; startup.hStdError = writePipe;
            startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            PROCESS_INFORMATION process{};
            const BOOL created = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
            CloseHandle(writePipe);
            if (!created) { CloseHandle(readPipe); Log(prefix + "Failed to start process."); return -1; }

            char buffer[4096]; DWORD bytesRead = 0; std::string pending;
            while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
            {
                pending.append(buffer, buffer + bytesRead);
                std::size_t newline = 0;
                while ((newline = pending.find('\n')) != std::string::npos)
                {
                    std::string line = pending.substr(0, newline);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    Log(prefix + line); pending.erase(0, newline + 1);
                }
            }
            if (!pending.empty()) Log(prefix + pending);
            WaitForSingleObject(process.hProcess, INFINITE);
            DWORD exitCode = 1; GetExitCodeProcess(process.hProcess, &exitCode);
            CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(readPipe);
            return static_cast<int>(exitCode);
#else
            (void)executable; (void)arguments; (void)prefix; return -1;
#endif
        }
    }

    void ProjectScriptBuildSystem::SetLogger(LogFunction logger) { s_Log = std::move(logger); }

    bool ProjectScriptBuildSystem::Start(const std::filesystem::path& projectRoot)
    {
        if (s_Running.exchange(true)) { Log("[Scripts] Compilation is already running."); return false; }
        const auto cmake = FindCMakeExecutable();
        if (cmake.empty())
        {
            Log("[Scripts] CMake was not found.");
            Log("[Scripts] Install CMake or the Visual Studio C++/CMake tools.");
            s_Running = false; return false;
        }

        std::lock_guard<std::mutex> lock(s_StateMutex);
        if (s_Worker.joinable()) s_Worker.join();
        s_ReloadPending = false;
        const std::uint64_t generation = ++s_Generation;
        Log("[Scripts] Hot reload generation: " + std::to_string(generation));

        s_Worker = std::thread([projectRoot, cmake, generation]()
        {
            Log("[Scripts] CMake: " + cmake.string());
            Log("[Scripts] Configuring project...");
            const auto out = projectRoot / "out";
            const int configCode = RunProcess(cmake,
                {L"-S", projectRoot.wstring(), L"-B", out.wstring(),
                 L"-DNOJOB_HOT_RELOAD_GENERATION=" + std::to_wstring(generation)}, "[CMake] ");
            if (configCode != 0)
            {
                Log("[Scripts] CMake configure FAILED (exit code " + std::to_string(configCode) + ").");
                s_Running = false; return;
            }
            Log("[Scripts] Incremental build: compiling changed scripts only...");
            const int buildCode = RunProcess(cmake,
                {L"--build", out.wstring(), L"--target", L"NoJobProjectScripts", L"--config", L"Debug"}, "[Build] ");
            if (buildCode != 0)
            {
                Log("[Scripts] Build FAILED (exit code " + std::to_string(buildCode) + ").");
                s_Running = false; return;
            }
            Log("[Scripts] Build succeeded. DLL reload queued...");
            s_ReloadPending = true;
            s_Running = false;
        });
        return true;
    }

    bool ProjectScriptBuildSystem::IsRunning() { return s_Running.load(); }
    bool ProjectScriptBuildSystem::ConsumeReloadPending() { return s_ReloadPending.exchange(false); }

    void ProjectScriptBuildSystem::Shutdown()
    {
        std::lock_guard<std::mutex> lock(s_StateMutex);
        if (s_Worker.joinable()) s_Worker.join();
        s_Running = false;
        s_ReloadPending = false;
    }
}
