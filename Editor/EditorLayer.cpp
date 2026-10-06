#include "Editor/EditorLayer.h"

#include "Engine/Asset/AssetManager.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Texture.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/Scene/NativeScripts.h"
#include "Engine/Scene/SceneRenderer.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Assets/MaterialSerializer.h"
#include "Engine/Assets/PrefabSerializer.h"
#include "Engine/Animation/Animation.h"
#include "Engine/Audio/AudioEngine.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <atomic>
#include <mutex>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <commdlg.h>
#include <cctype>
#endif

namespace NoJob
{
    // Set during Init when there is no valid saved docking tree.
    // DockSpaceOverViewport itself creates a root node, so checking for a node
    // after that call cannot tell us whether a saved layout existed.
    static bool s_BuildDefaultDockLayout = false;
    namespace
    {
#ifdef _WIN32
        HMODULE s_ProjectScriptsModule = nullptr;
        std::vector<std::string> s_ProjectScriptNames;
        std::vector<std::string> s_ScriptConsole;
        std::mutex s_ScriptConsoleMutex;
        std::atomic_bool s_ScriptCompileRunning{ false };
        std::atomic_bool s_ProjectScriptsReloadPending{ false };
        std::atomic_bool s_ScriptCompileSucceeded{ false };
        std::thread s_ScriptCompileThread;
        bool s_ScriptsCompiled = false;
        std::uint64_t s_ProjectScriptReloadGeneration = 0;

        void ScriptLog(std::string message)
        {
            std::lock_guard<std::mutex> lock(s_ScriptConsoleMutex);
            s_ScriptConsole.push_back(std::move(message));
        }

        void ClearScriptLog()
        {
            std::lock_guard<std::mutex> lock(s_ScriptConsoleMutex);
            s_ScriptConsole.clear();
        }

        using RegisterProjectScriptFn = void(*)(void(*)(ScriptDefinition));

        void HostRegisterProjectScript(ScriptDefinition definition)
        {
            s_ProjectScriptNames.push_back(definition.Name);
            ScriptRegistry::Register(std::move(definition));
        }

        void UnloadProjectScripts()
        {
            for (const auto& name : s_ProjectScriptNames)
                ScriptRegistry::Unregister(name);
            s_ProjectScriptNames.clear();

            if (s_ProjectScriptsModule)
            {
                FreeLibrary(s_ProjectScriptsModule);
                s_ProjectScriptsModule = nullptr;
            }
        }

        bool LoadProjectScripts(const std::filesystem::path& root)
        {
            UnloadProjectScripts();

            std::filesystem::path dll;
            const auto outputDirectory = root / "out" / "ProjectScripts";
            std::error_code dllEc;
            std::filesystem::file_time_type newestTime{};

            if (std::filesystem::exists(outputDirectory, dllEc))
            {
                for (auto it = std::filesystem::recursive_directory_iterator(
                         outputDirectory,
                         std::filesystem::directory_options::skip_permission_denied,
                         dllEc);
                     !dllEc && it != std::filesystem::recursive_directory_iterator();
                     ++it)
                {
                    if (!it->is_regular_file(dllEc))
                        continue;

                    const auto candidate = it->path();
                    if (candidate.extension() != ".dll")
                        continue;

                    // Accept Debug postfix too:
                    // NoJobProjectScripts_12.dll / NoJobProjectScripts_12d.dll
                    const std::string stem = candidate.stem().string();
                    if (stem.rfind("NoJobProjectScripts_", 0) != 0)
                        continue;

                    const auto writeTime =
                        std::filesystem::last_write_time(candidate, dllEc);
                    if (dllEc)
                    {
                        dllEc.clear();
                        continue;
                    }

                    if (dll.empty() || writeTime > newestTime)
                    {
                        dll = candidate;
                        newestTime = writeTime;
                    }
                }
            }

            if (dll.empty())
            {
                ScriptLog(
                    "[Scripts] Versioned ProjectScripts DLL not found under: " +
                    outputDirectory.string());
                return false;
            }

            ScriptLog("[Scripts] Loading versioned DLL: " + dll.string());

            s_ProjectScriptsModule = LoadLibraryW(dll.wstring().c_str());
            if (!s_ProjectScriptsModule)
            {
                ScriptLog(
                    "[Scripts] LoadLibrary failed (Win32 error " +
                    std::to_string(GetLastError()) + ").");
                return false;
            }

            const auto scripts = AssetManager::GetAssetsDirectory() / "Scripts";
            std::error_code ec;
            if (std::filesystem::exists(scripts, ec))
            {
                for (const auto& entry : std::filesystem::directory_iterator(scripts, ec))
                {
                    if (entry.path().extension() != ".cpp") continue;
                    const auto name = entry.path().stem().string();
                    const auto symbol = "NoJobRegister_" + name;
                    auto fn = reinterpret_cast<RegisterProjectScriptFn>(
                        GetProcAddress(s_ProjectScriptsModule, symbol.c_str()));
                    if (fn) fn(&HostRegisterProjectScript);
                }
            }

            ScriptLog(
                "[Scripts] Loaded " + std::to_string(s_ProjectScriptNames.size()) +
                " project script(s).");
            return true;
        }

        std::filesystem::path FindCMakeExecutable()
        {
#ifdef _WIN32
            wchar_t pathBuffer[32768]{};
            const DWORD found = SearchPathW(nullptr, L"cmake.exe", nullptr,
                static_cast<DWORD>(std::size(pathBuffer)), pathBuffer, nullptr);
            if (found > 0 && found < std::size(pathBuffer))
                return std::filesystem::path(pathBuffer);

            wchar_t* programFilesX86 = nullptr;
            std::size_t envLength = 0;
            if (_wdupenv_s(&programFilesX86, &envLength, L"ProgramFiles(x86)") == 0 &&
                programFilesX86)
            {
                const std::filesystem::path vswhere =
                    std::filesystem::path(programFilesX86) /
                    "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
                free(programFilesX86);

                if (std::filesystem::exists(vswhere))
                {
                    const std::wstring command =
                        L"\"" + vswhere.wstring() +
                        L"\" -latest -products * -property installationPath";
                    FILE* pipe = _wpopen(command.c_str(), L"rt");
                    if (pipe)
                    {
                        wchar_t buffer[4096]{};
                        std::wstring installPath;
                        if (fgetws(buffer, static_cast<int>(std::size(buffer)), pipe))
                            installPath = buffer;
                        _pclose(pipe);
                        while (!installPath.empty() &&
                               (installPath.back()==L'\r' || installPath.back()==L'\n' ||
                                installPath.back()==L' ' || installPath.back()==L'\t'))
                            installPath.pop_back();

                        if (!installPath.empty())
                        {
                            const auto bundled = std::filesystem::path(installPath) /
                                "Common7" / "IDE" / "CommonExtensions" /
                                "Microsoft" / "CMake" / "CMake" / "bin" / "cmake.exe";
                            if (std::filesystem::exists(bundled)) return bundled;
                        }
                    }
                }
            }

            wchar_t* programFiles = nullptr;
            envLength = 0;
            if (_wdupenv_s(&programFiles, &envLength, L"ProgramFiles") == 0 &&
                programFiles)
            {
                const auto standalone = std::filesystem::path(programFiles) /
                    "CMake" / "bin" / "cmake.exe";
                free(programFiles);
                if (std::filesystem::exists(standalone)) return standalone;
            }
#endif
            return {};
        }

                int RunProcessToScriptConsole(
            const std::filesystem::path& executable,
            const std::vector<std::wstring>& arguments,
            const std::string& prefix)
        {
#ifdef _WIN32
            SECURITY_ATTRIBUTES security{};
            security.nLength = sizeof(SECURITY_ATTRIBUTES);
            security.bInheritHandle = TRUE;

            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            if (!CreatePipe(&readPipe, &writePipe, &security, 0))
            {
                ScriptLog("[Scripts] CreatePipe failed.");
                return -1;
            }

            SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

            std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
            for (const auto& arg : arguments)
            {
                commandLine += L" \"";
                for (wchar_t c : arg)
                {
                    if (c == L'"')
                        commandLine += L'\\';
                    commandLine += c;
                }
                commandLine += L"\"";
            }

            STARTUPINFOW startup{};
            startup.cb = sizeof(STARTUPINFOW);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdOutput = writePipe;
            startup.hStdError = writePipe;
            startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

            PROCESS_INFORMATION process{};
            std::vector<wchar_t> mutableCommand(
                commandLine.begin(), commandLine.end());
            mutableCommand.push_back(L'\0');

            const std::wstring workingDirectory =
                AssetManager::GetProjectRoot().wstring();

            const BOOL created = CreateProcessW(
                executable.wstring().c_str(),
                mutableCommand.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                workingDirectory.c_str(),
                &startup,
                &process);

            CloseHandle(writePipe);

            if (!created)
            {
                const DWORD error = GetLastError();
                CloseHandle(readPipe);
                ScriptLog(
                    "[Scripts] CreateProcess failed (Win32 error " +
                    std::to_string(error) + ").");
                return -1;
            }

            std::string pending;
            char buffer[2048];
            DWORD bytesRead = 0;
            while (ReadFile(
                       readPipe,
                       buffer,
                       static_cast<DWORD>(sizeof(buffer)),
                       &bytesRead,
                       nullptr) &&
                   bytesRead > 0)
            {
                pending.append(buffer, buffer + bytesRead);

                std::size_t newline = 0;
                while ((newline = pending.find('\n')) != std::string::npos)
                {
                    std::string line = pending.substr(0, newline);
                    pending.erase(0, newline + 1);
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if (!line.empty())
                        ScriptLog(prefix + line);
                }
            }

            if (!pending.empty())
            {
                if (!pending.empty() && pending.back() == '\r')
                    pending.pop_back();
                if (!pending.empty())
                    ScriptLog(prefix + pending);
            }

            WaitForSingleObject(process.hProcess, INFINITE);

            DWORD exitCode = 1;
            GetExitCodeProcess(process.hProcess, &exitCode);

            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(readPipe);

            return static_cast<int>(exitCode);
#else
            (void)executable;
            (void)arguments;
            (void)prefix;
            return -1;
#endif
        }

        bool CompileProjectScripts()
        {
            if (s_ScriptCompileRunning.exchange(true))
            {
                ScriptLog("[Scripts] Compilation is already running.");
                return false;
            }

            // DLL unloading touches ScriptRegistry and therefore stays on the
            // editor/main thread. The worker only runs external build tools.
            UnloadProjectScripts();
            ClearScriptLog();
            ScriptLog("[Scripts] Starting asynchronous script compilation...");

            const auto root = AssetManager::GetProjectRoot();
            const auto cmake = FindCMakeExecutable();
            if (cmake.empty())
            {
                ScriptLog("[Scripts] CMake was not found.");
                ScriptLog("[Scripts] Install CMake or the Visual Studio C++/CMake tools.");
                s_ScriptCompileRunning = false;
                return false;
            }

            s_ScriptCompileSucceeded = false;
            s_ProjectScriptsReloadPending = false;

            const std::uint64_t generation =
                ++s_ProjectScriptReloadGeneration;
            ScriptLog(
                "[Scripts] Hot reload generation: " +
                std::to_string(generation));

            s_ScriptCompileThread = std::thread([root, cmake, generation]()
            {
                ScriptLog("[Scripts] CMake: " + cmake.string());
                ScriptLog("[Scripts] Configuring project...");

                const auto out = root / "out";
                const int configCode = RunProcessToScriptConsole(
                    cmake,
                    {
                        L"-S", root.wstring(),
                        L"-B", out.wstring(),
                        L"-DNOJOB_HOT_RELOAD_GENERATION=" +
                            std::to_wstring(generation)
                    },
                    "[CMake] ");

                if (configCode != 0)
                {
                    ScriptLog("[Scripts] CMake configure FAILED (exit code " +
                              std::to_string(configCode) + ").");
                    s_ScriptCompileRunning = false;
                    return;
                }

                ScriptLog("[Scripts] Incremental build: compiling changed scripts only...");
                const int buildCode = RunProcessToScriptConsole(
                    cmake,
                    { L"--build", out.wstring(), L"--target",
                      L"NoJobProjectScripts", L"--config", L"Debug" },
                    "[Build] ");

                if (buildCode != 0)
                {
                    ScriptLog("[Scripts] Build FAILED (exit code " +
                              std::to_string(buildCode) + ").");
                    s_ScriptCompileRunning = false;
                    return;
                }

                ScriptLog("[Scripts] Build succeeded. DLL reload queued...");
                s_ScriptCompileSucceeded = true;
                s_ProjectScriptsReloadPending = true;
                s_ScriptCompileRunning = false;
            });
            s_ScriptCompileThread.detach();

            return true;
        }
#endif

        std::filesystem::path FindNoJobProjectRoot()
        {
            auto containsProject = [](const std::filesystem::path& directory)
            {
                std::error_code ec;
                if (!std::filesystem::exists(directory, ec))
                    return false;

                bool hasProjectFile = false;
                for (const auto& entry :
                     std::filesystem::directory_iterator(
                         directory,
                         std::filesystem::directory_options::skip_permission_denied,
                         ec))
                {
                    if (ec) break;
                    if (entry.is_regular_file() &&
                        entry.path().extension() == ".nojobproject")
                    {
                        hasProjectFile = true;
                        break;
                    }
                }

                return hasProjectFile &&
                       std::filesystem::exists(directory / "CMakeLists.txt", ec);
            };

            auto walkUp = [&](std::filesystem::path start)
                -> std::filesystem::path
            {
                std::error_code ec;
                start = std::filesystem::absolute(start, ec).lexically_normal();

                while (!start.empty())
                {
                    if (containsProject(start))
                        return start;

                    const auto parent = start.parent_path();
                    if (parent == start)
                        break;
                    start = parent;
                }
                return {};
            };

            // First try the process working directory.
            if (auto root = walkUp(std::filesystem::current_path()); !root.empty())
                return root;

#ifdef _WIN32
            // Then try the executable location. This makes the editor portable
            // when launched from out/build/... instead of the repository root.
            std::wstring executable(MAX_PATH, L'\0');
            const DWORD length = GetModuleFileNameW(
                nullptr, executable.data(), static_cast<DWORD>(executable.size()));
            if (length > 0)
            {
                executable.resize(length);
                if (auto root =
                        walkUp(std::filesystem::path(executable).parent_path());
                    !root.empty())
                    return root;
            }
#endif

            return std::filesystem::current_path();
        }

#ifdef _WIN32
        std::filesystem::path FindVisualStudioExecutable()
        {
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
            return std::filesystem::exists(devenv) ? devenv
                                                    : std::filesystem::path{};
        }

        bool OpenScriptInVisualStudio(const std::filesystem::path& scriptPath)
        {
            const auto absoluteScript =
                std::filesystem::absolute(scriptPath).lexically_normal();

            if (!std::filesystem::exists(absoluteScript))
            {
                ScriptLog(
                    "[Scripts] File not found: " + absoluteScript.string());
                return false;
            }

            const auto visualStudio = FindVisualStudioExecutable();
            if (!visualStudio.empty())
            {
                const auto result = reinterpret_cast<std::intptr_t>(
                    ShellExecuteW(
                        nullptr,
                        L"open",
                        visualStudio.wstring().c_str(),
                        (L"\"" + absoluteScript.wstring() + L"\"").c_str(),
                        AssetManager::GetProjectRoot().wstring().c_str(),
                        SW_SHOWNORMAL));

                if (result > 32)
                {
                    ScriptLog(
                        "[Scripts] Opened in Visual Studio: " +
                        absoluteScript.filename().string());
                    return true;
                }
            }

            // Portable fallback: use the Windows association for C++ files.
            const auto result = reinterpret_cast<std::intptr_t>(
                ShellExecuteW(
                    nullptr,
                    L"open",
                    absoluteScript.wstring().c_str(),
                    nullptr,
                    AssetManager::GetProjectRoot().wstring().c_str(),
                    SW_SHOWNORMAL));

            if (result > 32)
            {
                ScriptLog(
                    "[Scripts] Opened with Windows file association: " +
                    absoluteScript.filename().string());
                return true;
            }

            ScriptLog(
                "[Scripts] Could not open " + absoluteScript.string());
            return false;
        }
#endif

        std::string SanitizeCppIdentifier(std::string name)
        {
            name.erase(std::remove_if(name.begin(), name.end(),
                [](unsigned char c){ return !(std::isalnum(c) || c == '_'); }),
                name.end());
            if (name.empty()) name = "NewScript";
            if (std::isdigit(static_cast<unsigned char>(name.front())))
                name.insert(name.begin(), '_');
            return name;
        }

        bool CreateCppScriptAsset(const std::filesystem::path& scriptsDirectory,
                                  const std::string& requestedName)
        {
            const std::string name = SanitizeCppIdentifier(requestedName);
            std::error_code ec;
            std::filesystem::create_directories(scriptsDirectory, ec);
            if (ec)
            {
#ifdef _WIN32
                ScriptLog(
                    "[Scripts] Could not create Assets/Scripts: " + ec.message());
#endif
                return false;
            }

            const auto headerPath = scriptsDirectory / (name + ".h");
            const auto sourcePath = scriptsDirectory / (name + ".cpp");
            if (std::filesystem::exists(headerPath) ||
                std::filesystem::exists(sourcePath))
                return false;

            std::ofstream header(headerPath);
            std::ofstream source(sourcePath);
            if (!header || !source)
            {
#ifdef _WIN32
                ScriptLog(
                    "[Scripts] Could not write script files to: " +
                    scriptsDirectory.string());
#endif
                return false;
            }

            header << "#pragma once\n"
                   << "#include \"Engine/Scene/ScriptableEntity.h\"\n\n"
                   << "namespace NoJob\n{\n"
                   << "    class " << name << " final : public Script\n"
                   << "    {\n    public:\n"
                   << "        // Exposed fields: float, int, bool, glm::vec3\n"
                   << "        float Speed = 5.0f;\n"
                   << "        int Lives = 3;\n"
                   << "        bool EnabledMovement = true;\n"
                   << "        glm::vec3 Direction{ 1.0f, 0.0f, 0.0f };\n\n"
                   << "        void OnCreate() override;\n"
                   << "        void OnUpdate(float deltaTime) override;\n"
                   << "        void OnDestroy() override;\n"
                   << "    };\n}\n";

            source << "#include \"" << name << ".h\"\n"
                   << "#include \"Engine/Scene/ScriptRegistry.h\"\n\n"
                   << "namespace NoJob\n{\n"
                   << "    void " << name << "::OnCreate()\n    {\n    }\n\n"
                   << "    void " << name << "::OnUpdate(float deltaTime)\n"
                   << "    {\n        (void)deltaTime;\n    }\n\n"
                   << "    void " << name << "::OnDestroy()\n    {\n    }\n\n"
                   << "}\n\n"
                   << "NOJOB_REGISTER_SCRIPT(" << name << ", \"Gameplay\",\n"
                   << "    NOJOB_FIELD(" << name << ", Speed),\n"
                   << "    NOJOB_FIELD(" << name << ", Lives),\n"
                   << "    NOJOB_FIELD(" << name << ", EnabledMovement),\n"
                   << "    NOJOB_FIELD(" << name << ", Direction))\n";

            AssetRegistry registry(AssetManager::GetAssetsDirectory());
            registry.Load();
            registry.Register(headerPath, AssetType::Script);
            registry.Register(sourcePath, AssetType::Script);
            registry.Save();
#ifdef _WIN32
            ScriptLog(
                "[Scripts] Created " + name + ".h / " + name +
                ".cpp in Assets/Scripts.");
#endif
            return true;
        }
        ImVec2 ProjectColliderPoint(
            const glm::vec3& point,
            const glm::mat4& viewProjection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            bool& visible)
        {
            const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
            if (clip.w <= 0.0001f)
            {
                visible = false;
                return {};
            }

            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            visible = ndc.z >= -1.0f && ndc.z <= 1.0f;

            return {
                viewportMin.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x,
                viewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSize.y
            };
        }

        void DrawColliderLine(
            ImDrawList* drawList,
            const glm::vec3& a,
            const glm::vec3& b,
            const glm::mat4& viewProjection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImU32 color,
            float thickness)
        {
            bool visibleA = false;
            bool visibleB = false;
            const ImVec2 screenA = ProjectColliderPoint(
                a, viewProjection, viewportMin, viewportSize, visibleA);
            const ImVec2 screenB = ProjectColliderPoint(
                b, viewProjection, viewportMin, viewportSize, visibleB);

            if (!visibleA && !visibleB)
                return;

            drawList->AddLine(screenA, screenB, color, thickness);
        }

        glm::vec3 ColliderTransformPoint(
            const glm::mat4& transform,
            const glm::vec3& point)
        {
            return glm::vec3(transform * glm::vec4(point, 1.0f));
        }

        void DrawBoxColliderWire(
            ImDrawList* drawList,
            const glm::mat4& world,
            const glm::vec3& size,
            const glm::mat4& viewProjection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImU32 color,
            float thickness)
        {
            const glm::vec3 h = glm::max(size * 0.5f, glm::vec3(0.005f));
            const glm::vec3 local[8] = {
                {-h.x,-h.y,-h.z}, { h.x,-h.y,-h.z},
                { h.x, h.y,-h.z}, {-h.x, h.y,-h.z},
                {-h.x,-h.y, h.z}, { h.x,-h.y, h.z},
                { h.x, h.y, h.z}, {-h.x, h.y, h.z}
            };

            glm::vec3 points[8];
            for (int i = 0; i < 8; ++i)
                points[i] = ColliderTransformPoint(world, local[i]);

            constexpr int edges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}
            };

            for (const auto& edge : edges)
                DrawColliderLine(
                    drawList, points[edge[0]], points[edge[1]],
                    viewProjection, viewportMin, viewportSize,
                    color, thickness);
        }

        void DrawColliderEllipse(
            ImDrawList* drawList,
            const glm::mat4& world,
            float radiusA,
            float radiusB,
            int plane,
            const glm::vec3& offset,
            const glm::mat4& viewProjection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImU32 color,
            float thickness)
        {
            constexpr int segments = 48;
            glm::vec3 previous{};
            bool hasPrevious = false;

            for (int i = 0; i <= segments; ++i)
            {
                const float angle =
                    glm::two_pi<float>() * static_cast<float>(i) /
                    static_cast<float>(segments);
                const float c = std::cos(angle);
                const float s = std::sin(angle);

                glm::vec3 local = offset;
                if (plane == 0) {
                    local.y += c * radiusA;
                    local.z += s * radiusB;
                }
                else if (plane == 1) {
                    local.x += c * radiusA;
                    local.z += s * radiusB;
                }
                else {
                    local.x += c * radiusA;
                    local.y += s * radiusB;
                }

                const glm::vec3 current =
                    ColliderTransformPoint(world, local);

                if (hasPrevious)
                    DrawColliderLine(
                        drawList, previous, current,
                        viewProjection, viewportMin, viewportSize,
                        color, thickness);

                previous = current;
                hasPrevious = true;
            }
        }

        void DrawSphereColliderWire(
            ImDrawList* drawList,
            const glm::mat4& world,
            float radius,
            const glm::mat4& viewProjection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImU32 color,
            float thickness)
        {
            radius = std::max(radius, 0.005f);
            DrawColliderEllipse(drawList, world, radius, radius, 0, {},
                viewProjection, viewportMin, viewportSize, color, thickness);
            DrawColliderEllipse(drawList, world, radius, radius, 1, {},
                viewProjection, viewportMin, viewportSize, color, thickness);
            DrawColliderEllipse(drawList, world, radius, radius, 2, {},
                viewProjection, viewportMin, viewportSize, color, thickness);
        }

        void DrawCapsuleColliderWire(
            ImDrawList* drawList,
            const glm::mat4& world,
            float radius,
            float height,
            const glm::mat4& viewProjection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImU32 color,
            float thickness)
        {
            radius = std::max(radius, 0.005f);
            height = std::max(height, radius * 2.0f);
            const float halfCylinder = height * 0.5f - radius;

            DrawColliderEllipse(drawList, world, radius, radius, 1,
                {0.0f, halfCylinder, 0.0f},
                viewProjection, viewportMin, viewportSize, color, thickness);
            DrawColliderEllipse(drawList, world, radius, radius, 1,
                {0.0f,-halfCylinder, 0.0f},
                viewProjection, viewportMin, viewportSize, color, thickness);

            const glm::vec3 top[4] = {
                { radius, halfCylinder, 0.0f},
                {-radius, halfCylinder, 0.0f},
                {0.0f, halfCylinder, radius},
                {0.0f, halfCylinder,-radius}
            };
            const glm::vec3 bottom[4] = {
                { radius,-halfCylinder, 0.0f},
                {-radius,-halfCylinder, 0.0f},
                {0.0f,-halfCylinder, radius},
                {0.0f,-halfCylinder,-radius}
            };

            for (int i = 0; i < 4; ++i)
                DrawColliderLine(
                    drawList,
                    ColliderTransformPoint(world, top[i]),
                    ColliderTransformPoint(world, bottom[i]),
                    viewProjection, viewportMin, viewportSize,
                    color, thickness);

            // Two full meridians create the rounded top/bottom silhouette.
            DrawColliderEllipse(drawList, world, radius,
                halfCylinder + radius, 2, {},
                viewProjection, viewportMin, viewportSize, color, thickness);
            DrawColliderEllipse(drawList, world, radius,
                halfCylinder + radius, 0, {},
                viewProjection, viewportMin, viewportSize, color, thickness);
        }

        void DrawSceneColliderGizmos(
            Scene& scene,
            Entity selectedEntity,
            const glm::mat4& view,
            const glm::mat4& projection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize)
        {
            if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f)
                return;

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(
                viewportMin,
                {viewportMin.x + viewportSize.x,
                 viewportMin.y + viewportSize.y},
                true);

            const glm::mat4 viewProjection = projection * view;
            std::uint64_t selectedID = 0;
            if (selectedEntity && selectedEntity.HasComponent<IDComponent>())
                selectedID = selectedEntity.GetComponent<IDComponent>().ID;

            for (Entity entity : scene.GetEntities())
            {
                const std::uint64_t entityID =
                    entity.GetComponent<IDComponent>().ID;
                const bool selected =
                    selectedID != 0 && entityID == selectedID;

                const float thickness = selected ? 2.5f : 1.25f;
                const ImU32 normalColor = selected
                    ? IM_COL32(110, 255, 135, 255)
                    : IM_COL32(80, 205, 110, 175);
                const ImU32 triggerColor = selected
                    ? IM_COL32(255, 205, 80, 255)
                    : IM_COL32(230, 170, 65, 175);

                const glm::mat4 world = scene.GetWorldTransform(entity);

                if (entity.HasComponent<BoxColliderComponent>())
                {
                    const auto& c =
                        entity.GetComponent<BoxColliderComponent>();
                    DrawBoxColliderWire(
                        drawList, world, c.Size,
                        viewProjection, viewportMin, viewportSize,
                        c.IsTrigger ? triggerColor : normalColor,
                        thickness);
                }

                if (entity.HasComponent<SphereColliderComponent>())
                {
                    const auto& c =
                        entity.GetComponent<SphereColliderComponent>();
                    DrawSphereColliderWire(
                        drawList, world, c.Radius,
                        viewProjection, viewportMin, viewportSize,
                        c.IsTrigger ? triggerColor : normalColor,
                        thickness);
                }

                if (entity.HasComponent<CapsuleColliderComponent>())
                {
                    const auto& c =
                        entity.GetComponent<CapsuleColliderComponent>();
                    DrawCapsuleColliderWire(
                        drawList, world, c.Radius, c.Height,
                        viewProjection, viewportMin, viewportSize,
                        c.IsTrigger ? triggerColor : normalColor,
                        thickness);
                }

                const glm::vec3 origin =
                    ColliderTransformPoint(world, {0.0f, 0.0f, 0.0f});
                glm::vec3 forward =
                    ColliderTransformPoint(world, {0.0f, 0.0f, -1.0f}) -
                    origin;
                if (glm::length(forward) > 0.0001f)
                    forward = glm::normalize(forward);
                else
                    forward = {0.0f, 0.0f, -1.0f};

                const ImU32 cameraColor =
                    selected ? IM_COL32(100, 220, 255, 255)
                             : IM_COL32(70, 170, 220, 180);
                const ImU32 lightColor =
                    selected ? IM_COL32(255, 235, 90, 255)
                             : IM_COL32(235, 205, 70, 190);

                if (entity.HasComponent<CameraComponent>())
                {
                    const auto& camera =
                        entity.GetComponent<CameraComponent>();
                    const float distance = 1.5f;
                    float halfHeight = 0.7f;
                    float halfWidth = halfHeight *
                        (viewportSize.x / std::max(viewportSize.y, 1.0f));

                    if (camera.ProjectionType ==
                        CameraProjectionType::Perspective)
                    {
                        halfHeight =
                            std::tan(glm::radians(camera.PerspectiveFOV * 0.5f))
                            * distance;
                        halfWidth = halfHeight *
                            (viewportSize.x / std::max(viewportSize.y, 1.0f));
                    }
                    else
                    {
                        halfHeight = camera.OrthographicSize * 0.25f;
                        halfWidth = halfHeight *
                            (viewportSize.x / std::max(viewportSize.y, 1.0f));
                    }

                    const glm::vec3 localCorners[4] = {
                        {-halfWidth,-halfHeight,-distance},
                        { halfWidth,-halfHeight,-distance},
                        { halfWidth, halfHeight,-distance},
                        {-halfWidth, halfHeight,-distance}
                    };
                    glm::vec3 corners[4];
                    for (int i = 0; i < 4; ++i)
                    {
                        corners[i] =
                            ColliderTransformPoint(world, localCorners[i]);
                        DrawColliderLine(
                            drawList, origin, corners[i],
                            viewProjection, viewportMin, viewportSize,
                            cameraColor, thickness);
                    }
                    for (int i = 0; i < 4; ++i)
                        DrawColliderLine(
                            drawList, corners[i], corners[(i + 1) % 4],
                            viewProjection, viewportMin, viewportSize,
                            cameraColor, thickness);
                }

                if (entity.HasComponent<DirectionalLightComponent>())
                {
                    DrawColliderLine(
                        drawList,
                        origin,
                        origin + forward * 1.0f,
                        viewProjection, viewportMin, viewportSize,
                        lightColor, thickness);
                }

                if (entity.HasComponent<PointLightComponent>())
                {
                    const auto& light =
                        entity.GetComponent<PointLightComponent>();
                    const float radius =
                        std::min(std::max(light.Range * 0.04f, 0.12f), 0.55f);
                    DrawSphereColliderWire(
                        drawList,
                        glm::translate(glm::mat4(1.0f), origin),
                        radius,
                        viewProjection, viewportMin, viewportSize,
                        lightColor, thickness);
                }

                if (entity.HasComponent<ParticleSystemComponent>())
                {
                    const auto& particles =
                        entity.GetComponent<ParticleSystemComponent>();
                    const ImU32 particleColor = selected
                        ? IM_COL32(255, 120, 210, 255)
                        : IM_COL32(220, 90, 185, 155);
                    if (particles.Shape == ParticleShape::Sphere)
                    {
                        DrawSphereColliderWire(
                            drawList,
                            glm::translate(glm::mat4(1.0f), origin),
                            std::max(particles.ShapeRadius, 0.01f),
                            viewProjection, viewportMin, viewportSize,
                            particleColor, thickness);
                    }
                    else if (particles.Shape == ParticleShape::Cone)
                    {
                        glm::vec3 dir = particles.Direction;
                        if (glm::length(dir) < 0.0001f) dir = {0,1,0};
                        dir = glm::normalize(glm::mat3(world) * glm::normalize(dir));
                        const float length = 1.25f;
                        const float radius = std::tan(glm::radians(
                            glm::clamp(particles.ConeAngle,0.0f,89.0f))) * length;
                        glm::vec3 tangent = std::abs(dir.y) < 0.99f
                            ? glm::normalize(glm::cross(dir,glm::vec3(0,1,0)))
                            : glm::vec3(1,0,0);
                        glm::vec3 bitangent = glm::normalize(glm::cross(dir,tangent));
                        const glm::vec3 center = origin + dir * length;
                        for (int i=0;i<4;++i)
                        {
                            const float a = glm::half_pi<float>() * float(i);
                            const glm::vec3 edge = center +
                                tangent * std::cos(a)*radius +
                                bitangent * std::sin(a)*radius;
                            DrawColliderLine(drawList, origin, edge,
                                viewProjection, viewportMin, viewportSize,
                                particleColor, thickness);
                        }
                    }
                }

                if (entity.HasComponent<AudioSourceComponent>())
                {
                    const auto& audio = entity.GetComponent<AudioSourceComponent>();
                    if (audio.SpatialBlend > 0.001f)
                    {
                        const ImU32 minColor = selected
                            ? IM_COL32(100, 220, 255, 255)
                            : IM_COL32(80, 180, 235, 175);
                        const ImU32 maxColor = selected
                            ? IM_COL32(185, 120, 255, 255)
                            : IM_COL32(150, 90, 220, 145);
                        const glm::mat4 audioWorld =
                            glm::translate(glm::mat4(1.0f), origin);
                        DrawSphereColliderWire(
                            drawList, audioWorld, std::max(audio.MinDistance, 0.01f),
                            viewProjection, viewportMin, viewportSize,
                            minColor, thickness);
                        if (selected)
                            DrawSphereColliderWire(
                                drawList, audioWorld,
                                std::max(audio.MaxDistance, audio.MinDistance + 0.01f),
                                viewProjection, viewportMin, viewportSize,
                                maxColor, 1.25f);
                    }
                }

                if (entity.HasComponent<AudioListenerComponent>() &&
                    entity.GetComponent<AudioListenerComponent>().Enabled)
                {
                    const ImU32 listenerColor = selected
                        ? IM_COL32(255, 150, 90, 255)
                        : IM_COL32(235, 125, 70, 180);
                    DrawColliderLine(
                        drawList, origin, origin + forward * 0.8f,
                        viewProjection, viewportMin, viewportSize,
                        listenerColor, thickness);
                }

                if (entity.HasComponent<SpotLightComponent>())
                {
                    const auto& light =
                        entity.GetComponent<SpotLightComponent>();
                    const float length =
                        std::min(std::max(light.Range * 0.1f, 0.35f), 1.5f);
                    const float radius =
                        std::tan(glm::radians(light.OuterAngle)) * length;
                    const glm::vec3 tipLocal{0.0f, 0.0f, 0.0f};
                    const glm::vec3 ringLocal[4] = {
                        { radius, 0.0f,-length},
                        {-radius, 0.0f,-length},
                        {0.0f, radius,-length},
                        {0.0f,-radius,-length}
                    };
                    for (const auto& local : ringLocal)
                        DrawColliderLine(
                            drawList,
                            ColliderTransformPoint(world, tipLocal),
                            ColliderTransformPoint(world, local),
                            viewProjection, viewportMin, viewportSize,
                            lightColor, thickness);
                    DrawColliderEllipse(
                        drawList, world, radius, radius, 2,
                        {0.0f, 0.0f,-length},
                        viewProjection, viewportMin, viewportSize,
                        lightColor, thickness);
                }
            }

            drawList->PopClipRect();
        }

        std::string OpenModelFileDialog(){
#ifdef _WIN32
char f[MAX_PATH]{};OPENFILENAMEA d{};d.lStructSize=sizeof(d);d.lpstrFile=f;d.nMaxFile=MAX_PATH;d.lpstrFilter=
"3D Models\0*.obj;*.fbx;*.gltf;*.glb;*.dae;*.stl;*.ply;*.3ds;*.blend\0"
"glTF / GLB\0*.gltf;*.glb\0"
"FBX\0*.fbx\0"
"Wavefront OBJ\0*.obj\0"
"All Files\0*.*\0";d.Flags=OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameA(&d)==TRUE)return f;
#endif
return{};}
        std::string OpenHDRIFileDialog()
        {
#ifdef _WIN32
            char fileName[MAX_PATH]{};
            OPENFILENAMEA dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.lpstrFile = fileName;
            dialog.nMaxFile = MAX_PATH;
            dialog.lpstrFilter =
                "HDR Environment (*.hdr)\0*.hdr\0"
                "All Files\0*.*\0";
            dialog.nFilterIndex = 1;
            dialog.Flags =
                OFN_PATHMUSTEXIST |
                OFN_FILEMUSTEXIST |
                OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&dialog) == TRUE)
                return fileName;
#endif
            return {};
        }

        std::string OpenAudioFileDialog()
        {
#ifdef _WIN32
            char fileName[MAX_PATH]{};
            OPENFILENAMEA dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.lpstrFile = fileName;
            dialog.nMaxFile = MAX_PATH;
            dialog.lpstrFilter =
                "Audio Files (*.wav;*.mp3;*.flac)\0*.wav;*.mp3;*.flac\0"
                "WAV (*.wav)\0*.wav\0"
                "MP3 (*.mp3)\0*.mp3\0"
                "FLAC (*.flac)\0*.flac\0"
                "All Files\0*.*\0";
            dialog.nFilterIndex = 1;
            dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&dialog) == TRUE) return fileName;
#endif
            return {};
        }

        std::string OpenTextureFileDialog()
        {
#ifdef _WIN32
            char fileName[MAX_PATH]{};

            OPENFILENAMEA dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.lpstrFile = fileName;
            dialog.nMaxFile = MAX_PATH;
            dialog.lpstrFilter =
                "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0"
                "PNG Files\0*.png\0"
                "JPEG Files\0*.jpg;*.jpeg\0"
                "All Files\0*.*\0";
            dialog.nFilterIndex = 1;
            dialog.Flags =
                OFN_PATHMUSTEXIST |
                OFN_FILEMUSTEXIST |
                OFN_NOCHANGEDIR;

            if (GetOpenFileNameA(&dialog) == TRUE)
                return fileName;
#endif
            return {};
        }
    }

    void EditorLayer::Init(GLFWwindow* window, Scene* scene)
    {
        m_Scene = scene;

        const auto projectRoot = FindNoJobProjectRoot();
        AssetManager::Init(projectRoot);
        std::filesystem::create_directories(
            AssetManager::GetAssetsDirectory() / "Scripts");
        m_ProjectDirectory = AssetManager::GetProjectRoot();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Keep the editor layout in the repository root instead of out/build.
        // Deleting the CMake build directory will therefore not erase it.
        static std::string imguiIniPath =
            (AssetManager::GetProjectRoot() / "NoJobEngineLayout.ini").string();
        io.IniFilename = imguiIniPath.c_str();

        // A window-position-only ini is not enough: we need an actual docking
        // tree. This also repairs ini files produced by the previous broken
        // default-layout implementation.
        s_BuildDefaultDockLayout = true;
        if (std::filesystem::exists(imguiIniPath))
        {
            std::ifstream iniFile(imguiIniPath);
            const std::string iniContents(
                (std::istreambuf_iterator<char>(iniFile)),
                std::istreambuf_iterator<char>());
            s_BuildDefaultDockLayout =
                iniContents.find("[Docking][Data]") == std::string::npos;
        }

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460");
    }

    void EditorLayer::Shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void EditorLayer::BeginFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImGuiID dockspaceID = ImGui::GetID("NoJobEngineDockSpace");

        ImGui::DockSpaceOverViewport(
            dockspaceID,
            viewport,
            ImGuiDockNodeFlags_PassthruCentralNode);

        // Build only when Init found no valid saved docking data.
        // Do not test DockBuilderGetNode here: DockSpaceOverViewport has
        // already created that node by this point.
        if (s_BuildDefaultDockLayout)
        {
            s_BuildDefaultDockLayout = false;
            ImGui::DockBuilderRemoveNode(dockspaceID);
            ImGui::DockBuilderAddNode(
                dockspaceID,
                ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::DockBuilderSetNodeSize(
                dockspaceID,
                viewport->WorkSize);

            ImGuiID mainID = dockspaceID;

            // Left: narrow Hierarchy.
            ImGuiID leftID = ImGui::DockBuilderSplitNode(
                mainID, ImGuiDir_Left, 0.075f, nullptr, &mainID);

            // Right: Inspector.
            ImGuiID rightID = ImGui::DockBuilderSplitNode(
                mainID, ImGuiDir_Right, 0.14f, nullptr, &mainID);

            // Bottom: Console + Project, leaving most space to Viewport.
            ImGuiID bottomID = ImGui::DockBuilderSplitNode(
                mainID, ImGuiDir_Down, 0.16f, nullptr, &mainID);

            ImGuiID consoleID = ImGui::DockBuilderSplitNode(
                bottomID, ImGuiDir_Left, 0.22f, nullptr, &bottomID);

            ImGui::DockBuilderDockWindow("Hierarchy", leftID);
            ImGui::DockBuilderDockWindow("Inspector", rightID);
            ImGui::DockBuilderDockWindow("Console", consoleID);
            ImGui::DockBuilderDockWindow("Project", bottomID);
            ImGui::DockBuilderDockWindow("Viewport", mainID);

            ImGui::DockBuilderFinish(dockspaceID);
        }
    }

    void EditorLayer::Draw()
    {
#ifdef _WIN32
        if (s_ProjectScriptsReloadPending.exchange(false))
        {
            if (LoadProjectScripts(AssetManager::GetProjectRoot()))
            {
                s_ScriptsCompiled = true;
                ScriptLog("[Scripts] Hot reload completed successfully.");
            }
            else
            {
                s_ScriptsCompiled = false;
                ScriptLog("[Scripts] Build succeeded, but DLL reload failed.");
            }
        }
#endif
        DrawPlayToolbar();
#ifdef _WIN32
        if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift &&
            ImGui::IsKeyPressed(ImGuiKey_B, false))
            CompileProjectScripts();
#endif
        DrawMainMenu();
        DrawHierarchy();
        DrawViewport();
        DrawInspector();
        DrawConsole();
        DrawProjectPanel();
        if (m_ShowGraphicsSettings)
            DrawGraphicsSettings();
        DrawRendererProfiler();

        if (m_SelectedEntity && ImGui::IsKeyPressed(ImGuiKey_Delete))
            DeleteSelectedEntity();

        const ImGuiIO& io = ImGui::GetIO();

        if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            if (io.KeyShift)
                Redo();
            else
                Undo();
        }

        if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
            Redo();

        if (m_SelectedEntity
            && io.KeyCtrl
            && ImGui::IsKeyPressed(ImGuiKey_D))
        {
            DuplicateSelectedEntity();
        }

        if (!io.WantTextInput && m_SelectedEntity && ImGui::IsKeyPressed(ImGuiKey_F2))
            m_RenameSelectedRequested = true;

        if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape))
            ClearMultiSelection();
    }

    void EditorLayer::EndFrame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void EditorLayer::SetSelectedEntity(Entity entity)
    {
        m_SelectedEntity = entity;
    }

    void EditorLayer::SetViewportTexture(std::uint32_t textureID)
    {
        m_ViewportTextureID = textureID;
    }

    void EditorLayer::SetEditorCameraMatrices(
        const glm::mat4& view,
        const glm::mat4& projection)
    {
        m_EditorView = view;
        m_EditorProjection = projection;
    }

    void EditorLayer::SetDefaultCubeAssets(
        std::shared_ptr<Mesh> mesh,
        std::shared_ptr<Material> material)
    {
        m_DefaultCubeMesh = std::move(mesh);
        m_DefaultCubeMaterial = std::move(material);
    }

    std::uint32_t EditorLayer::GetViewportWidth() const
    {
        return static_cast<std::uint32_t>(
            std::max(1.0f, m_ViewportWidth));
    }

    std::uint32_t EditorLayer::GetViewportHeight() const
    {
        return static_cast<std::uint32_t>(
            std::max(1.0f, m_ViewportHeight));
    }

    Entity EditorLayer::CreateEmptyEntity()
    {
        if (!m_Scene)
            return {};

        CaptureUndoSnapshot();
        Entity entity = m_Scene->CreateEntity("Empty Entity");
        m_SelectedEntity = entity;
        return entity;
    }

    Entity EditorLayer::CreateCubeEntity()
    {
        if (!m_Scene)
            return {};

        CaptureUndoSnapshot();
        Entity entity = m_Scene->CreateEntity("Cube");

        if (m_DefaultCubeMesh)
            entity.AddComponent<MeshComponent>(m_DefaultCubeMesh);

        if (m_DefaultCubeMaterial)
        {
            // Give every cube its own Material instance so editing one
            // entity's color does not recolor all cubes.
            auto material = std::make_shared<Material>(
                m_DefaultCubeMaterial->GetShader(),
                m_DefaultCubeMaterial->GetColor());

            material->SetTexture(
                m_DefaultCubeMaterial->GetTexture());
            material->UseTexture() =
                m_DefaultCubeMaterial->UseTexture();
            material->Metallic() = m_DefaultCubeMaterial->Metallic();
            material->Roughness() = m_DefaultCubeMaterial->Roughness();
            material->AmbientOcclusion() = m_DefaultCubeMaterial->AmbientOcclusion();
            material->NormalStrength() = m_DefaultCubeMaterial->NormalStrength();
            material->EmissiveColor() = m_DefaultCubeMaterial->EmissiveColor();
            material->EmissiveStrength() = m_DefaultCubeMaterial->EmissiveStrength();
            material->SetNormalTexture(m_DefaultCubeMaterial->GetNormalTexture());
            material->SetMetallicTexture(m_DefaultCubeMaterial->GetMetallicTexture());
            material->SetRoughnessTexture(m_DefaultCubeMaterial->GetRoughnessTexture());
            material->SetAOTexture(m_DefaultCubeMaterial->GetAOTexture());
            material->SetEmissiveTexture(m_DefaultCubeMaterial->GetEmissiveTexture());

            entity.AddComponent<MeshRendererComponent>(material);
        }

        m_SelectedEntity = entity;
        return entity;
    }

    Entity EditorLayer::CreateModelEntity(const std::filesystem::path& p)
    {
        if (!m_Scene || p.empty())
            return {};

        try
        {
            CaptureUndoSnapshot();

            auto mesh = AssetManager::LoadMesh(p);
            auto entity = m_Scene->CreateEntity(p.stem().string());
            entity.AddComponent<MeshComponent>(mesh);

            std::shared_ptr<Material> material;
            if (m_DefaultCubeMaterial)
            {
                auto materials=AssetManager::ImportModelMaterials(
                    p,m_DefaultCubeMaterial->GetShader());
                if(materials.empty())
                {
                    material=std::make_shared<Material>(*m_DefaultCubeMaterial);
                    material->UseTexture()=false;
                    materials.push_back(material);
                }
                else material=materials.front();

                entity.AddComponent<MeshRendererComponent>(material);
                entity.GetComponent<MeshRendererComponent>()
                    .SetMaterials(std::move(materials));
            }

            // Unity-style import: if the model contains an Assimp skeleton/animation,
            // automatically attach an Animator so the clips are immediately visible.
            try
            {
                auto animation = AnimationAsset::Load(p);
                if (animation && animation->HasAnimations())
                {
                    AnimatorComponent animator;
                    animator.Animation = std::move(animation);
                    animator.ClipIndex = 0;
                    animator.TimeSeconds = 0.0f;
                    animator.Speed = 1.0f;
                    animator.Playing = true;
                    animator.Loop = true;
                    entity.AddComponent<AnimatorComponent>(std::move(animator));
                }
            }
            catch (...) {}

            m_SelectedEntity = entity;
            return entity;
        }
        catch (...)
        {
            return {};
        }
    }

    void EditorLayer::ClearRedoHistory()
    {
        m_RedoHistory.clear();
    }

    void EditorLayer::PushUndoSnapshot(std::unique_ptr<Scene> snapshot)
    {
        if (!snapshot)
            return;

        m_UndoHistory.push_back(std::move(snapshot));
        if (m_UndoHistory.size() > MaxHistoryEntries)
            m_UndoHistory.erase(m_UndoHistory.begin());

        ClearRedoHistory();
    }

    void EditorLayer::CaptureUndoSnapshot()
    {
        if (m_Scene && !m_IsPlaying)
            PushUndoSnapshot(m_Scene->Copy());
    }

    void EditorLayer::Undo()
    {
        if (!m_Scene || m_IsPlaying || m_UndoHistory.empty())
            return;

        const std::uint32_t selected =
            m_SelectedEntity ? m_SelectedEntity.GetHandle() : 0;

        m_RedoHistory.push_back(m_Scene->Copy());
        m_Scene->RestoreFrom(*m_UndoHistory.back());
        m_UndoHistory.pop_back();

        m_SelectedEntity =
            selected != 0 && m_Scene->IsValid(selected)
                ? Entity(selected, m_Scene)
                : Entity{};
    }

    void EditorLayer::Redo()
    {
        if (!m_Scene || m_IsPlaying || m_RedoHistory.empty())
            return;

        const std::uint32_t selected =
            m_SelectedEntity ? m_SelectedEntity.GetHandle() : 0;

        m_UndoHistory.push_back(m_Scene->Copy());
        m_Scene->RestoreFrom(*m_RedoHistory.back());
        m_RedoHistory.pop_back();

        m_SelectedEntity =
            selected != 0 && m_Scene->IsValid(selected)
                ? Entity(selected, m_Scene)
                : Entity{};
    }

    void EditorLayer::DeleteSelectedEntity()
    {
        if (!m_Scene || !m_SelectedEntity)
            return;

        std::vector<Entity> selectedEntities;
        for (Entity entity : m_Scene->GetEntities())
        {
            if (IsMultiSelected(entity))
                selectedEntities.push_back(entity);
        }

        if (selectedEntities.empty())
            selectedEntities.push_back(m_SelectedEntity);

        // Keep only selected roots. If a child is also selected, its selected
        // ancestor owns the operation for that whole branch.
        std::vector<Entity> selectedRoots;
        for (Entity candidate : selectedEntities)
        {
            bool hasSelectedAncestor = false;
            Entity parent = m_Scene->GetParent(candidate);
            while (parent)
            {
                if (IsMultiSelected(parent))
                {
                    hasSelectedAncestor = true;
                    break;
                }
                parent = m_Scene->GetParent(parent);
            }

            if (!hasSelectedAncestor)
                selectedRoots.push_back(candidate);
        }

        CaptureUndoSnapshot();

        // Destroy descendants first, then the root. This makes hierarchy
        // deletion independent from whether Scene::DestroyEntity itself is
        // recursive and prevents orphaned child entities.
        auto destroyHierarchy = [&](auto&& self, Entity entity) -> void
        {
            if (!entity || !m_Scene->IsValid(entity.GetHandle()))
                return;

            const auto children = m_Scene->GetChildren(entity);
            for (Entity child : children)
                self(self, child);

            if (m_Scene->IsValid(entity.GetHandle()))
                m_Scene->DestroyEntity(entity);
        };

        for (Entity root : selectedRoots)
            destroyHierarchy(destroyHierarchy, root);

        ClearMultiSelection();
    }

    void EditorLayer::DuplicateSelectedEntity()
    {
        if (!m_Scene || !m_SelectedEntity)
            return;

        std::vector<Entity> selectedEntities;
        for (Entity entity : m_Scene->GetEntities())
        {
            if (IsMultiSelected(entity))
                selectedEntities.push_back(entity);
        }

        if (selectedEntities.empty())
            selectedEntities.push_back(m_SelectedEntity);

        // Duplicate only selected roots. A selected descendant will already be
        // duplicated recursively with its selected ancestor.
        std::vector<Entity> selectedRoots;
        for (Entity candidate : selectedEntities)
        {
            bool hasSelectedAncestor = false;
            Entity parent = m_Scene->GetParent(candidate);
            while (parent)
            {
                if (IsMultiSelected(parent))
                {
                    hasSelectedAncestor = true;
                    break;
                }
                parent = m_Scene->GetParent(parent);
            }

            if (!hasSelectedAncestor)
                selectedRoots.push_back(candidate);
        }

        CaptureUndoSnapshot();

        auto copyComponents = [&](Entity source, Entity copy)
        {
            copy.GetComponent<TransformComponent>() =
                source.GetComponent<TransformComponent>();

            if (source.HasComponent<MeshComponent>())
                copy.AddComponent<MeshComponent>(
                    source.GetComponent<MeshComponent>().MeshAsset);

            if (source.HasComponent<MeshRendererComponent>())
            {
                const auto& src =
                    source.GetComponent<MeshRendererComponent>();
                auto& dst =
                    copy.AddComponent<MeshRendererComponent>(src.MaterialAsset);
                dst.MaterialAsset = src.MaterialAsset;
                dst.Materials = src.Materials;
            }

            if (source.HasComponent<NativeScriptComponent>())
                copy.AddComponent<NativeScriptComponent>(
                    source.GetComponent<NativeScriptComponent>());
            if (source.HasComponent<RigidbodyComponent>())
                copy.AddComponent<RigidbodyComponent>(
                    source.GetComponent<RigidbodyComponent>());
            if (source.HasComponent<BoxColliderComponent>())
                copy.AddComponent<BoxColliderComponent>(
                    source.GetComponent<BoxColliderComponent>());
            if (source.HasComponent<SphereColliderComponent>())
                copy.AddComponent<SphereColliderComponent>(
                    source.GetComponent<SphereColliderComponent>());
            if (source.HasComponent<CapsuleColliderComponent>())
                copy.AddComponent<CapsuleColliderComponent>(
                    source.GetComponent<CapsuleColliderComponent>());
            if (source.HasComponent<CameraComponent>())
                copy.AddComponent<CameraComponent>(
                    source.GetComponent<CameraComponent>());
            if (source.HasComponent<DirectionalLightComponent>())
                copy.AddComponent<DirectionalLightComponent>(
                    source.GetComponent<DirectionalLightComponent>());
            if (source.HasComponent<PointLightComponent>())
                copy.AddComponent<PointLightComponent>(
                    source.GetComponent<PointLightComponent>());
            if (source.HasComponent<SpotLightComponent>())
                copy.AddComponent<SpotLightComponent>(
                    source.GetComponent<SpotLightComponent>());
            if (source.HasComponent<AnimatorComponent>())
                copy.AddComponent<AnimatorComponent>(
                    source.GetComponent<AnimatorComponent>());
            if (source.HasComponent<PrefabInstanceComponent>())
                copy.AddComponent<PrefabInstanceComponent>(
                    source.GetComponent<PrefabInstanceComponent>());
        };

        // Recursively clone a complete hierarchy. Parenting uses
        // keepWorldTransform=false because copied transforms are already local
        // transforms from the original hierarchy.
        auto duplicateHierarchy =
            [&](auto&& self, Entity source, Entity newParent) -> Entity
        {
            const auto sourceTag =
                source.GetComponent<TagComponent>().Tag;

            Entity copy = m_Scene->CreateEntity(sourceTag + " Copy");
            copyComponents(source, copy);

            if (newParent)
                m_Scene->SetParent(copy, newParent, false);

            const auto children = m_Scene->GetChildren(source);
            for (Entity child : children)
                self(self, child, copy);

            return copy;
        };

        std::vector<std::uint32_t> newSelection;
        Entity lastCopy{};

        for (Entity sourceRoot : selectedRoots)
        {
            if (!sourceRoot || !m_Scene->IsValid(sourceRoot.GetHandle()))
                continue;

            // Preserve the original external parent for a duplicated root.
            Entity originalParent = m_Scene->GetParent(sourceRoot);
            Entity rootCopy =
                duplicateHierarchy(duplicateHierarchy, sourceRoot, Entity{});

            if (originalParent)
                m_Scene->SetParent(rootCopy, originalParent, false);

            newSelection.push_back(rootCopy.GetHandle());
            lastCopy = rootCopy;
        }

        m_MultiSelection = std::move(newSelection);
        m_SelectedEntity = lastCopy;
    }

    void EditorLayer::DrawMainMenu()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                    m_SaveSceneRequested = true;
                if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
                    m_LoadSceneRequested = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Load Graphics Test Scene"))
                    m_GraphicsTestSceneRequested = true;
                ImGui::Separator();
                if(ImGui::MenuItem("Import 3D Model...")){auto s=OpenModelFileDialog();if(!s.empty())try{CreateModelEntity(AssetManager::ImportModel(s));}catch(...){}}
                ImGui::Separator();
                ImGui::MenuItem("Exit");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem(
                        "Duplicate Entity",
                        "Ctrl+D",
                        false,
                        static_cast<bool>(m_SelectedEntity)))
                {
                    DuplicateSelectedEntity();
                }

                if (ImGui::MenuItem(
                        "Delete Entity",
                        "Delete",
                        false,
                        static_cast<bool>(m_SelectedEntity)))
                {
                    DeleteSelectedEntity();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                ImGui::BeginDisabled(m_UndoHistory.empty() || m_IsPlaying);
                if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                    Undo();
                ImGui::EndDisabled();

                ImGui::BeginDisabled(m_RedoHistory.empty() || m_IsPlaying);
                if (ImGui::MenuItem("Redo", "Ctrl+Y"))
                    Redo();
                ImGui::EndDisabled();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Build"))
            {
#ifdef _WIN32
                ImGui::BeginDisabled(s_ScriptCompileRunning.load());
                if (ImGui::MenuItem("Compile Scripts", "Ctrl+Shift+B"))
                    CompileProjectScripts();
                ImGui::EndDisabled();
                ImGui::BeginDisabled(s_ScriptCompileRunning.load());
                if (ImGui::MenuItem("Reload Scripts"))
                    LoadProjectScripts(AssetManager::GetProjectRoot());
                ImGui::EndDisabled();
                if (s_ScriptCompileRunning.load())
                    ImGui::TextDisabled("Compiling scripts...");
                ImGui::Separator();
                ImGui::TextDisabled(s_ProjectScriptsModule
                    ? "ProjectScripts.dll loaded"
                    : "ProjectScripts.dll not loaded");
#else
                ImGui::TextDisabled("Script compilation is currently implemented for Windows.");
#endif
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("GameObject"))
            {
                if (ImGui::MenuItem("Create Empty"))
                    CreateEmptyEntity();

                if (ImGui::BeginMenu("3D Object"))
                {
                    if (ImGui::MenuItem("Cube"))
                        CreateCubeEntity();

                    if (ImGui::MenuItem("Import 3D Model..."))
                    {
                        const std::string source = OpenModelFileDialog();
                        if (!source.empty())
                        {
                            try
                            {
                                const auto imported = AssetManager::ImportModel(source);
                                CreateModelEntity(imported);
                            }
                            catch (...) {}
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Camera") && m_Scene)
                {
                    CaptureUndoSnapshot();
                    Entity camera = m_Scene->CreateEntity("Camera");
                    camera.AddComponent<CameraComponent>();
                    m_SelectedEntity = camera;
                }

                if (m_SelectedEntity && ImGui::MenuItem("Create Prefab From Selected"))
                {
                    const auto prefabPath =
                        AssetManager::GetAssetsDirectory() / "Prefabs" /
                        (m_SelectedEntity.GetComponent<TagComponent>().Tag + ".nojobprefab");

                    if (PrefabSerializer::Save(m_SelectedEntity, prefabPath))
                    {
                        AssetRegistry registry(AssetManager::GetAssetsDirectory());
                        registry.Load();
                        registry.Register(prefabPath, AssetType::Prefab);
                        registry.Save();
                    }
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Light"))
                {
                    if (ImGui::MenuItem("Directional Light") && m_Scene)
                    {
                        CaptureUndoSnapshot();
                    Entity light = m_Scene->CreateEntity("Directional Light");
                        light.AddComponent<DirectionalLightComponent>();
                        m_SelectedEntity = light;
                    }
                    if (ImGui::MenuItem("Point Light") && m_Scene)
                    {
                        CaptureUndoSnapshot();
                    Entity light = m_Scene->CreateEntity("Point Light");
                        light.AddComponent<PointLightComponent>();
                        m_SelectedEntity = light;
                    }
                    if (ImGui::MenuItem("Spot Light") && m_Scene)
                    {
                        CaptureUndoSnapshot();
                    Entity light = m_Scene->CreateEntity("Spot Light");
                        light.AddComponent<SpotLightComponent>();
                        m_SelectedEntity = light;
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Hierarchy");
                ImGui::MenuItem("Inspector");
                ImGui::MenuItem("Console");
                ImGui::MenuItem("Graphics Settings", nullptr, &m_ShowGraphicsSettings);
                ImGui::MenuItem("Renderer Profiler", nullptr, &m_ShowRendererProfiler);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void EditorLayer::SetScene(Scene* scene)
    {
        m_Scene = scene;
        m_SelectedEntity = {};
    }

    bool EditorLayer::ConsumePlayRequest()
    {
        const bool value = m_PlayRequested;
        m_PlayRequested = false;
        return value;
    }

    bool EditorLayer::ConsumePauseRequest()
    {
        const bool value = m_PauseRequested;
        m_PauseRequested = false;
        return value;
    }

    bool EditorLayer::ConsumeStopRequest()
    {
        const bool value = m_StopRequested;
        m_StopRequested = false;
        return value;
    }

    void EditorLayer::SetRuntimeState(bool playing, bool paused)
    {
        m_IsPlaying = playing;
        m_IsPaused = paused;
    }

    void EditorLayer::DrawPlayToolbar()
    {
        ImGui::Begin("Toolbar", nullptr,
            ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse);

        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(
            ImGui::GetCursorPosX() + (width - 170.0f) * 0.5f);

        ImGui::BeginDisabled(m_IsPlaying);
        if (ImGui::Button("Play", ImVec2(50.0f, 0.0f)))
            m_PlayRequested = true;
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!m_IsPlaying);
        if (ImGui::Button(
                m_IsPaused ? "Resume" : "Pause",
                ImVec2(60.0f, 0.0f)))
        {
            m_PauseRequested = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!m_IsPlaying);
        if (ImGui::Button("Stop", ImVec2(50.0f, 0.0f)))
            m_StopRequested = true;
        ImGui::EndDisabled();

        ImGui::End();
    }

    bool EditorLayer::IsMultiSelected(Entity entity) const
    {
        return std::find(m_MultiSelection.begin(), m_MultiSelection.end(),
            entity.GetHandle()) != m_MultiSelection.end();
    }

    void EditorLayer::ClearMultiSelection()
    {
        m_MultiSelection.clear();
        m_SelectedEntity = {};
    }

    bool EditorLayer::HierarchyMatchesFilter(Entity entity) const
    {
        if (!entity) return false;
        std::string filter = m_HierarchySearch;
        if (filter.empty()) return true;
        std::transform(filter.begin(), filter.end(), filter.begin(),
            [](unsigned char c){ return (char)std::tolower(c); });

        std::string name = entity.GetComponent<TagComponent>().Tag;
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char c){ return (char)std::tolower(c); });
        if (name.find(filter) != std::string::npos) return true;

        for (Entity child : m_Scene->GetChildren(entity))
            if (HierarchyMatchesFilter(child)) return true;
        return false;
    }

    void EditorLayer::DrawHierarchy()
    {
        ImGui::Begin("Hierarchy");

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##HierarchySearch", "Search Hierarchy...",
            m_HierarchySearch, sizeof(m_HierarchySearch));

        if (m_MultiSelection.size() > 1)
        {
            ImGui::Text("%zu entities selected", m_MultiSelection.size());
            if (ImGui::SmallButton("Duplicate Selected"))
                DuplicateSelectedEntity();
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete Selected"))
                DeleteSelectedEntity();
        }

        ImGui::Separator();

        if (ImGui::BeginPopupContextWindow(
                "HierarchyContext",
                ImGuiPopupFlags_MouseButtonRight
                | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty"))
                CreateEmptyEntity();

            if (ImGui::BeginMenu("3D Object"))
            {
                if (ImGui::MenuItem("Cube"))
                    CreateCubeEntity();
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        // Dropping an entity onto empty Hierarchy space makes it a root.
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("NOJOB_ENTITY"))
            {
                const auto handle =
                    *static_cast<const std::uint32_t*>(payload->Data);

                if (m_Scene->IsValid(handle))
                {
                    CaptureUndoSnapshot();
                    const bool dragIsMulti =
                        std::find(m_MultiSelection.begin(), m_MultiSelection.end(), handle)
                        != m_MultiSelection.end()
                        && m_MultiSelection.size() > 1;

                    if (dragIsMulti)
                    {
                        for (std::uint32_t selectedHandle : m_MultiSelection)
                            if (m_Scene->IsValid(selectedHandle))
                                m_Scene->Unparent(Entity(selectedHandle, m_Scene), true);
                    }
                    else
                    {
                        m_Scene->Unparent(Entity(handle, m_Scene), true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (m_Scene)
        {
            for (Entity entity : m_Scene->GetEntities())
            {
                if (entity.GetComponent<RelationshipComponent>().Parent == 0
                    && HierarchyMatchesFilter(entity))
                    DrawEntityNode(entity);
            }
        }

        ImGui::End();
    }

    void EditorLayer::DrawEntityNode(Entity entity)
    {
        if (!entity)
            return;

        auto& tag = entity.GetComponent<TagComponent>().Tag;
        const auto& relationship =
            entity.GetComponent<RelationshipComponent>();

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (relationship.Children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;

        if (entity == m_SelectedEntity || IsMultiSelected(entity))
            flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID(static_cast<int>(entity.GetHandle()));

        const bool open =
            ImGui::TreeNodeEx("EntityNode", flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked())
        {
            const bool ctrl = ImGui::GetIO().KeyCtrl;
            if (ctrl)
            {
                auto it = std::find(m_MultiSelection.begin(), m_MultiSelection.end(),
                    entity.GetHandle());
                if (it == m_MultiSelection.end())
                    m_MultiSelection.push_back(entity.GetHandle());
                else
                    m_MultiSelection.erase(it);
            }
            else
            {
                m_MultiSelection.clear();
                m_MultiSelection.push_back(entity.GetHandle());
            }
            m_SelectedEntity = entity;
        }

        if (ImGui::BeginDragDropSource())
        {
            const std::uint32_t handle = entity.GetHandle();
            ImGui::SetDragDropPayload(
                "NOJOB_ENTITY",
                &handle,
                sizeof(handle));
            ImGui::Text("%s", tag.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("NOJOB_ENTITY"))
            {
                const auto childHandle =
                    *static_cast<const std::uint32_t*>(payload->Data);

                if (m_Scene->IsValid(childHandle))
                {
                    CaptureUndoSnapshot();

                    const bool dragIsMulti =
                        std::find(m_MultiSelection.begin(), m_MultiSelection.end(), childHandle)
                        != m_MultiSelection.end()
                        && m_MultiSelection.size() > 1;

                    if (dragIsMulti)
                    {
                        // Parent every selected entity that can legally become
                        // a child of the drop target. Scene::SetParent keeps the
                        // world transform and rejects invalid cycles.
                        for (std::uint32_t selectedHandle : m_MultiSelection)
                        {
                            if (!m_Scene->IsValid(selectedHandle)
                                || selectedHandle == entity.GetHandle())
                                continue;

                            m_Scene->SetParent(
                                Entity(selectedHandle, m_Scene),
                                entity,
                                true);
                        }
                    }
                    else
                    {
                        m_Scene->SetParent(
                            Entity(childHandle, m_Scene),
                            entity,
                            true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("EntityContext"))
        {
            m_SelectedEntity = entity;

            if (ImGui::MenuItem("Rename", "F2"))
                m_RenameSelectedRequested = true;

            if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                DuplicateSelectedEntity();

            if (ImGui::MenuItem("Create Prefab"))
            {
                const auto prefabPath =
                    AssetManager::GetAssetsDirectory() / "Prefabs" /
                    (entity.GetComponent<TagComponent>().Tag + ".nojobprefab");

                if (PrefabSerializer::Save(entity, prefabPath))
                {
                    AssetRegistry registry(AssetManager::GetAssetsDirectory());
                    registry.Load();
                    registry.Register(prefabPath, AssetType::Prefab);
                    registry.Save();
                }
            }

            if (relationship.Parent != 0
                && ImGui::MenuItem("Unparent"))
            {
                CaptureUndoSnapshot();
                m_Scene->Unparent(entity, true);
            }

            if (ImGui::MenuItem("Delete", "Delete"))
                DeleteSelectedEntity();

            ImGui::EndPopup();
        }

        if (open)
        {
            const auto children = m_Scene->GetChildren(entity);
            for (Entity child : children)
                if (HierarchyMatchesFilter(child))
                    DrawEntityNode(child);

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void EditorLayer::DrawInspector()
    {
        ImGui::Begin("Inspector");

        if (m_SelectedEntity)
        {
            auto& tag =
                m_SelectedEntity.GetComponent<TagComponent>().Tag;

            char tagBuffer[256]{};
            std::strncpy(
                tagBuffer,
                tag.c_str(),
                sizeof(tagBuffer) - 1);

            if (m_RenameSelectedRequested)
            {
                ImGui::SetKeyboardFocusHere();
                m_RenameSelectedRequested = false;
            }
            if (ImGui::InputText(
                    "##EntityName",
                    tagBuffer,
                    sizeof(tagBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue))
            {
                tag = tagBuffer;
            }
            if (m_MultiSelection.size() > 1)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%zu selected", m_MultiSelection.size());
            }

            ImGui::Separator();

            if (ImGui::CollapsingHeader(
                    "Transform",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& transform =
                    m_SelectedEntity.GetComponent<TransformComponent>();

                // Unity-style numeric transform fields with Undo/Redo transactions.
                auto editTransform = [&](const char* label, glm::vec3& value)
                {
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::DragFloat3(
                        label,
                        &value.x,
                        0.01f,
                        0.0f,
                        0.0f,
                        "%.3f");

                    if (ImGui::IsItemActivated() && !m_TransformEditSnapshot)
                        m_TransformEditSnapshot = m_Scene->Copy();

                    if (ImGui::IsItemDeactivatedAfterEdit()
                        && m_TransformEditSnapshot)
                    {
                        PushUndoSnapshot(std::move(m_TransformEditSnapshot));
                    }
                    else if (ImGui::IsItemDeactivated()
                             && m_TransformEditSnapshot)
                    {
                        m_TransformEditSnapshot.reset();
                    }
                };

                editTransform("Position", transform.Position);
                editTransform("Rotation", transform.Rotation);
                editTransform("Scale", transform.Scale);

                ImGui::TextDisabled(
                    "Ctrl + click to type | Ctrl+Z / Ctrl+Y to undo/redo.");
            }

            {
                Entity parent = m_Scene->GetParent(m_SelectedEntity);
                if (parent)
                {
                    ImGui::TextDisabled(
                        "Parent: %s",
                        parent.GetComponent<TagComponent>().Tag.c_str());

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Unparent"))
                    {
                        CaptureUndoSnapshot();
                        m_Scene->Unparent(m_SelectedEntity, true);
                    }
                }
                else
                {
                    ImGui::TextDisabled("Parent: None");
                }
            }

            if (m_SelectedEntity.HasComponent<MeshComponent>())
            {
                ImGui::Separator();

                if (ImGui::CollapsingHeader(
                        "Mesh",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextDisabled("Primitive Mesh");
                }
            }

            ImGui::Separator();
            if (m_SelectedEntity.HasComponent<NativeScriptComponent>())
            {
                auto& script =
                    m_SelectedEntity.GetComponent<NativeScriptComponent>();
                RegisterBuiltinScripts();
                ScriptRegistry::ApplyDefaults(script);

                const std::string header =
                    "Native Script: " + script.ScriptName;
                if (ImGui::CollapsingHeader(
                        header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto beginEdit = [&]()
                    {
                        if (ImGui::IsItemActivated() &&
                            !m_ScriptFieldEditSnapshot)
                            m_ScriptFieldEditSnapshot = m_Scene->Copy();
                    };
                    auto endEdit = [&]()
                    {
                        if (ImGui::IsItemDeactivatedAfterEdit() &&
                            m_ScriptFieldEditSnapshot)
                            PushUndoSnapshot(
                                std::move(m_ScriptFieldEditSnapshot));
                        else if (ImGui::IsItemDeactivated() &&
                                 m_ScriptFieldEditSnapshot)
                            m_ScriptFieldEditSnapshot.reset();
                    };

                    ImGui::Checkbox(
                        "Enabled##NativeScript", &script.Enabled);
                    beginEdit(); endEdit();
                    ImGui::TextDisabled("C++ Native Script");

                    const auto* definition =
                        ScriptRegistry::Find(script.ScriptName);
                    if (!definition)
                    {
                        ImGui::TextDisabled(
                            "Script definition not loaded. Compile Scripts.");
                    }
                    else
                    {
                        for (const auto& fieldDef : definition->Fields)
                        {
                            auto it = script.Fields.find(fieldDef.Name);
                            if (it == script.Fields.end()) continue;
                            auto& field = it->second;

                            ImGui::PushID(fieldDef.Name.c_str());
                            if (field.Type == ScriptFieldType::Float)
                                ImGui::DragFloat(
                                    fieldDef.Name.c_str(), &field.Float, 0.05f);
                            else if (field.Type == ScriptFieldType::Int)
                                ImGui::DragInt(
                                    fieldDef.Name.c_str(), &field.Int);
                            else if (field.Type == ScriptFieldType::Bool)
                                ImGui::Checkbox(
                                    fieldDef.Name.c_str(), &field.Bool);
                            else if (field.Type == ScriptFieldType::Vec3)
                                ImGui::DragFloat3(
                                    fieldDef.Name.c_str(), &field.Vec3.x, 0.05f);
                            beginEdit(); endEdit();
                            ImGui::PopID();
                        }
                        ImGui::TextDisabled(
                            "Scene/Prefab persistent | Hot-reload safe");
                    }
                }
            }

            ImGui::Separator();
            if (m_SelectedEntity.HasComponent<RigidbodyComponent>())
            {
                if (ImGui::CollapsingHeader(
                        "Rigidbody",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& body =
                        m_SelectedEntity.GetComponent<RigidbodyComponent>();

                    const char* bodyTypes[] = {
                        "Static", "Dynamic", "Kinematic"
                    };
                    int type = static_cast<int>(body.Type);
                    if (ImGui::Combo(
                            "Body Type",
                            &type,
                            bodyTypes,
                            IM_ARRAYSIZE(bodyTypes)))
                    {
                        body.Type = static_cast<RigidbodyType>(type);
                    }

                    if (body.Type == RigidbodyType::Dynamic)
                    {
                        ImGui::DragFloat(
                            "Mass",
                            &body.Mass,
                            0.05f,
                            0.001f,
                            10000.0f);
                        ImGui::Checkbox("Use Gravity", &body.UseGravity);
                    }
                    else if (body.Type == RigidbodyType::Kinematic)
                    {
                        ImGui::Checkbox("Use Gravity", &body.UseGravity);
                        ImGui::TextDisabled(
                            "Kinematic motion control comes in a later step.");
                    }

                    if (ImGui::Button("Remove Rigidbody"))
                        m_SelectedEntity.RemoveComponent<RigidbodyComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<BoxColliderComponent>())
            {
                if (ImGui::CollapsingHeader(
                        "Box Collider",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& c = m_SelectedEntity.GetComponent<BoxColliderComponent>();
                    ImGui::DragFloat3("Size##Box", &c.Size.x, 0.05f, 0.01f, 1000.0f);
                    c.Size = glm::max(c.Size, glm::vec3(0.01f));
                    ImGui::Checkbox("Is Trigger##Box", &c.IsTrigger);
                    ImGui::SliderFloat("Friction##Box", &c.Material.Friction, 0.0f, 1.0f);
                    ImGui::SliderFloat("Bounciness##Box", &c.Material.Bounciness, 0.0f, 1.0f);
                    if (ImGui::Button("Remove Box Collider"))
                        m_SelectedEntity.RemoveComponent<BoxColliderComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<SphereColliderComponent>())
            {
                if (ImGui::CollapsingHeader(
                        "Sphere Collider",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& c = m_SelectedEntity.GetComponent<SphereColliderComponent>();
                    ImGui::DragFloat("Radius##Sphere", &c.Radius, 0.02f, 0.01f, 1000.0f);
                    c.Radius = std::max(c.Radius, 0.01f);
                    ImGui::Checkbox("Is Trigger##Sphere", &c.IsTrigger);
                    ImGui::SliderFloat("Friction##Sphere", &c.Material.Friction, 0.0f, 1.0f);
                    ImGui::SliderFloat("Bounciness##Sphere", &c.Material.Bounciness, 0.0f, 1.0f);
                    if (ImGui::Button("Remove Sphere Collider"))
                        m_SelectedEntity.RemoveComponent<SphereColliderComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<CapsuleColliderComponent>())
            {
                if (ImGui::CollapsingHeader(
                        "Capsule Collider",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& c = m_SelectedEntity.GetComponent<CapsuleColliderComponent>();
                    ImGui::DragFloat("Radius##Capsule", &c.Radius, 0.02f, 0.01f, 1000.0f);
                    ImGui::DragFloat("Height##Capsule", &c.Height, 0.05f, 0.02f, 1000.0f);
                    c.Radius = std::max(c.Radius, 0.01f);
                    c.Height = std::max(c.Height, c.Radius * 2.0f);
                    ImGui::Checkbox("Is Trigger##Capsule", &c.IsTrigger);
                    ImGui::SliderFloat("Friction##Capsule", &c.Material.Friction, 0.0f, 1.0f);
                    ImGui::SliderFloat("Bounciness##Capsule", &c.Material.Bounciness, 0.0f, 1.0f);
                    if (ImGui::Button("Remove Capsule Collider"))
                        m_SelectedEntity.RemoveComponent<CapsuleColliderComponent>();
                }
            }


            ImGui::Separator();

            if (m_SelectedEntity.HasComponent<ParticleSystemComponent>())
            {
                if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& p = m_SelectedEntity.GetComponent<ParticleSystemComponent>();
                    ImGui::Checkbox("Playing##Particles", &p.Playing);
                    ImGui::SameLine();
                    ImGui::Checkbox("Loop##Particles", &p.Loop);
                    ImGui::DragFloat("Duration##Particles", &p.Duration, 0.1f, 0.0f, 120.0f);
                    ImGui::SeparatorText("Main");
                    ImGui::DragFloat("Start Lifetime##Particles", &p.StartLifetime, 0.05f, 0.01f, 60.0f);
                    ImGui::SliderFloat("Lifetime Random##Particles", &p.LifetimeRandom, 0.0f, 1.0f);
                    ImGui::DragFloat("Start Speed##Particles", &p.StartSpeed, 0.05f, -100.0f, 100.0f);
                    ImGui::SliderFloat("Speed Random##Particles", &p.SpeedRandom, 0.0f, 1.0f);
                    ImGui::DragFloat("Start Size##Particles", &p.StartSize, 0.01f, 0.001f, 20.0f);
                    ImGui::SliderFloat("Size Random##Particles", &p.SizeRandom, 0.0f, 1.0f);
                    ImGui::ColorEdit4("Start Color##Particles", &p.StartColor.x);
                    ImGui::ColorEdit4("End Color##Particles", &p.EndColor.x);
                    ImGui::SliderFloat("End Size Multiplier##Particles", &p.EndSizeMultiplier, 0.0f, 4.0f);
                    ImGui::DragFloat3("Gravity##Particles", &p.Gravity.x, 0.02f);

                    ImGui::SeparatorText("Emission");
                    ImGui::DragFloat("Emission Rate##Particles", &p.EmissionRate, 0.5f, 0.0f, 10000.0f);
                    int maxParticles = static_cast<int>(p.MaxParticles);
                    if (ImGui::DragInt("Max Particles##Particles", &maxParticles, 1.0f, 1, 100000))
                        p.MaxParticles = static_cast<std::uint32_t>(std::max(maxParticles, 1));

                    ImGui::SeparatorText("Shape");
                    const char* shapes[] = {"Point", "Sphere", "Cone"};
                    int shape = static_cast<int>(p.Shape);
                    if (ImGui::Combo("Emitter Shape##Particles", &shape, shapes, 3))
                        p.Shape = static_cast<ParticleShape>(shape);
                    ImGui::DragFloat3("Direction##Particles", &p.Direction.x, 0.02f);
                    if (p.Shape == ParticleShape::Sphere)
                        ImGui::DragFloat("Sphere Radius##Particles", &p.ShapeRadius, 0.02f, 0.0f, 100.0f);
                    if (p.Shape == ParticleShape::Cone)
                        ImGui::SliderFloat("Cone Angle##Particles", &p.ConeAngle, 0.0f, 89.0f);

                    ImGui::SeparatorText("Renderer");
                    const char* blends[] = {"Alpha", "Additive"};
                    int blend = static_cast<int>(p.BlendMode);
                    if (ImGui::Combo("Blend Mode##Particles", &blend, blends, 2))
                        p.BlendMode = static_cast<ParticleBlendMode>(blend);
                    ImGui::TextWrapped("Texture: %s", p.TexturePath.empty() ? "<none>" : p.TexturePath.c_str());
                    ImGui::Button(p.TexturePath.empty() ? "Drop Texture Here##Particles" : "Texture Assigned##Particles", ImVec2(-1.0f,0.0f));
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NOJOB_TEXTURE_ASSET"))
                            p.TexturePath = static_cast<const char*>(payload->Data);
                        ImGui::EndDragDropTarget();
                    }
                    if (!p.TexturePath.empty())
                    {
                        if (ImGui::Button("Clear Texture##Particles")) p.TexturePath.clear();
                    }
                    ImGui::TextDisabled("Simulation runs in Play Mode. Rendering is instanced per emitter.");
                    if (ImGui::Button("Remove Particle System"))
                        m_SelectedEntity.RemoveComponent<ParticleSystemComponent>();
                }
            }

            ImGui::Separator();

            if (m_SelectedEntity.HasComponent<AudioSourceComponent>())
            {
                if (ImGui::CollapsingHeader("Audio Source", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& audio = m_SelectedEntity.GetComponent<AudioSourceComponent>();
                    ImGui::TextWrapped("Clip: %s", audio.ClipPath.empty() ? "<none>" : audio.ClipPath.c_str());
                    ImGui::Button(audio.ClipPath.empty() ? "Drop Audio Clip Here" : "Audio Clip Assigned", ImVec2(-1.0f, 0.0f));
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NOJOB_AUDIO_ASSET"))
                        {
                            audio.ClipPath = static_cast<const char*>(payload->Data);
                            AudioEngine::Stop(m_SelectedEntity.GetHandle());
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ImGui::Button("Load Audio Clip"))
                    {
                        const std::string path = OpenAudioFileDialog();
                        if (!path.empty())
                        {
                            std::filesystem::path selected(path);
                            std::error_code ec;
                            const auto relative = std::filesystem::relative(selected, m_ProjectDirectory, ec);
                            audio.ClipPath = (!ec && !relative.empty() && relative.generic_string().rfind("..", 0) != 0)
                                ? relative.generic_string() : selected.lexically_normal().string();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear##AudioClip")) audio.ClipPath.clear();
                    ImGui::Checkbox("Play On Awake", &audio.PlayOnAwake);
                    ImGui::Checkbox("Loop##Audio", &audio.Loop);
                    ImGui::SliderFloat("Volume##Audio", &audio.Volume, 0.0f, 1.0f);
                    ImGui::SliderFloat("Pitch##Audio", &audio.Pitch, 0.1f, 3.0f);
                    ImGui::SeparatorText("Spatial Audio");
                    ImGui::SliderFloat("Spatial Blend##Audio", &audio.SpatialBlend, 0.0f, 1.0f, "%.2f");
                    ImGui::DragFloat("Min Distance##Audio", &audio.MinDistance, 0.1f, 0.01f, 10000.0f);
                    ImGui::DragFloat("Max Distance##Audio", &audio.MaxDistance, 0.25f, 0.02f, 100000.0f);
                    ImGui::SliderFloat("Doppler Factor##Audio", &audio.DopplerFactor, 0.0f, 5.0f);
                    audio.MinDistance = std::max(0.01f, audio.MinDistance);
                    audio.MaxDistance = std::max(audio.MinDistance + 0.01f, audio.MaxDistance);
                    audio.SpatialBlend = std::clamp(audio.SpatialBlend, 0.0f, 1.0f);
                    if (audio.SpatialBlend <= 0.001f)
                        ImGui::TextDisabled("2D: position and distance attenuation are disabled.");
                    else
                        ImGui::TextDisabled("3D: source follows the entity world Transform.");
                    if (ImGui::Button("Preview Play"))
                    {
                        const glm::mat4 world = m_Scene->GetWorldTransform(m_SelectedEntity);
                        AudioEngine::Play(m_SelectedEntity.GetHandle(), audio, glm::vec3(world[3]));
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Preview Stop")) AudioEngine::Stop(m_SelectedEntity.GetHandle());
                    if (!AudioEngine::GetLastError().empty())
                        ImGui::TextWrapped("Audio: %s", AudioEngine::GetLastError().c_str());
                    if (ImGui::Button("Remove Audio Source"))
                    {
                        AudioEngine::Stop(m_SelectedEntity.GetHandle());
                        m_SelectedEntity.RemoveComponent<AudioSourceComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<AudioListenerComponent>())
            {
                if (ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& listener = m_SelectedEntity.GetComponent<AudioListenerComponent>();
                    ImGui::Checkbox("Enabled##AudioListener", &listener.Enabled);
                    ImGui::TextDisabled("Uses this entity's world position and orientation.");
                    ImGui::TextDisabled("The first enabled listener in the runtime scene is active.");
                    if (ImGui::Button("Remove Audio Listener"))
                        m_SelectedEntity.RemoveComponent<AudioListenerComponent>();
                }
            }

            if (m_SelectedEntity.HasComponent<CameraComponent>())
            {
                if (ImGui::CollapsingHeader(
                        "Camera",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& camera =
                        m_SelectedEntity.GetComponent<CameraComponent>();

                    ImGui::Checkbox("Primary", &camera.Primary);

                    const char* projectionTypes[] =
                        { "Perspective", "Orthographic" };
                    int projectionType =
                        static_cast<int>(camera.ProjectionType);
                    if (ImGui::Combo(
                            "Projection",
                            &projectionType,
                            projectionTypes,
                            IM_ARRAYSIZE(projectionTypes)))
                    {
                        camera.ProjectionType =
                            static_cast<CameraProjectionType>(projectionType);
                    }

                    if (camera.ProjectionType ==
                        CameraProjectionType::Perspective)
                    {
                        ImGui::SliderFloat(
                            "Field of View",
                            &camera.PerspectiveFOV,
                            1.0f, 179.0f);
                        ImGui::DragFloat(
                            "Near Clip",
                            &camera.PerspectiveNear,
                            0.01f, 0.001f, 100.0f);
                        ImGui::DragFloat(
                            "Far Clip",
                            &camera.PerspectiveFar,
                            1.0f, 1.0f, 100000.0f);
                        camera.PerspectiveFar =
                            std::max(
                                camera.PerspectiveFar,
                                camera.PerspectiveNear + 0.01f);
                    }
                    else
                    {
                        ImGui::DragFloat(
                            "Size",
                            &camera.OrthographicSize,
                            0.1f, 0.01f, 10000.0f);
                        ImGui::DragFloat(
                            "Near Clip##Ortho",
                            &camera.OrthographicNear,
                            0.1f);
                        ImGui::DragFloat(
                            "Far Clip##Ortho",
                            &camera.OrthographicFar,
                            1.0f);
                    }

                    if (ImGui::Button("Remove Camera"))
                        m_SelectedEntity.RemoveComponent<CameraComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<DirectionalLightComponent>())
            {
                if (ImGui::CollapsingHeader(
                        "Directional Light",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& light =
                        m_SelectedEntity.GetComponent<DirectionalLightComponent>();
                    ImGui::ColorEdit3("Color##Directional", &light.Color.x);
                    ImGui::DragFloat(
                        "Intensity##Directional",
                        &light.Intensity, 0.05f, 0.0f, 100.0f);
                    ImGui::Checkbox("Cast Shadows##Directional", &light.CastShadows);
                    ImGui::DragFloat("Shadow Bias##Directional", &light.ShadowBias, 0.0001f, 0.00001f, 0.05f, "%.5f");
                    if (ImGui::Button("Remove Directional Light"))
                        m_SelectedEntity.RemoveComponent<DirectionalLightComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<PointLightComponent>())
            {
                if (ImGui::CollapsingHeader(
                        "Point Light",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& light =
                        m_SelectedEntity.GetComponent<PointLightComponent>();
                    ImGui::ColorEdit3("Color##Point", &light.Color.x);
                    ImGui::DragFloat(
                        "Intensity##Point",
                        &light.Intensity, 0.05f, 0.0f, 100.0f);
                    ImGui::DragFloat(
                        "Range##Point",
                        &light.Range, 0.1f, 0.01f, 10000.0f);
                    ImGui::Checkbox("Cast Shadows##Point", &light.CastShadows);
                    ImGui::DragFloat("Shadow Bias##Point", &light.ShadowBias, 0.001f, 0.001f, 0.25f, "%.4f");
                    if (ImGui::Button("Remove Point Light"))
                        m_SelectedEntity.RemoveComponent<PointLightComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<SpotLightComponent>())
            {
                if (ImGui::CollapsingHeader(
                        "Spot Light",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& light =
                        m_SelectedEntity.GetComponent<SpotLightComponent>();
                    ImGui::ColorEdit3("Color##Spot", &light.Color.x);
                    ImGui::DragFloat(
                        "Intensity##Spot",
                        &light.Intensity, 0.05f, 0.0f, 100.0f);
                    ImGui::DragFloat(
                        "Range##Spot",
                        &light.Range, 0.1f, 0.01f, 10000.0f);
                    ImGui::SliderFloat(
                        "Inner Angle",
                        &light.InnerAngle, 0.1f, 89.0f);
                    ImGui::SliderFloat(
                        "Outer Angle",
                        &light.OuterAngle, 0.1f, 89.0f);
                    light.OuterAngle =
                        std::max(light.OuterAngle, light.InnerAngle);
                    ImGui::Checkbox("Cast Shadows##Spot", &light.CastShadows);
                    ImGui::DragFloat("Shadow Bias##Spot", &light.ShadowBias, 0.0001f, 0.00001f, 0.05f, "%.5f");
                    if (ImGui::Button("Remove Spot Light"))
                        m_SelectedEntity.RemoveComponent<SpotLightComponent>();
                }
            }


            if (m_SelectedEntity.HasComponent<AnimatorComponent>())
            {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& animator=m_SelectedEntity.GetComponent<AnimatorComponent>();
                    ImGui::Checkbox("Playing", &animator.Playing);
                    ImGui::SameLine(); ImGui::Checkbox("Loop", &animator.Loop);
                    ImGui::DragFloat("Speed", &animator.Speed, 0.05f, -4.0f, 4.0f);
                    if(animator.Animation && !animator.Animation->Clips().empty())
                    {
                        const auto& clips=animator.Animation->Clips();
                        animator.ClipIndex=std::clamp(animator.ClipIndex,0,(int)clips.size()-1);
                        if(ImGui::BeginCombo("Clip", clips[animator.ClipIndex].Name.c_str()))
                        {
                            for(int ci=0;ci<(int)clips.size();++ci)
                                if(ImGui::Selectable(clips[ci].Name.c_str(),ci==animator.ClipIndex))
                                { animator.ClipIndex=ci; animator.TimeSeconds=0.0f; }
                            ImGui::EndCombo();
                        }
                        ImGui::Text("Skeleton bones: %zu", animator.Animation->Bones().size());
                        ImGui::Text("Time: %.2f / %.2f s", animator.TimeSeconds,
                            (float)clips[animator.ClipIndex].DurationSeconds());
                    }
                    else ImGui::TextDisabled("No animation asset loaded.");
                }
            }

            DrawComponentTools();

            ImGui::Separator();
            ImGui::Spacing();
            const float addWidth = std::min(260.0f, ImGui::GetContentRegionAvail().x);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                std::max(0.0f, (ImGui::GetContentRegionAvail().x - addWidth) * 0.5f));
            if (ImGui::Button("Add Component", ImVec2(addWidth, 0.0f)))
                ImGui::OpenPopup("AddComponentPopup");

            if (ImGui::BeginPopup("AddComponentPopup"))
            {
                static char componentSearch[96]{};
                ImGui::SetNextItemWidth(300.0f);
                ImGui::InputTextWithHint("##ComponentSearch", "Search components...",
                    componentSearch, sizeof(componentSearch));
                ImGui::Separator();

                std::string filter = componentSearch;
                std::transform(filter.begin(), filter.end(), filter.begin(),
                    [](unsigned char c){ return (char)std::tolower(c); });
                auto visible = [&](const char* name)
                {
                    if(filter.empty()) return true;
                    std::string n=name;
                    std::transform(n.begin(), n.end(), n.begin(),
                        [](unsigned char c){ return (char)std::tolower(c); });
                    return n.find(filter) != std::string::npos;
                };
                auto addItem = [&](const char* category, const char* name, bool enabled, auto add)
                {
                    if(!visible(name)) return;
                    ImGui::TextDisabled("%s", category);
                    ImGui::SameLine(95.0f);
                    if(!enabled) ImGui::BeginDisabled();
                    if(ImGui::Selectable(name, false, enabled ? 0 : ImGuiSelectableFlags_Disabled))
                    {
                        CaptureUndoSnapshot();
                        add();
                        ImGui::CloseCurrentPopup();
                    }
                    if(!enabled) ImGui::EndDisabled();
                };

                addItem("Physics", "Rigidbody",
                    !m_SelectedEntity.HasComponent<RigidbodyComponent>(),
                    [&]{ m_SelectedEntity.AddComponent<RigidbodyComponent>(); });

                const bool noCollider =
                    !m_SelectedEntity.HasComponent<BoxColliderComponent>() &&
                    !m_SelectedEntity.HasComponent<SphereColliderComponent>() &&
                    !m_SelectedEntity.HasComponent<CapsuleColliderComponent>();
                addItem("Physics", "Box Collider", noCollider,
                    [&]{ m_SelectedEntity.AddComponent<BoxColliderComponent>(); });
                addItem("Physics", "Sphere Collider", noCollider,
                    [&]{ m_SelectedEntity.AddComponent<SphereColliderComponent>(); });
                addItem("Physics", "Capsule Collider", noCollider,
                    [&]{ m_SelectedEntity.AddComponent<CapsuleColliderComponent>(); });

                const bool noViewLight =
                    !m_SelectedEntity.HasComponent<CameraComponent>() &&
                    !m_SelectedEntity.HasComponent<DirectionalLightComponent>() &&
                    !m_SelectedEntity.HasComponent<PointLightComponent>() &&
                    !m_SelectedEntity.HasComponent<SpotLightComponent>();
                addItem("Rendering", "Camera", noViewLight,
                    [&]{ m_SelectedEntity.AddComponent<CameraComponent>(); });
                addItem("Rendering", "Directional Light", noViewLight,
                    [&]{ m_SelectedEntity.AddComponent<DirectionalLightComponent>(); });
                addItem("Rendering", "Point Light", noViewLight,
                    [&]{ m_SelectedEntity.AddComponent<PointLightComponent>(); });
                addItem("Rendering", "Spot Light", noViewLight,
                    [&]{ m_SelectedEntity.AddComponent<SpotLightComponent>(); });

                addItem("Effects", "Particle System",
                    !m_SelectedEntity.HasComponent<ParticleSystemComponent>(),
                    [&]{ m_SelectedEntity.AddComponent<ParticleSystemComponent>(); });

                addItem("Audio", "Audio Source",
                    !m_SelectedEntity.HasComponent<AudioSourceComponent>(),
                    [&]{ m_SelectedEntity.AddComponent<AudioSourceComponent>(); });
                addItem("Audio", "Audio Listener",
                    !m_SelectedEntity.HasComponent<AudioListenerComponent>(),
                    [&]{ m_SelectedEntity.AddComponent<AudioListenerComponent>(); });

                addItem("Animation", "Animator",
                    !m_SelectedEntity.HasComponent<AnimatorComponent>(),
                    [&]{ m_SelectedEntity.AddComponent<AnimatorComponent>(); });
                RegisterBuiltinScripts();
                for(const auto* definition:ScriptRegistry::All())
                {
                    const std::string label=definition->Name+" Script";
                    addItem("Scripting",label.c_str(),
                        !m_SelectedEntity.HasComponent<NativeScriptComponent>(),
                        [&,definition]{
                            NativeScriptComponent component;
                            component.ScriptName=definition->Name;
                            ScriptRegistry::ApplyDefaults(component);
                            m_SelectedEntity.AddComponent<NativeScriptComponent>(component);
                        });
                }

                ImGui::EndPopup();
            }

            if (m_SelectedEntity.HasComponent<PrefabInstanceComponent>())
            {
                ImGui::Separator();
                if(ImGui::CollapsingHeader("Prefab Instance",ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& pi=m_SelectedEntity.GetComponent<PrefabInstanceComponent>();
                    ImGui::TextWrapped("Source: %s",pi.SourcePath.c_str());
                    if(ImGui::Button("Apply to Prefab"))
                    {
                        CaptureUndoSnapshot();
                        PrefabSerializer::Apply(m_SelectedEntity,pi.SourcePath);
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Revert"))
                    {
                        CaptureUndoSnapshot();
                        Entity reverted=PrefabSerializer::Revert(
                            m_SelectedEntity,m_DefaultCubeMesh,m_DefaultCubeMaterial);
                        if(reverted) m_SelectedEntity=reverted;
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<MeshRendererComponent>())
            {
                ImGui::Separator();

                if (ImGui::CollapsingHeader(
                        "Mesh Renderer",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& renderer =
                        m_SelectedEntity.GetComponent<MeshRendererComponent>();

                    // Upgrade V1 renderers to the slot representation lazily.
                    if (renderer.Materials.empty() && renderer.MaterialAsset)
                        renderer.Materials.push_back(renderer.MaterialAsset);
                    else if (!renderer.Materials.empty() && !renderer.MaterialAsset)
                        renderer.MaterialAsset = renderer.Materials.front();

                    if (!renderer.Materials.empty())
                    {
                        if (m_SelectedMaterialSlot >= renderer.Materials.size())
                            m_SelectedMaterialSlot = 0;

                        ImGui::SeparatorText("Materials");
                        for (std::size_t slot = 0;
                             slot < renderer.Materials.size();
                             ++slot)
                        {
                            ImGui::PushID(static_cast<int>(slot));

                            const bool selected =
                                slot == m_SelectedMaterialSlot;
                            const char* state =
                                renderer.Materials[slot] ? "Assigned" : "None";

                            std::string label =
                                "Element " + std::to_string(slot) +
                                "  [" + state + "]";

                            if (ImGui::Selectable(label.c_str(), selected))
                                m_SelectedMaterialSlot = slot;

                            // Every element is a real material drop target.
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* payload =
                                        ImGui::AcceptDragDropPayload(
                                            "NOJOB_MATERIAL_ASSET"))
                                {
                                    const char* relativePath =
                                        static_cast<const char*>(payload->Data);

                                    AssetRegistry registry(
                                        AssetManager::GetAssetsDirectory());
                                    registry.Load();

                                    std::shared_ptr<Shader> shader;
                                    if (renderer.Materials[slot])
                                        shader =
                                            renderer.Materials[slot]->GetShader();
                                    else if (renderer.MaterialAsset)
                                        shader =
                                            renderer.MaterialAsset->GetShader();

                                    auto loaded = MaterialSerializer::Load(
                                        AssetManager::GetProjectRoot() /
                                            relativePath,
                                        shader,
                                        registry);

                                    if (loaded)
                                    {
                                        renderer.Materials[slot] = loaded;
                                        if (slot == 0)
                                            renderer.MaterialAsset = loaded;
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }

                            ImGui::PopID();
                        }

                        auto& activeMaterial =
                            renderer.Materials[m_SelectedMaterialSlot];

                        if (!activeMaterial)
                        {
                            ImGui::TextDisabled(
                                "Selected material slot is empty.");
                        }
                        else
                        {
                            // Keep the legacy slot-0 alias synchronized.
                            if (m_SelectedMaterialSlot == 0)
                                renderer.MaterialAsset = activeMaterial;

                            ImGui::SeparatorText(
                                ("Element " +
                                 std::to_string(m_SelectedMaterialSlot))
                                    .c_str());

                            const char* surfaceModes[] =
                            {
                                "Opaque",
                                "Alpha Clip",
                                "Transparent"
                            };

                            int surfaceMode =
                                static_cast<int>(
                                    activeMaterial->SurfaceMode());

                            if (ImGui::Combo(
                                    "Rendering Mode",
                                    &surfaceMode,
                                    surfaceModes,
                                    3))
                            {
                                activeMaterial->SurfaceMode() =
                                    static_cast<MaterialSurfaceMode>(
                                        surfaceMode);
                            }

                            if (activeMaterial->SurfaceMode() ==
                                MaterialSurfaceMode::AlphaClip)
                            {
                                ImGui::SliderFloat(
                                    "Alpha Cutoff",
                                    &activeMaterial->AlphaCutoff(),
                                    0.0f,
                                    1.0f);
                            }

                            ImGui::TextUnformatted("Material");
                            ImGui::SameLine();
                            ImGui::Button(
                                "Material Slot",
                                ImVec2(
                                    ImGui::GetContentRegionAvail().x,
                                    0.0f));

                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* materialPayload =
                                        ImGui::AcceptDragDropPayload(
                                            "NOJOB_MATERIAL_ASSET"))
                                {
                                    const char* relativePath =
                                        static_cast<const char*>(
                                            materialPayload->Data);

                                    AssetRegistry registry(
                                        AssetManager::GetAssetsDirectory());
                                    registry.Load();

                                    auto loaded = MaterialSerializer::Load(
                                        AssetManager::GetProjectRoot() /
                                            relativePath,
                                        activeMaterial->GetShader(),
                                        registry);

                                    if (loaded)
                                    {
                                        activeMaterial = loaded;
                                        if (m_SelectedMaterialSlot == 0)
                                            renderer.MaterialAsset = loaded;
                                    }
                                }

                                if (const ImGuiPayload* texturePayload =
                                        ImGui::AcceptDragDropPayload(
                                            "NOJOB_TEXTURE_ASSET"))
                                {
                                    const char* relativePath =
                                        static_cast<const char*>(
                                            texturePayload->Data);
                                    try
                                    {
                                        activeMaterial->SetTexture(
                                            AssetManager::LoadTexture(
                                                relativePath));
                                        activeMaterial->UseTexture() = true;
                                    }
                                    catch (const std::exception&) {}
                                }

                                ImGui::EndDragDropTarget();
                            }

                            ImGui::Separator();
                            auto& color = activeMaterial->GetColor();
                            ImGui::ColorEdit4(
                                "Material Color", &color.x);

                            ImGui::SeparatorText("PBR Surface");
                            ImGui::SliderFloat(
                                "Metallic",
                                &activeMaterial->Metallic(), 0.0f, 1.0f);
                            ImGui::SliderFloat(
                                "Roughness",
                                &activeMaterial->Roughness(), 0.04f, 1.0f);
                            ImGui::SliderFloat(
                                "Ambient Occlusion",
                                &activeMaterial->AmbientOcclusion(),
                                0.0f, 1.0f);
                            ImGui::SliderFloat(
                                "Normal Strength",
                                &activeMaterial->NormalStrength(),
                                0.0f, 2.0f);
                            ImGui::ColorEdit3(
                                "Emissive Color",
                                &activeMaterial->EmissiveColor().x);
                            ImGui::SliderFloat(
                                "Emissive Strength",
                                &activeMaterial->EmissiveStrength(),
                                0.0f, 20.0f);

                            ImGui::SeparatorText("PBR Texture Maps");
                            auto selectPBRMap =
                                [&](const char* label, auto setter)
                            {
                                if (ImGui::Button(label))
                                {
                                    const std::string path =
                                        OpenTextureFileDialog();
                                    if (!path.empty())
                                    {
                                        try
                                        {
                                            const auto importedPath =
                                                AssetManager::ImportTexture(
                                                    path);
                                            setter(
                                                AssetManager::LoadTexture(
                                                    importedPath));
                                        }
                                        catch (const std::exception&) {}
                                    }
                                }
                            };

                            selectPBRMap(
                                "Normal Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetNormalTexture(
                                        std::move(v));
                                });
                            ImGui::SameLine();
                            selectPBRMap(
                                "Metallic Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetMetallicTexture(
                                        std::move(v));
                                });
                            selectPBRMap(
                                "Roughness Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetRoughnessTexture(
                                        std::move(v));
                                });
                            ImGui::SameLine();
                            selectPBRMap(
                                "AO Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetAOTexture(
                                        std::move(v));
                                });
                            selectPBRMap(
                                "Emissive Map...",
                                [&](std::shared_ptr<Texture2D> v)
                                {
                                    activeMaterial->SetEmissiveTexture(
                                        std::move(v));
                                });

                            ImGui::Checkbox(
                                "Use Texture",
                                &activeMaterial->UseTexture());

                            if (ImGui::Button("Select Texture..."))
                            {
                                const std::string path =
                                    OpenTextureFileDialog();
                                if (!path.empty())
                                {
                                    try
                                    {
                                        const auto importedPath =
                                            AssetManager::ImportTexture(path);
                                        activeMaterial->SetTexture(
                                            AssetManager::LoadTexture(
                                                importedPath));
                                        activeMaterial->UseTexture() = true;
                                    }
                                    catch (const std::exception&) {}
                                }
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Checkerboard"))
                            {
                                activeMaterial->SetTexture(
                                    Texture2D::CreateCheckerboard());
                                activeMaterial->UseTexture() = true;
                            }

                            if (ImGui::Button("Save Material Asset"))
                            {
                                const auto& tag =
                                    m_SelectedEntity
                                        .GetComponent<TagComponent>().Tag;

                                const auto materialPath =
                                    AssetManager::GetAssetsDirectory() /
                                    "Materials" /
                                    (tag + "_Element" +
                                     std::to_string(
                                         m_SelectedMaterialSlot) +
                                     ".nojobmat");

                                AssetRegistry registry(
                                    AssetManager::GetAssetsDirectory());
                                registry.Load();
                                MaterialSerializer::Save(
                                    *activeMaterial,
                                    materialPath,
                                    registry);
                                registry.Register(
                                    materialPath,
                                    AssetType::Material);
                                registry.Save();
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Create Prefab"))
                            {
                                auto prefabPath =
                                    AssetManager::GetAssetsDirectory() /
                                    "Prefabs" /
                                    (m_SelectedEntity
                                         .GetComponent<TagComponent>().Tag +
                                     ".nojobprefab");
                                PrefabSerializer::Save(
                                    m_SelectedEntity, prefabPath);
                                AssetRegistry registry(
                                    AssetManager::GetAssetsDirectory());
                                registry.Load();
                                registry.Register(
                                    prefabPath, AssetType::Prefab);
                                registry.Save();
                            }

                            if (activeMaterial->GetTexture())
                            {
                                const auto& texture =
                                    activeMaterial->GetTexture();
                                const std::filesystem::path texturePath(
                                    texture->GetPath());

                                const std::string displayName =
                                    texture->GetPath() == "Checkerboard"
                                        ? std::string("Checkerboard")
                                        : texturePath.filename().string();

                                ImGui::TextDisabled(
                                    "Texture: %s",
                                    displayName.c_str());

                                ImGui::Image(
                                    static_cast<ImTextureID>(
                                        static_cast<intptr_t>(
                                            texture->GetRendererID())),
                                    ImVec2(96.0f, 96.0f));
                            }
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("No material assigned.");
                    }
                }
            }

            ImGui::Separator();

            const auto id =
                m_SelectedEntity.GetComponent<IDComponent>().ID;

            ImGui::Text(
                "Entity ID: %llu",
                static_cast<unsigned long long>(id));
        }
        else
        {
            ImGui::TextDisabled(
                "Select an entity in Hierarchy.");
        }

        ImGui::End();
    }

    void EditorLayer::DrawViewport()
    {
        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(0, 0));

        ImGui::Begin("Viewport");

        m_ViewportHovered = ImGui::IsWindowHovered();
        m_ViewportFocused = ImGui::IsWindowFocused();

        const ImVec2 viewportMin =
            ImGui::GetCursorScreenPos();

        const ImVec2 available =
            ImGui::GetContentRegionAvail();

        m_ViewportWidth =
            std::max(1.0f, available.x);

        m_ViewportHeight =
            std::max(1.0f, available.y);

        if (m_ViewportTextureID != 0)
        {
            ImGui::Image(
                static_cast<ImTextureID>(
                    static_cast<intptr_t>(
                        m_ViewportTextureID)),
                ImVec2(
                    m_ViewportWidth,
                    m_ViewportHeight),
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        }

        // Unity-style collider wireframes. These are editor-only overlays
        // and never become part of the game framebuffer.
        // Scene gizmos belong to Edit Mode only. During Play the viewport
        // must contain only the game camera render, like Unity's Game view.
        if (m_Scene && !m_IsPlaying)
        {
            DrawSceneColliderGizmos(
                *m_Scene,
                m_SelectedEntity,
                m_EditorView,
                m_EditorProjection,
                viewportMin,
                ImVec2(m_ViewportWidth, m_ViewportHeight));
        }

        if (!m_IsPlaying &&
            m_CameraPreviewTextureID != 0 &&
            m_SelectedEntity &&
            m_SelectedEntity.HasComponent<CameraComponent>())
        {
            const ImVec2 previewSize(320.0f, 180.0f);
            const ImVec2 previewMin(
                viewportMin.x + m_ViewportWidth - previewSize.x - 16.0f,
                viewportMin.y + 16.0f);
            const ImVec2 previewMax(
                previewMin.x + previewSize.x,
                previewMin.y + previewSize.y);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(
                {previewMin.x - 3.0f, previewMin.y - 3.0f},
                {previewMax.x + 3.0f, previewMax.y + 3.0f},
                IM_COL32(25, 25, 28, 240));
            drawList->AddImage(
                static_cast<ImTextureID>(
                    static_cast<intptr_t>(m_CameraPreviewTextureID)),
                previewMin,
                previewMax,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
            drawList->AddText(
                {previewMin.x + 8.0f, previewMin.y + 6.0f},
                IM_COL32(255, 255, 255, 220),
                "Camera Preview");
        }

        // W = Translate, E = Rotate, R = Scale.
        // Only change tools while the viewport is active and the user
        // isn't currently dragging the gizmo.
        if (m_ViewportHovered && !ImGuizmo::IsUsing())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W))
                m_GizmoOperation = 0;

            if (ImGui::IsKeyPressed(ImGuiKey_E))
                m_GizmoOperation = 1;

            if (ImGui::IsKeyPressed(ImGuiKey_R))
                m_GizmoOperation = 2;
        }

        if (m_SelectedEntity
            && m_ViewportWidth > 1.0f
            && m_ViewportHeight > 1.0f)
        {
            

            glm::mat4 transformMatrix =
                m_Scene->GetWorldTransform(m_SelectedEntity);

            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(
                viewportMin.x,
                viewportMin.y,
                m_ViewportWidth,
                m_ViewportHeight);

            ImGuizmo::OPERATION operation =
                ImGuizmo::TRANSLATE;

            if (m_GizmoOperation == 1)
                operation = ImGuizmo::ROTATE;
            else if (m_GizmoOperation == 2)
                operation = ImGuizmo::SCALE;

            const ImGuizmo::MODE mode =
                operation == ImGuizmo::SCALE
                    ? ImGuizmo::LOCAL
                    : ImGuizmo::WORLD;

            ImGuizmo::Manipulate(
                glm::value_ptr(m_EditorView),
                glm::value_ptr(m_EditorProjection),
                operation,
                mode,
                glm::value_ptr(transformMatrix));

            const bool gizmoUsing = ImGuizmo::IsUsing();
            if (gizmoUsing && !m_GizmoWasUsing)
                m_TransformEditSnapshot = m_Scene->Copy();

            if (gizmoUsing)
            {
                float translation[3]{};
                float rotationDegrees[3]{};
                float scale[3]{};

                ImGuizmo::DecomposeMatrixToComponents(
                    glm::value_ptr(transformMatrix),
                    translation,
                    rotationDegrees,
                    scale);

                // Gizmo operates in world space. Scene converts it back
                // into the selected entity's local transform if it has a parent.
                m_Scene->SetWorldTransform(
                    m_SelectedEntity,
                    transformMatrix);
            }

            if (!gizmoUsing && m_GizmoWasUsing && m_TransformEditSnapshot)
                PushUndoSnapshot(std::move(m_TransformEditSnapshot));

            m_GizmoWasUsing = gizmoUsing;
        }

        // Small toolbar over the scene.
        ImGui::SetCursorScreenPos(
            ImVec2(viewportMin.x + 10.0f, viewportMin.y + 10.0f));

        ImGui::BeginGroup();

        if (ImGui::Button("W Move"))
            m_GizmoOperation = 0;

        ImGui::SameLine();

        if (ImGui::Button("E Rotate"))
            m_GizmoOperation = 1;

        ImGui::SameLine();

        if (ImGui::Button("R Scale"))
            m_GizmoOperation = 2;

        ImGui::EndGroup();

        ImGui::SetCursorScreenPos(
            ImVec2(viewportMin.x + 10.0f, viewportMin.y + 42.0f));

        ImGui::TextDisabled(
            "RMB + mouse: camera | WASD: move | "
            "Q/E: down/up | Shift: faster");

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::DrawConsole()
    {
        ImGui::Begin("Console");
        ImGui::Text("[Info] NoJobEngine editor started.");
        ImGui::Text("[Info] Scene entity management active.");
        ImGui::Text(
            "[Info] Right click Hierarchy to create objects.");
#ifdef _WIN32
        std::vector<std::string> scriptConsoleSnapshot;
        {
            std::lock_guard<std::mutex> lock(s_ScriptConsoleMutex);
            scriptConsoleSnapshot = s_ScriptConsole;
        }
        if (!scriptConsoleSnapshot.empty())
        {
            ImGui::SeparatorText("Native Scripting");
            for (const auto& line : scriptConsoleSnapshot)
                ImGui::TextUnformatted(line.c_str());
        }
#endif
        ImGui::End();
    }
    void EditorLayer::DrawComponentTools()
    {
        if (!m_SelectedEntity) return;

        ImGui::SeparatorText("Component Actions");
        static int componentIndex = 0;

        struct Entry { const char* Name; int Id; };
        std::vector<Entry> entries;
        entries.push_back({"Transform", 0});
        if(m_SelectedEntity.HasComponent<NativeScriptComponent>()) entries.push_back({"Native Script",1});
        if(m_SelectedEntity.HasComponent<RigidbodyComponent>()) entries.push_back({"Rigidbody",2});
        if(m_SelectedEntity.HasComponent<BoxColliderComponent>()) entries.push_back({"Box Collider",3});
        if(m_SelectedEntity.HasComponent<SphereColliderComponent>()) entries.push_back({"Sphere Collider",4});
        if(m_SelectedEntity.HasComponent<CapsuleColliderComponent>()) entries.push_back({"Capsule Collider",5});
        if(m_SelectedEntity.HasComponent<CameraComponent>()) entries.push_back({"Camera",6});
        if(m_SelectedEntity.HasComponent<DirectionalLightComponent>()) entries.push_back({"Directional Light",7});
        if(m_SelectedEntity.HasComponent<PointLightComponent>()) entries.push_back({"Point Light",8});
        if(m_SelectedEntity.HasComponent<SpotLightComponent>()) entries.push_back({"Spot Light",9});
        if(m_SelectedEntity.HasComponent<AnimatorComponent>()) entries.push_back({"Animator",10});

        componentIndex = std::clamp(componentIndex, 0, (int)entries.size()-1);
        if(ImGui::BeginCombo("Component", entries[componentIndex].Name))
        {
            for(int i=0;i<(int)entries.size();++i)
                if(ImGui::Selectable(entries[i].Name, i==componentIndex))
                    componentIndex=i;
            ImGui::EndCombo();
        }

        const int id=entries[componentIndex].Id;
        auto copy=[&]{
            switch(id){
            case 0:m_ComponentClipboard=m_SelectedEntity.GetComponent<TransformComponent>();break;
            case 1:m_ComponentClipboard=m_SelectedEntity.GetComponent<NativeScriptComponent>();break;
            case 2:m_ComponentClipboard=m_SelectedEntity.GetComponent<RigidbodyComponent>();break;
            case 3:m_ComponentClipboard=m_SelectedEntity.GetComponent<BoxColliderComponent>();break;
            case 4:m_ComponentClipboard=m_SelectedEntity.GetComponent<SphereColliderComponent>();break;
            case 5:m_ComponentClipboard=m_SelectedEntity.GetComponent<CapsuleColliderComponent>();break;
            case 6:m_ComponentClipboard=m_SelectedEntity.GetComponent<CameraComponent>();break;
            case 7:m_ComponentClipboard=m_SelectedEntity.GetComponent<DirectionalLightComponent>();break;
            case 8:m_ComponentClipboard=m_SelectedEntity.GetComponent<PointLightComponent>();break;
            case 9:m_ComponentClipboard=m_SelectedEntity.GetComponent<SpotLightComponent>();break;
            case 10:m_ComponentClipboard=m_SelectedEntity.GetComponent<AnimatorComponent>();break;
            }
        };
        auto reset=[&]{
            CaptureUndoSnapshot();
            switch(id){
            case 0:m_SelectedEntity.GetComponent<TransformComponent>()={};break;
            case 1:m_SelectedEntity.GetComponent<NativeScriptComponent>()={};break;
            case 2:m_SelectedEntity.GetComponent<RigidbodyComponent>()={};break;
            case 3:m_SelectedEntity.GetComponent<BoxColliderComponent>()={};break;
            case 4:m_SelectedEntity.GetComponent<SphereColliderComponent>()={};break;
            case 5:m_SelectedEntity.GetComponent<CapsuleColliderComponent>()={};break;
            case 6:m_SelectedEntity.GetComponent<CameraComponent>()={};break;
            case 7:m_SelectedEntity.GetComponent<DirectionalLightComponent>()={};break;
            case 8:m_SelectedEntity.GetComponent<PointLightComponent>()={};break;
            case 9:m_SelectedEntity.GetComponent<SpotLightComponent>()={};break;
            case 10:m_SelectedEntity.GetComponent<AnimatorComponent>()={};break;
            }
        };
        auto paste=[&]{
            CaptureUndoSnapshot();
            switch(id){
            case 0:if(auto p=std::get_if<TransformComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<TransformComponent>()=*p;break;
            case 1:if(auto p=std::get_if<NativeScriptComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<NativeScriptComponent>()=*p;break;
            case 2:if(auto p=std::get_if<RigidbodyComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<RigidbodyComponent>()=*p;break;
            case 3:if(auto p=std::get_if<BoxColliderComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<BoxColliderComponent>()=*p;break;
            case 4:if(auto p=std::get_if<SphereColliderComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<SphereColliderComponent>()=*p;break;
            case 5:if(auto p=std::get_if<CapsuleColliderComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<CapsuleColliderComponent>()=*p;break;
            case 6:if(auto p=std::get_if<CameraComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<CameraComponent>()=*p;break;
            case 7:if(auto p=std::get_if<DirectionalLightComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<DirectionalLightComponent>()=*p;break;
            case 8:if(auto p=std::get_if<PointLightComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<PointLightComponent>()=*p;break;
            case 9:if(auto p=std::get_if<SpotLightComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<SpotLightComponent>()=*p;break;
            case 10:if(auto p=std::get_if<AnimatorComponent>(&m_ComponentClipboard))m_SelectedEntity.GetComponent<AnimatorComponent>()=*p;break;
            }
        };

        if(ImGui::SmallButton("Reset")) reset();
        ImGui::SameLine();
        if(ImGui::SmallButton("Copy")) copy();
        ImGui::SameLine();
        if(ImGui::SmallButton("Paste")) paste();
        ImGui::SameLine();

        const bool removable=id!=0;
        if(!removable) ImGui::BeginDisabled();
        if(ImGui::SmallButton("Remove") && removable)
        {
            CaptureUndoSnapshot();
            switch(id){
            case 1:m_SelectedEntity.RemoveComponent<NativeScriptComponent>();break;
            case 2:m_SelectedEntity.RemoveComponent<RigidbodyComponent>();break;
            case 3:m_SelectedEntity.RemoveComponent<BoxColliderComponent>();break;
            case 4:m_SelectedEntity.RemoveComponent<SphereColliderComponent>();break;
            case 5:m_SelectedEntity.RemoveComponent<CapsuleColliderComponent>();break;
            case 6:m_SelectedEntity.RemoveComponent<CameraComponent>();break;
            case 7:m_SelectedEntity.RemoveComponent<DirectionalLightComponent>();break;
            case 8:m_SelectedEntity.RemoveComponent<PointLightComponent>();break;
            case 9:m_SelectedEntity.RemoveComponent<SpotLightComponent>();break;
            case 10:m_SelectedEntity.RemoveComponent<AnimatorComponent>();break;
            }
            componentIndex=0;
        }
        if(!removable) ImGui::EndDisabled();
        ImGui::TextDisabled("F2 Rename | Ctrl+D Duplicate | Delete | Ctrl+Z/Y Undo/Redo");
    }

    void EditorLayer::DrawProjectPanel()
    {
        ImGui::Begin("Project");

        if (m_ProjectDirectory.empty())
            m_ProjectDirectory = AssetManager::GetProjectRoot();

        const auto projectRoot = AssetManager::GetProjectRoot();
        const auto assetsRoot = AssetManager::GetAssetsDirectory();

        if (m_ProjectDirectory != projectRoot)
        {
            if (ImGui::Button("< Back"))
            {
                const auto parent = m_ProjectDirectory.parent_path();
                m_ProjectDirectory =
                    parent.string().size() < projectRoot.string().size()
                    ? projectRoot : parent;
            }

            ImGui::SameLine();
        }

        ImGui::TextDisabled(
            "%s",
            m_ProjectDirectory.lexically_relative(
                AssetManager::GetProjectRoot()).generic_string().c_str());

        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##ProjectSearch", "Search current folder...",
            m_ProjectSearch, sizeof(m_ProjectSearch));
        ImGui::Separator();

        // Unity-style prefab creation:
        // drag an entity from Hierarchy and drop it into any folder inside Assets.
        const bool projectFolderIsInsideAssets =
            m_ProjectDirectory == assetsRoot
            || m_ProjectDirectory.string().rfind(assetsRoot.string(), 0) == 0;

        if (projectFolderIsInsideAssets)
        {
            ImGui::InvisibleButton(
                "##ProjectPrefabDropTarget",
                ImVec2(ImGui::GetContentRegionAvail().x, 28.0f));

            // IMPORTANT: BeginDragDropTarget must be immediately after the
            // target item. Previously TextDisabled became the last item,
            // therefore ImGui never accepted the Hierarchy payload here.
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("NOJOB_ENTITY"))
                {
                    const auto handle =
                        *static_cast<const std::uint32_t*>(payload->Data);

                    if (m_Scene && m_Scene->IsValid(handle))
                    {
                        Entity source(handle, m_Scene);
                        auto prefabName =
                            source.GetComponent<TagComponent>().Tag;

                        for (char& c : prefabName)
                        {
                            if (c == '/' || c == '\\' || c == ':' ||
                                c == '*' || c == '?' || c == '"' ||
                                c == '<' || c == '>' || c == '|')
                                c = '_';
                        }

                        auto prefabPath =
                            m_ProjectDirectory /
                            (prefabName + ".nojobprefab");

                        int suffix = 1;
                        while (std::filesystem::exists(prefabPath))
                        {
                            prefabPath =
                                m_ProjectDirectory /
                                (prefabName + " (" +
                                 std::to_string(suffix++) +
                                 ").nojobprefab");
                        }

                        if (PrefabSerializer::Save(source, prefabPath))
                        {
                            AssetRegistry registry(
                                AssetManager::GetAssetsDirectory());
                            registry.Load();
                            registry.Register(
                                prefabPath,
                                AssetType::Prefab);
                            registry.Save();
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 28.0f);
            ImGui::TextDisabled("Drop a Hierarchy object here to create a Prefab");
            ImGui::Separator();
        }

        // Right click empty Project space -> Create -> Prefab From Selected.
        if (ImGui::BeginPopupContextWindow(
                "ProjectCreateContext",
                ImGuiPopupFlags_MouseButtonRight |
                ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("Create"))
            {
                const bool canCreatePrefab =
                    projectFolderIsInsideAssets
                    && static_cast<bool>(m_SelectedEntity);

                if (ImGui::MenuItem("C++ Script", nullptr, false,
                        projectFolderIsInsideAssets))
                {
                    m_RequestCreateCppScript = true;
                }

                if (ImGui::MenuItem(
                        "Prefab From Selected",
                        nullptr,
                        false,
                        canCreatePrefab))
                {
                    auto prefabName =
                        m_SelectedEntity.GetComponent<TagComponent>().Tag;

                    for (char& c : prefabName)
                    {
                        if (c == '/' || c == '\\' || c == ':' ||
                            c == '*' || c == '?' || c == '"' ||
                            c == '<' || c == '>' || c == '|')
                            c = '_';
                    }

                    auto prefabPath =
                        m_ProjectDirectory /
                        (prefabName + ".nojobprefab");

                    int suffix = 1;
                    while (std::filesystem::exists(prefabPath))
                    {
                        prefabPath =
                            m_ProjectDirectory /
                            (prefabName + " (" +
                             std::to_string(suffix++) +
                             ").nojobprefab");
                    }

                    if (PrefabSerializer::Save(
                            m_SelectedEntity,
                            prefabPath))
                    {
                        AssetRegistry registry(
                            AssetManager::GetAssetsDirectory());
                        registry.Load();
                        registry.Register(
                            prefabPath,
                            AssetType::Prefab);
                        registry.Save();
                    }
                }

                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        // Open the script modal only after the Project context popup has
        // completely ended. Opening it from inside the nested popup gives it
        // the wrong ImGui popup parent and it silently disappears.
        if (m_RequestCreateCppScript)
        {
            ImGui::OpenPopup("Create C++ Script");
            m_RequestCreateCppScript = false;
        }

        // Native C++ script authoring. Scripts are always created under the
        // project's Assets/Scripts tree so CMake can discover them reliably.
        static char newScriptName[128] = "NewScript";
        if (ImGui::BeginPopupModal("Create C++ Script", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Script name");
            ImGui::SetNextItemWidth(280.0f);
            const bool enter = ImGui::InputText("##CppScriptName",
                newScriptName, sizeof(newScriptName),
                ImGuiInputTextFlags_EnterReturnsTrue);

            const auto scriptsRoot = assetsRoot / "Scripts";
            std::string sanitized = SanitizeCppIdentifier(newScriptName);
            const bool exists =
                std::filesystem::exists(scriptsRoot / (sanitized + ".h")) ||
                std::filesystem::exists(scriptsRoot / (sanitized + ".cpp"));
            if (exists) ImGui::TextDisabled("A script with this name already exists.");
            else ImGui::TextDisabled("Creates Assets/Scripts/%s.h and %s.cpp",
                sanitized.c_str(), sanitized.c_str());

            const bool create = (ImGui::Button("Create") || enter) && !exists;
            ImGui::SameLine();
            const bool cancel = ImGui::Button("Cancel");

            if (create)
            {
                if (CreateCppScriptAsset(scriptsRoot, sanitized))
                {
                    m_ProjectDirectory = scriptsRoot;
                    newScriptName[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }
            if (cancel) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        std::error_code error;
        if (std::filesystem::exists(m_ProjectDirectory, error))
        {
            for (const auto& entry :
                 std::filesystem::directory_iterator(
                     m_ProjectDirectory,
                     std::filesystem::directory_options::skip_permission_denied,
                     error))
            {
                const auto path = entry.path();
                const std::string name =
                    path.filename().string();

                if (m_ProjectSearch[0] != '\0')
                {
                    std::string filter = m_ProjectSearch;
                    std::string lowerName = name;
                    std::transform(filter.begin(), filter.end(), filter.begin(),
                        [](unsigned char c){ return (char)std::tolower(c); });
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                        [](unsigned char c){ return (char)std::tolower(c); });
                    if (lowerName.find(filter) == std::string::npos)
                        continue;
                }

                // Unity-style root: Project shows the Assets folder first,
                // instead of dumping CMake/source files into this panel.
                if (m_ProjectDirectory == projectRoot
                    && path.lexically_normal() != assetsRoot.lexically_normal())
                    continue;

                ImGui::PushID(path.string().c_str());

                if (entry.is_directory())
                {
                    if (ImGui::Selectable(
                            ("[Folder] " + name).c_str(),
                            false,
                            ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (ImGui::IsMouseDoubleClicked(
                                ImGuiMouseButton_Left))
                        {
                            m_ProjectDirectory = path;
                        }
                    }

                    // Unity-style: drop a Hierarchy entity directly ON a
                    // folder (for example Assets/Prefabs).
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload =
                                ImGui::AcceptDragDropPayload("NOJOB_ENTITY"))
                        {
                            const auto handle =
                                *static_cast<const std::uint32_t*>(
                                    payload->Data);

                            if (m_Scene && m_Scene->IsValid(handle))
                            {
                                Entity source(handle, m_Scene);
                                auto prefabName =
                                    source.GetComponent<TagComponent>().Tag;

                                for (char& c : prefabName)
                                {
                                    if (c == '/' || c == '\\' || c == ':' ||
                                        c == '*' || c == '?' || c == '"' ||
                                        c == '<' || c == '>' || c == '|')
                                        c = '_';
                                }

                                auto prefabPath =
                                    path / (prefabName + ".nojobprefab");

                                int suffix = 1;
                                while (std::filesystem::exists(prefabPath))
                                {
                                    prefabPath =
                                        path /
                                        (prefabName + " (" +
                                         std::to_string(suffix++) +
                                         ").nojobprefab");
                                }

                                if (PrefabSerializer::Save(
                                        source,
                                        prefabPath))
                                {
                                    AssetRegistry registry(
                                        AssetManager::GetAssetsDirectory());
                                    registry.Load();
                                    registry.Register(
                                        prefabPath,
                                        AssetType::Prefab);
                                    registry.Save();
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                else
                {
                    const std::string extension =
                        path.extension().string();

                    const bool isTexture =
                        extension == ".png"
                        || extension == ".jpg"
                        || extension == ".jpeg"
                        || extension == ".bmp"
                        || extension == ".tga";

                    const bool isModel =
                        extension == ".obj" || extension == ".fbx" ||
                        extension == ".gltf" || extension == ".glb" ||
                        extension == ".dae" || extension == ".stl" ||
                        extension == ".ply" || extension == ".3ds" ||
                        extension == ".blend";
                    const bool isMaterial = extension == ".nojobmat";
                    const bool isPrefab = extension == ".nojobprefab";
                    const bool isScript = extension == ".cpp" || extension == ".h" || extension == ".hpp";
                    const bool isAudio = extension == ".wav" || extension == ".mp3" || extension == ".flac";
                    if(isAudio)
                    {
                        ImGui::Selectable(("[Audio] " + name).c_str());
                        const std::string relative =
                            AssetManager::ToProjectRelative(path).generic_string();
                        if (ImGui::BeginDragDropSource())
                        {
                            ImGui::SetDragDropPayload(
                                "NOJOB_AUDIO_ASSET",
                                relative.c_str(), relative.size() + 1);
                            ImGui::Text("Audio: %s", name.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                    else if(isScript)
                    {
                        const bool clicked = ImGui::Selectable(
                            ("[C++] " + name).c_str(),
                            false,
                            ImGuiSelectableFlags_AllowDoubleClick);

#ifdef _WIN32
                        if (clicked &&
                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            OpenScriptInVisualStudio(path);
                        }

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Open in Visual Studio"))
                                OpenScriptInVisualStudio(path);

                            ImGui::Separator();
                            ImGui::TextDisabled(
                                "%s",
                                path.lexically_relative(
                                    AssetManager::GetProjectRoot())
                                    .generic_string().c_str());
                            ImGui::EndPopup();
                        }
#endif
                    }
                    else if(isModel){if(ImGui::Selectable(name.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick)&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))CreateModelEntity(AssetManager::ToProjectRelative(path));}
                    else if(isPrefab)
                    {
                        if(ImGui::Selectable(name.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick)
                            && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            auto e=PrefabSerializer::Instantiate(
                                *m_Scene,path,m_DefaultCubeMesh,m_DefaultCubeMaterial);
                            if(e)m_SelectedEntity=e;
                        }

                        if(ImGui::BeginPopupContextItem())
                        {
                            if(ImGui::MenuItem("Instantiate Prefab"))
                            {
                                auto e=PrefabSerializer::Instantiate(
                                    *m_Scene,path,m_DefaultCubeMesh,m_DefaultCubeMaterial);
                                if(e)m_SelectedEntity=e;
                            }
                            ImGui::EndPopup();
                        }
                    }
                    else if(isMaterial)
                    {
                        const bool clicked=ImGui::Selectable(
                            name.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick);

                        const std::string relative =
                            AssetManager::ToProjectRelative(path).generic_string();

                        if(clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
                           && m_SelectedEntity
                           && m_SelectedEntity.HasComponent<MeshRendererComponent>())
                        {
                            auto& renderer=m_SelectedEntity.GetComponent<MeshRendererComponent>();
                            if(renderer.MaterialAsset)
                            {
                                AssetRegistry registry(AssetManager::GetAssetsDirectory());
                                registry.Load();
                                auto loaded=MaterialSerializer::Load(
                                    path,renderer.MaterialAsset->GetShader(),registry);
                                if(loaded) renderer.MaterialAsset=loaded;
                            }
                        }

                        if(ImGui::BeginDragDropSource())
                        {
                            ImGui::SetDragDropPayload(
                                "NOJOB_MATERIAL_ASSET",
                                relative.c_str(),relative.size()+1);
                            ImGui::Text("Material: %s",name.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                    else if (isTexture)
                    {
                        ImGui::Selectable(name.c_str());

                        if (ImGui::BeginDragDropSource())
                        {
                            const std::string relative =
                                AssetManager::ToProjectRelative(path)
                                    .generic_string();

                            ImGui::SetDragDropPayload(
                                "NOJOB_TEXTURE_ASSET",
                                relative.c_str(),
                                relative.size() + 1);

                            ImGui::Text("Texture: %s", name.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("%s", name.c_str());
                    }
                }

                ImGui::PopID();
            }
        }

        ImGui::End();
    }

    void EditorLayer::DrawGraphicsSettings()
    {
        if (!ImGui::Begin("Graphics Settings", &m_ShowGraphicsSettings))
        {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("NoJobEngine Rendering Pipeline");
        ImGui::SeparatorText("HDR / Tone Mapping");
        ImGui::Checkbox("HDR", &m_GraphicsSettings.HDR);
        ImGui::SliderFloat("Exposure", &m_GraphicsSettings.Exposure, 0.1f, 3.0f);

        ImGui::SeparatorText("Environment / IBL");
        ImGui::Checkbox(
            "Image Based Lighting", &m_GraphicsSettings.ImageBasedLighting);
        ImGui::SliderFloat(
            "Environment Intensity",
            &m_GraphicsSettings.EnvironmentIntensity, 0.0f, 4.0f);
        ImGui::SliderFloat(
            "Diffuse IBL",
            &m_GraphicsSettings.DiffuseIBLStrength, 0.0f, 1.0f);
        ImGui::SliderFloat(
            "Specular IBL",
            &m_GraphicsSettings.SpecularIBLStrength, 0.0f, 1.0f);
        ImGui::SliderFloat(
            "Environment Rotation",
            &m_GraphicsSettings.EnvironmentRotation, -180.0f, 180.0f,
            "%.0f deg");

        if (ImGui::Button("Load HDRI..."))
        {
            const std::string path = OpenHDRIFileDialog();
            if (!path.empty())
            {
                std::filesystem::path selected(path);
                std::error_code ec;
                const auto relative = std::filesystem::relative(
                    selected, m_ProjectDirectory, ec);
                if (!ec && !relative.empty() &&
                    relative.generic_string().rfind("..", 0) != 0)
                {
                    m_GraphicsSettings.EnvironmentHDRIPath =
                        relative.generic_string();
                }
                else
                {
                    m_GraphicsSettings.EnvironmentHDRIPath =
                        selected.lexically_normal().string();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear HDRI"))
            m_GraphicsSettings.EnvironmentHDRIPath.clear();

        if (m_GraphicsSettings.EnvironmentHDRIPath.empty())
            ImGui::TextDisabled("Environment: Procedural fallback");
        else
        {
            const std::filesystem::path hdriPath(
                m_GraphicsSettings.EnvironmentHDRIPath);
            ImGui::TextWrapped(
                "HDRI: %s", hdriPath.filename().string().c_str());
        }

        const char* environmentSizes[] = {"128", "256", "512", "1024"};
        int environmentSizeIndex =
            m_GraphicsSettings.EnvironmentResolution <= 128 ? 0 :
            m_GraphicsSettings.EnvironmentResolution <= 256 ? 1 :
            m_GraphicsSettings.EnvironmentResolution <= 512 ? 2 : 3;
        if (ImGui::Combo(
                "Environment Resolution", &environmentSizeIndex,
                environmentSizes, 4))
        {
            const std::uint32_t sizes[] = {128, 256, 512, 1024};
            m_GraphicsSettings.EnvironmentResolution =
                sizes[environmentSizeIndex];
        }

        ImGui::TextDisabled(
            "HDRI is converted to cubemaps and precomputed for real-time IBL.");

        ImGui::SeparatorText("Bloom");
        ImGui::Checkbox("Bloom", &m_GraphicsSettings.Bloom);
        ImGui::SliderFloat("Bloom Threshold", &m_GraphicsSettings.BloomThreshold, 0.1f, 5.0f);
        ImGui::SliderFloat("Bloom Strength", &m_GraphicsSettings.BloomStrength, 0.0f, 1.0f);

        ImGui::SeparatorText("Ambient Occlusion");
        ImGui::Checkbox("Screen Space AO", &m_GraphicsSettings.ScreenSpaceAO);
        ImGui::SliderFloat("AO Intensity", &m_GraphicsSettings.AOIntensity, 0.0f, 1.0f);

        ImGui::SeparatorText("Anti-Aliasing");
        ImGui::Checkbox("FXAA", &m_GraphicsSettings.FXAA);

        ImGui::Separator();
        ImGui::TextWrapped("ACES filmic tone mapping and final gamma conversion are applied once at the end of the HDR pipeline.");
        ImGui::End();
    }

    void EditorLayer::DrawRendererProfiler()
    {
        if (!m_ShowRendererProfiler)
            return;

        if (!ImGui::Begin("Renderer Profiler", &m_ShowRendererProfiler))
        {
            ImGui::End();
            return;
        }

        const auto& rendererStats = SceneRenderer::GetStatistics();
        const ImGuiIO& io = ImGui::GetIO();

        ImGui::TextUnformatted("NoJobEngine Renderer V3");
        ImGui::SeparatorText("Frame");
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame Time: %.2f ms",
            io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);

        ImGui::SeparatorText("Scene Renderer");
        ImGui::Text("CPU Time: %.3f ms", rendererStats.CPUTimeMs);
        if (rendererStats.GPUTimeValid)
            ImGui::Text("GPU Time: %.3f ms", rendererStats.GPUTimeMs);
        else
            ImGui::TextDisabled("GPU Time: warming up...");

        ImGui::SeparatorText("Geometry");
        ImGui::Text("Draw Calls: %u", rendererStats.DrawCalls);
        ImGui::Text("Triangles: %llu",
            static_cast<unsigned long long>(rendererStats.Triangles));

        ImGui::SeparatorText("Shadows");
        ImGui::Text("Shadow Passes: %u", rendererStats.ShadowPasses);
        ImGui::Text("Shadow Draw Calls: %u",
            rendererStats.ShadowDrawCalls);
        ImGui::Text("Shadow Triangles: %llu",
            static_cast<unsigned long long>(rendererStats.ShadowTriangles));

        ImGui::Separator();
        ImGui::TextDisabled(
            "GPU timing uses non-blocking queries with a 3-frame ring.");
        ImGui::End();
    }

    bool EditorLayer::ConsumeSaveSceneRequest()
    {
        const bool requested = m_SaveSceneRequested;
        m_SaveSceneRequested = false;
        return requested;
    }

    bool EditorLayer::ConsumeLoadSceneRequest()
    {
        const bool requested = m_LoadSceneRequested;
        m_LoadSceneRequested = false;
        return requested;
    }

    bool EditorLayer::ConsumeGraphicsTestSceneRequest()
    {
        const bool requested = m_GraphicsTestSceneRequested;
        m_GraphicsTestSceneRequested = false;
        return requested;
    }

}
