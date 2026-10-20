#include "Engine/AI/BehaviorTree/BTAsset.h"
#include "Engine/AI/BehaviorTree/BTAssetPaths.h"
#include "Engine/AI/BehaviorTree/BTGameplay.h"
#include "Editor/EditorLayer.h"
#include "Editor/Panels/NeuralEditorPanel.h"
#include "Editor/Build/StandaloneBuilder.h"
#include "Editor/Build/BuildSettings.h"
#include "Editor/Assets/ProjectPanel.h"
#include "Editor/Assets/ProjectAssetOperations.h"
#include "Editor/Scene/SceneFileDialog.h"
#include "Editor/Scripting/ProjectScriptManager.h"
#include "Editor/Scripting/ProjectScriptBuildSystem.h"

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

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <commdlg.h>
#endif

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
#include <unordered_map>
#include <functional>
#include <iomanip>
#include <atomic>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <cctype>
#endif

namespace NoJob
{
    // Set during Init when there is no valid saved docking tree.
    // DockSpaceOverViewport itself creates a root node, so checking for a node
    // after that call cannot tell us whether a saved layout existed.
    static bool s_BuildDefaultDockLayout = false;
    static bool s_ShowBTAssetEditor = false;
    static void DrawBTAssetEditorWindow(bool* opened);
    namespace
    {
#ifdef _WIN32
        std::vector<std::string> s_ScriptConsole;
        std::mutex s_ScriptConsoleMutex;
        bool s_ScriptsCompiled = false;
        std::thread s_StandaloneBuildThread;
        std::atomic_bool s_StandaloneBuildRunning{ false };
        std::atomic_bool s_StandaloneBuildSucceeded{ false };
        std::filesystem::path s_LastStandaloneBuildDirectory;
        BuildSettings s_BuildSettings;
        bool s_ShowBuildSettings = false;
        char s_BuildNameBuffer[128]{};

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
                            (installPath.back() == L'\r' || installPath.back() == L'\n' ||
                                installPath.back() == L' ' || installPath.back() == L'\t'))
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

        bool BuildStandalone()
        {
            if (s_StandaloneBuildRunning.exchange(true))
            {
                ScriptLog("[Build] Standalone build is already running.");
                return false;
            }

            const auto root = AssetManager::GetProjectRoot();
            const auto cmake = FindCMakeExecutable();
            if (cmake.empty())
            {
                ScriptLog("[Build] CMake was not found.");
                s_StandaloneBuildRunning = false;
                return false;
            }

            s_StandaloneBuildSucceeded = false;
            if (s_StandaloneBuildThread.joinable())
                s_StandaloneBuildThread.join();

            const BuildSettings settings = s_BuildSettings;
            s_StandaloneBuildThread = std::thread([root, cmake, settings]()
                {
                    const auto result = StandaloneBuilder::Build(
                        root,
                        cmake,
                        settings,
                        [](std::string message) { ScriptLog(std::move(message)); },
                        [](const std::filesystem::path& executable,
                            const std::vector<std::wstring>& arguments,
                            const std::string& prefix)
                        {
                            return RunProcessToScriptConsole(executable, arguments, prefix);
                        });

                    s_StandaloneBuildSucceeded = result.Succeeded;
                    if (result.Succeeded)
                        s_LastStandaloneBuildDirectory = result.OutputDirectory;
                    s_StandaloneBuildRunning = false;
                });
            return true;
        }

        bool CompileProjectScripts()
        {
            if (ProjectScriptBuildSystem::IsRunning())
            {
                ScriptLog("[Scripts] Compilation is already running.");
                return false;
            }

            // ScriptRegistry/DLL lifecycle remains on the editor thread.
            ProjectScriptManager::Unload();
            ClearScriptLog();
            ScriptLog("[Scripts] Starting asynchronous script compilation...");
            return ProjectScriptBuildSystem::Start(AssetManager::GetProjectRoot());
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
                { 0.0f, halfCylinder, 0.0f },
                viewProjection, viewportMin, viewportSize, color, thickness);
            DrawColliderEllipse(drawList, world, radius, radius, 1,
                { 0.0f,-halfCylinder, 0.0f },
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

        // Editor-only AI perception overlay. Geometry is expressed in world units,
        // so DetectionRadius is not accidentally multiplied by entity scale.
        void DrawPerceptionWire(
            ImDrawList* drawList,
            const glm::mat4& world,
            const PerceptionComponent& perception,
            const glm::mat4& viewProjection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            ImU32 radiusColor,
            ImU32 fovColor,
            float thickness)
        {
            if (!perception.Enabled || !perception.DebugDraw)
                return;

            const float radius = std::max(0.0f, perception.DetectionRadius);
            if (radius <= 0.0001f)
                return;

            const glm::vec3 origin = glm::vec3(world[3]) +
                glm::vec3(0.0f, std::max(0.0f, perception.EyeHeight), 0.0f);
            glm::vec3 forward = glm::vec3(world * glm::vec4(0, 0, -1, 0));
            forward.y = 0.0f;
            if (glm::length(forward) < 0.0001f)
                forward = { 0, 0, -1 };
            else
                forward = glm::normalize(forward);

            // Horizontal right vector; independent of nonuniform object scale.
            const glm::vec3 right = glm::normalize(
                glm::cross(forward, glm::vec3(0, 1, 0)));

            constexpr int circleSegments = 64;
            for (int i = 0; i < circleSegments; ++i)
            {
                const float a = glm::two_pi<float>() * float(i) / float(circleSegments);
                const float b = glm::two_pi<float>() * float(i + 1) / float(circleSegments);
                const glm::vec3 pa = origin + radius *
                    (forward * std::cos(a) + right * std::sin(a));
                const glm::vec3 pb = origin + radius *
                    (forward * std::cos(b) + right * std::sin(b));
                DrawColliderLine(drawList, pa, pb, viewProjection,
                    viewportMin, viewportSize, radiusColor, thickness);
            }

            const float halfFov = glm::radians(
                glm::clamp(perception.FieldOfView, 0.0f, 360.0f) * 0.5f);
            constexpr int arcSegments = 32;
            glm::vec3 previous{};
            for (int i = 0; i <= arcSegments; ++i)
            {
                const float t = float(i) / float(arcSegments);
                const float angle = -halfFov + (2.0f * halfFov) * t;
                const glm::vec3 point = origin + radius *
                    (forward * std::cos(angle) + right * std::sin(angle));
                if (i > 0)
                    DrawColliderLine(drawList, previous, point, viewProjection,
                        viewportMin, viewportSize, fovColor, thickness);
                if (i == 0 || i == arcSegments)
                    DrawColliderLine(drawList, origin, point, viewProjection,
                        viewportMin, viewportSize, fovColor, thickness);
                previous = point;
            }
        }

        void DrawSceneColliderGizmos(
            Scene& scene,
            Entity selectedEntity,
            const glm::mat4& view,
            const glm::mat4& projection,
            const ImVec2& viewportMin,
            const ImVec2& viewportSize,
            bool isPlaying,
            bool showAIDebugger,
            bool showLastSeenMarker,
            std::uint32_t debugAgent)
        {
            if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f)
                return;

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(
                viewportMin,
                { viewportMin.x + viewportSize.x,
                 viewportMin.y + viewportSize.y },
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

                if (!isPlaying && entity.HasComponent<BoxColliderComponent>())
                {
                    const auto& c =
                        entity.GetComponent<BoxColliderComponent>();
                    DrawBoxColliderWire(
                        drawList, world, c.Size,
                        viewProjection, viewportMin, viewportSize,
                        c.IsTrigger ? triggerColor : normalColor,
                        thickness);
                }

                if (!isPlaying && entity.HasComponent<SphereColliderComponent>())
                {
                    const auto& c =
                        entity.GetComponent<SphereColliderComponent>();
                    DrawSphereColliderWire(
                        drawList, world, c.Radius,
                        viewProjection, viewportMin, viewportSize,
                        c.IsTrigger ? triggerColor : normalColor,
                        thickness);
                }

                if (!isPlaying && entity.HasComponent<CapsuleColliderComponent>())
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
                    ColliderTransformPoint(world, { 0.0f, 0.0f, 0.0f });
                glm::vec3 forward =
                    ColliderTransformPoint(world, { 0.0f, 0.0f, -1.0f }) -
                    origin;
                if (glm::length(forward) > 0.0001f)
                    forward = glm::normalize(forward);
                else
                    forward = { 0.0f, 0.0f, -1.0f };
                if (entity.HasComponent<PerceptionComponent>())
                {
                    const auto& perception =
                        entity.GetComponent<PerceptionComponent>();

                    // Dibujamos el radio y el FOV como hasta ahora.
                    DrawPerceptionWire(
                        drawList, world, perception,
                        viewProjection, viewportMin, viewportSize,
                        selected ? IM_COL32(75, 205, 255, 255)
                        : IM_COL32(65, 155, 205, 115),
                        selected ? IM_COL32(255, 165, 65, 255)
                        : IM_COL32(235, 140, 60, 170),
                        thickness);

                    // Draw runtime perception results for every agent while
                    // the AI Debugger is open, not just the selected entity.
                    // Each agent queries its own perception state by handle.
                    if (isPlaying && showAIDebugger &&
                        perception.Enabled && perception.DebugDraw)
                    {
                        const auto targets =
                            scene.GetPerceivedTargets(entity.GetHandle());

                        for (const auto& target : targets)
                        {
                            // Los objetivos visibles se dibujan en su
                            // posición actual; los recordados, en la última
                            // posición que conocía el agente.
                            glm::vec3 markerPosition =
                                target.LastKnownPosition;

                            if (target.IsVisible &&
                                scene.IsValid(target.EntityHandle))
                            {
                                markerPosition = glm::vec3(
                                    scene.GetWorldTransform(
                                        Entity(target.EntityHandle, &scene))[3]);
                            }

                            // Actual LOS endpoint, using the same eye offset as runtime.
                            const glm::vec3 eye = glm::vec3(world[3]) +
                                glm::vec3(0.0f, std::max(0.0f, perception.EyeHeight), 0.0f);
                            const glm::vec3 aim = markerPosition +
                                glm::vec3(0.0f, std::max(0.0f, perception.EyeHeight), 0.0f);
                            const ImU32 rayColor = target.IsVisible
                                ? IM_COL32(50, 230, 110, 255)
                                : IM_COL32(255, 205, 65, 210);
                            DrawColliderLine(drawList, eye, aim, viewProjection,
                                viewportMin, viewportSize, rayColor, 2.0f);

                            bool onScreen = false;

                            const ImVec2 screenPosition =
                                ProjectColliderPoint(
                                    markerPosition,
                                    viewProjection,
                                    viewportMin,
                                    viewportSize,
                                    onScreen);

                            if (!onScreen)
                                continue;

                            const ImU32 markerColor = target.IsVisible
                                ? IM_COL32(50, 230, 110, 255)
                                : IM_COL32(255, 205, 65, 255);

                            drawList->AddCircleFilled(
                                screenPosition,
                                7.0f,
                                markerColor,
                                16);

                            drawList->AddCircle(
                                screenPosition,
                                10.0f,
                                markerColor,
                                16,
                                2.0f);
                        }
                    }
                }

                // Optional last-seen marker: driven exclusively by the AI Debugger.
                if (isPlaying && showAIDebugger && showLastSeenMarker &&
                    entity.GetHandle() == debugAgent)
                {
                    const auto lastSeen = scene.GetLastSeenTarget(entity.GetHandle());
                    if (lastSeen)
                    {
                        const glm::vec3 p = lastSeen->Position;
                        const ImU32 yellow = IM_COL32(255, 205, 65, 255);
                        constexpr float radius = 0.35f;
                        constexpr int segments = 24;
                        for (int i = 0; i < segments; ++i)
                        {
                            const float a = glm::two_pi<float>() * float(i) / float(segments);
                            const float b = glm::two_pi<float>() * float(i + 1) / float(segments);
                            DrawColliderLine(drawList,
                                p + glm::vec3(std::cos(a) * radius, 0.05f, std::sin(a) * radius),
                                p + glm::vec3(std::cos(b) * radius, 0.05f, std::sin(b) * radius),
                                viewProjection, viewportMin, viewportSize, yellow, 2.5f);
                        }
                        DrawColliderLine(drawList, p + glm::vec3(0, 0.05f, 0),
                            p + glm::vec3(0, 1.2f, 0), viewProjection,
                            viewportMin, viewportSize, yellow, 2.5f);
                        bool onScreen = false;
                        const ImVec2 projected = ProjectColliderPoint(p + glm::vec3(0, 1.35f, 0),
                            viewProjection, viewportMin, viewportSize, onScreen);
                        if (onScreen)
                            drawList->AddText(projected, yellow, "LAST SEEN");
                    }
                }

                // During Play only AI perception debug overlays are drawn.
                if (isPlaying)
                    continue;

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
                        if (glm::length(dir) < 0.0001f) dir = { 0,1,0 };
                        dir = glm::normalize(glm::mat3(world) * glm::normalize(dir));
                        const float length = 1.25f;
                        const float radius = std::tan(glm::radians(
                            glm::clamp(particles.ConeAngle, 0.0f, 89.0f))) * length;
                        glm::vec3 tangent = std::abs(dir.y) < 0.99f
                            ? glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)))
                            : glm::vec3(1, 0, 0);
                        glm::vec3 bitangent = glm::normalize(glm::cross(dir, tangent));
                        const glm::vec3 center = origin + dir * length;
                        for (int i = 0;i < 4;++i)
                        {
                            const float a = glm::half_pi<float>() * float(i);
                            const glm::vec3 edge = center +
                                tangent * std::cos(a) * radius +
                                bitangent * std::sin(a) * radius;
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
                    const glm::vec3 tipLocal{ 0.0f, 0.0f, 0.0f };
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
                        { 0.0f, 0.0f,-length },
                        viewProjection, viewportMin, viewportSize,
                        lightColor, thickness);
                }
            }

            drawList->PopClipRect();
        }

        std::string OpenModelFileDialog() {
#ifdef _WIN32
            char f[MAX_PATH]{};OPENFILENAMEA d{};d.lStructSize = sizeof(d);d.lpstrFile = f;d.nMaxFile = MAX_PATH;d.lpstrFilter =
                "3D Models\0*.obj;*.fbx;*.gltf;*.glb;*.dae;*.stl;*.ply;*.3ds;*.blend\0"
                "glTF / GLB\0*.gltf;*.glb\0"
                "FBX\0*.fbx\0"
                "Wavefront OBJ\0*.obj\0"
                "All Files\0*.*\0";d.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;if (GetOpenFileNameA(&d) == TRUE)return f;
#endif
            return{};
        }
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
        s_BuildSettings = BuildSettings::Load(AssetManager::GetProjectRoot());
        std::snprintf(s_BuildNameBuffer, sizeof(s_BuildNameBuffer), "%s", s_BuildSettings.BuildName.c_str());
#ifdef _WIN32
        ProjectScriptManager::SetLogger(ScriptLog);
        ProjectScriptBuildSystem::SetLogger(ScriptLog);
#endif
        m_Scene = scene;

        const auto projectRoot = FindNoJobProjectRoot();
        AssetManager::Init(projectRoot);
        std::filesystem::create_directories(
            AssetManager::GetAssetsDirectory() / "Scripts");
        m_ProjectPanel.Initialize();

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
#ifdef _WIN32
        ProjectScriptBuildSystem::Shutdown();
        if (s_StandaloneBuildThread.joinable())
            s_StandaloneBuildThread.join();
        ProjectScriptManager::Unload();
#endif
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

    static NeuralEditorPanel s_NeuralEditorPanel;

    void EditorLayer::Draw()
    {
#ifdef _WIN32
        if (ProjectScriptBuildSystem::ConsumeReloadPending())
        {
            if (ProjectScriptManager::Load(AssetManager::GetProjectRoot()))
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
        DrawAIDebugger();
        s_NeuralEditorPanel.Draw();
        if (s_ShowBTAssetEditor) DrawBTAssetEditorWindow(&s_ShowBTAssetEditor);

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
                auto materials = AssetManager::ImportModelMaterials(
                    p, m_DefaultCubeMaterial->GetShader());
                if (materials.empty())
                {
                    material = std::make_shared<Material>(*m_DefaultCubeMaterial);
                    material->UseTexture() = false;
                    materials.push_back(material);
                }
                else material = materials.front();

                entity.AddComponent<MeshRendererComponent>(material);
                entity.GetComponent<MeshRendererComponent>()
                    .SetMaterials(std::move(materials));
            }

            // Unity-style import: if the model contains an Assimp skeleton/animation,
            // automatically attach an Animator so the clips are immediately visible.
            try
            {
                const auto animationPath = AssetManager::ResolveProjectPath(p);
                auto animation = AnimationAsset::Load(animationPath);
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
            catch (...)
            {
                ScriptLog("[Import] Model animation discovery failed; model imported without Animator.");
            }

            m_SelectedEntity = entity;
            return entity;
        }
        catch (...)
        {
            ScriptLog("[Import] Model entity creation failed.");
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

                if (source.HasComponent<PerceptionComponent>())
                    copy.AddComponent<PerceptionComponent>(
                        source.GetComponent<PerceptionComponent>());
                if (source.HasComponent<NavAgentComponent>())
                    copy.AddComponent<NavAgentComponent>(
                        source.GetComponent<NavAgentComponent>());
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
                if (ImGui::MenuItem("Save Scene As..."))
                    m_SaveSceneAsRequested = true;
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
                    m_LoadSceneRequested = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Load Graphics Test Scene"))
                    m_GraphicsTestSceneRequested = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Import 3D Model...")) { auto s = OpenModelFileDialog();if (!s.empty())try { CreateModelEntity(AssetManager::ImportModel(s)); } catch (...) { ScriptLog("[Import] 3D model import failed."); } }
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

                ImGui::Separator();
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

            if (s_ShowBuildSettings) ImGui::OpenPopup("Build Settings");
            if (ImGui::BeginPopupModal("Build Settings", &s_ShowBuildSettings, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::InputText("Build Name", s_BuildNameBuffer, sizeof(s_BuildNameBuffer)); s_BuildSettings.BuildName = s_BuildNameBuffer;
                ImGui::SeparatorText("Scenes in Build"); int remove = -1;
                for (size_t i = 0;i < s_BuildSettings.Scenes.size();++i) { ImGui::PushID((int)i); bool startup = i == s_BuildSettings.StartupSceneIndex; if (ImGui::RadioButton("##startup", startup))s_BuildSettings.StartupSceneIndex = i; ImGui::SameLine(); ImGui::TextUnformatted(s_BuildSettings.Scenes[i].generic_string().c_str()); ImGui::SameLine(); if (ImGui::SmallButton("Remove"))remove = (int)i; ImGui::PopID(); }
                if (remove >= 0) { s_BuildSettings.Scenes.erase(s_BuildSettings.Scenes.begin() + remove); if (s_BuildSettings.Scenes.empty())s_BuildSettings.StartupSceneIndex = 0; else if (s_BuildSettings.StartupSceneIndex >= s_BuildSettings.Scenes.size())s_BuildSettings.StartupSceneIndex = s_BuildSettings.Scenes.size() - 1; }
                if (ImGui::Button("Add Scene...")) { auto selected = SceneFileDialog::Open(AssetManager::GetProjectRoot() / "Assets/Scenes"); if (!selected.empty()) { std::error_code ec;auto rel = std::filesystem::relative(selected, AssetManager::GetProjectRoot(), ec);if (!ec && std::find(s_BuildSettings.Scenes.begin(), s_BuildSettings.Scenes.end(), rel) == s_BuildSettings.Scenes.end())s_BuildSettings.Scenes.push_back(rel); } }
                ImGui::SeparatorText("Configuration"); bool debug = s_BuildSettings.Configuration == L"Debug"; if (ImGui::RadioButton("Debug", debug))s_BuildSettings.Configuration = L"Debug"; ImGui::SameLine(); if (ImGui::RadioButton("Release", !debug))s_BuildSettings.Configuration = L"Release";
                ImGui::Separator();
                if (ImGui::Button("Save Settings")) { s_BuildSettings.Save(AssetManager::GetProjectRoot());ScriptLog("[Build] Build Settings saved."); } ImGui::SameLine();
                if (ImGui::Button("Build")) { std::string error;if (!s_BuildSettings.IsValid(error))ScriptLog("[Build] " + error);else { s_BuildSettings.Save(AssetManager::GetProjectRoot());s_ShowBuildSettings = false;ImGui::CloseCurrentPopup();BuildStandalone(); } } ImGui::SameLine();
                if (ImGui::Button("Close")) { s_ShowBuildSettings = false;ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

            if (ImGui::BeginMenu("Build"))
            {
#ifdef _WIN32
                ImGui::BeginDisabled(ProjectScriptBuildSystem::IsRunning());
                if (ImGui::MenuItem("Compile Scripts", "Ctrl+Shift+B"))
                    CompileProjectScripts();
                ImGui::EndDisabled();
                ImGui::BeginDisabled(ProjectScriptBuildSystem::IsRunning());
                if (ImGui::MenuItem("Reload Scripts"))
                    ProjectScriptManager::Load(AssetManager::GetProjectRoot());
                ImGui::EndDisabled();
                if (ProjectScriptBuildSystem::IsRunning())
                    ImGui::TextDisabled("Compiling scripts...");
                ImGui::Separator();
                ImGui::BeginDisabled(
                    ProjectScriptBuildSystem::IsRunning() || s_StandaloneBuildRunning.load());
                if (ImGui::MenuItem("Build Settings...")) s_ShowBuildSettings = true;
                if (ImGui::MenuItem("Build Project")) BuildStandalone();
                ImGui::EndDisabled();
                if (s_StandaloneBuildRunning.load())
                    ImGui::TextDisabled("Building standalone...");
                if (s_StandaloneBuildSucceeded.load() &&
                    !s_LastStandaloneBuildDirectory.empty())
                    ImGui::TextDisabled("Last build: %s",
                        s_LastStandaloneBuildDirectory.string().c_str());
                ImGui::Separator();
                ImGui::TextDisabled(ProjectScriptManager::IsLoaded()
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
                            catch (...)
                            {
                                ScriptLog("[Import] 3D model import failed.");
                            }
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
                    ProjectAssetOperations::CreatePrefab(
                        m_SelectedEntity,
                        AssetManager::GetAssetsDirectory() / "Prefabs",
                        [this](const std::string& message) { ScriptLog(message); });
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
            if (ImGui::BeginMenu("AI"))
            {
                ImGui::MenuItem("Behavior Tree Editor", nullptr, &s_ShowBTAssetEditor);
                if (ImGui::MenuItem("Neural Network Editor", nullptr, s_NeuralEditorPanel.IsOpen()))
                    s_NeuralEditorPanel.SetOpen(!s_NeuralEditorPanel.IsOpen());
                ImGui::MenuItem("AI Debugger", nullptr, &m_ShowAIDebugger);
                ImGui::Separator();
                if (ImGui::BeginMenu("Navigation"))
                {
                    if (ImGui::MenuItem(
                        "Bake NavMesh",
                        nullptr,
                        false,
                        !m_IsPlaying))
                    {
                        m_BakeNavMeshRequested = true;
                    }

                    if (ImGui::MenuItem(
                        "Clear NavMesh",
                        nullptr,
                        false,
                        !m_IsPlaying))
                    {
                        m_ClearNavMeshRequested = true;
                    }

                    ImGui::Separator();

                    ImGui::MenuItem(
                        "Show NavMesh",
                        nullptr,
                        &m_ShowNavMesh);

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
                ImGui::MenuItem("AI Debugger", nullptr, &m_ShowAIDebugger);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void EditorLayer::SetScene(Scene* scene)
    {
        m_Scene = scene;
        m_SelectedEntity = {};
        m_AIDebugAgent = 0;
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
    bool EditorLayer::ConsumeBakeNavMeshRequest()
    {
        const bool value = m_BakeNavMeshRequested;
        m_BakeNavMeshRequested = false;
        return value;
    }

    bool EditorLayer::ConsumeClearNavMeshRequest()
    {
        const bool value = m_ClearNavMeshRequested;
        m_ClearNavMeshRequested = false;
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
            [](unsigned char c) { return (char)std::tolower(c); });

        std::string name = entity.GetComponent<TagComponent>().Tag;
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
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
                ProjectAssetOperations::CreatePrefab(
                    entity,
                    AssetManager::GetAssetsDirectory() / "Prefabs",
                    [this](const std::string& message) { ScriptLog(message); });
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

        // Gizmos are editor overlays, never part of the game framebuffer.
        // In Play, draw only AI perception debugging (when enabled).
        if (m_Scene && m_SelectedEntity)
        {
            DrawSceneColliderGizmos(
                *m_Scene,
                m_SelectedEntity,
                m_EditorView,
                m_EditorProjection,
                viewportMin,
                ImVec2(m_ViewportWidth, m_ViewportHeight),
                m_IsPlaying,
                m_ShowAIDebugger,
                m_AIDebugShowLastSeenMarker,
                m_AIDebugAgent);
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
                { previewMin.x - 3.0f, previewMin.y - 3.0f },
                { previewMax.x + 3.0f, previewMax.y + 3.0f },
                IM_COL32(25, 25, 28, 240));
            drawList->AddImage(
                static_cast<ImTextureID>(
                    static_cast<intptr_t>(m_CameraPreviewTextureID)),
                previewMin,
                previewMax,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
            drawList->AddText(
                { previewMin.x + 8.0f, previewMin.y + 6.0f },
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


    std::string EditorLayer::OpenInspectorAudioFileDialog()
    {
        return OpenAudioFileDialog();
    }

    std::string EditorLayer::OpenInspectorTextureFileDialog()
    {
        return OpenTextureFileDialog();
    }

    void EditorLayer::LogInspectorMessage(const std::string& message)
    {
        ScriptLog(message);
    }

    void EditorLayer::DrawProjectPanel()
    {
        m_ProjectPanel.Draw(
            m_Scene,
            m_SelectedEntity,
            m_DefaultCubeMesh,
            m_DefaultCubeMaterial,
            [this](const std::filesystem::path& path) { CreateModelEntity(path); },
            ScriptLog);
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
                    selected, m_ProjectPanel.GetCurrentDirectoryPath(), ec);
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

        const char* environmentSizes[] = { "128", "256", "512", "1024" };
        int environmentSizeIndex =
            m_GraphicsSettings.EnvironmentResolution <= 128 ? 0 :
            m_GraphicsSettings.EnvironmentResolution <= 256 ? 1 :
            m_GraphicsSettings.EnvironmentResolution <= 512 ? 2 : 3;
        if (ImGui::Combo(
            "Environment Resolution", &environmentSizeIndex,
            environmentSizes, 4))
        {
            const std::uint32_t sizes[] = { 128, 256, 512, 1024 };
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


    // V1.9 native Behavior Tree asset graph editor. Nodes are stored in stable
    // parent-first order; editor coordinates are derived from depth/order.
    static void DrawBTAssetEditorWindow(bool* opened)
    {
        static BTAsset asset = BTAsset::Default();
        static std::string currentName = "Default";
        static char assetName[128] = "Default";
        static int selected = 1;
        // Per-parent child creation drafts. A draft is not a property of the
        // currently selected node and must not leak between selections.
        struct ChildDraft { int Kind = 0; char Native[128] = "StopMoving"; };
        static std::unordered_map<BTNodeID, ChildDraft> childDrafts;
        static std::string childDraftAsset;
        static std::string message;
        static bool dirty = false;
        // Editor-only callback catalog. Never construct project-script instances here:
        // project DLLs may have a different C++ ABI / lifetime than the editor.
        // Names are metadata; execution callbacks belong exclusively to the runtime.
        const auto registeredCallbacks = []() {
            std::vector<std::string> actions{
                "ChaseTarget", "MoveForward", "Patrol", "StopMoving"
            };
            std::vector<std::string> conditions{
                "CanSeeTarget", "HasRememberedTarget"
            };
            // Metadata is copied out of ScriptRegistry; never instantiate scripts,
            // build a temporary BTNativeRegistry, or capture DLL callbacks here.
            for (const auto& entry : ProjectScriptManager::BehaviorTreeCallbackCatalog()) {
                if (entry.Name.empty()) continue;
                auto& destination = entry.IsCondition ? conditions : actions;
                destination.push_back(entry.Name);
            }
            auto normalize = [](std::vector<std::string>& names) {
                std::sort(names.begin(), names.end());
                names.erase(std::unique(names.begin(), names.end()), names.end());
            };
            normalize(actions);
            normalize(conditions);
            return std::make_pair(std::move(actions), std::move(conditions));
        }();
        const auto callbackPicker = [&](const char* label, const char* kind, std::string& value) {
            const bool condition = std::string_view(kind) == "Condition";
            const auto& names = condition ? registeredCallbacks.second : registeredCallbacks.first;
            if (ImGui::BeginCombo(label, value.c_str())) {
                for (const auto& name : names) {
                    if (ImGui::Selectable(name.c_str(), value == name)) {
                        if (value != name) { value = name; dirty = true; }
                    }
                }
                ImGui::EndCombo();
            }
            if (std::find(names.begin(), names.end(), value) == names.end() && value != "-")
                ImGui::TextDisabled("Custom/unavailable callback: %s", value.c_str());
        };
        if (!ImGui::Begin("Behavior Tree Editor", opened)) { ImGui::End(); return; }
        const std::filesystem::path folder = BTAssetDirectory();
        const auto validName = [](const std::string& name) {
            if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') return false;
            return name.find_first_of("/\\:*?\"<>|") == std::string::npos;
        };
        const auto assetPath = [&](const std::string& name) { return BTAssetPath(name); };
        ImGui::TextUnformatted("Native Decision System - V1.9");
        ImGui::Text("Current asset: %s%s",currentName.c_str(),dirty ? " *" : "");
        const std::vector<std::string> available = ListBTAssetNames();
        // Restore the last opened asset on the first editor frame. Previously
        // BTAsset::Default() was displayed even when a saved .njbt existed.
        static bool initialAssetLoaded = false;
        const auto lastAssetPath = folder / ".last-opened-bt";
        const auto rememberAsset = [&](const std::string& name) {
            std::error_code ec;
            std::filesystem::create_directories(folder, ec);
            if (ec) return;
            std::ofstream out(lastAssetPath, std::ios::trunc);
            if (out) out << name << '\n';
        };
        if (!initialAssetLoaded) {
            initialAssetLoaded = true;
            std::string preferred;
            { std::ifstream in(lastAssetPath); std::getline(in, preferred); }
            const auto existsInList = [&](const std::string& name) {
                return std::find(available.begin(), available.end(), name) != available.end();
            };
            if (!existsInList(preferred))
                preferred = existsInList(currentName) ? currentName
                    : (available.empty() ? std::string{} : available.front());
            if (!preferred.empty()) {
                try {
                    BTAsset loaded = BTAsset::Load(assetPath(preferred));
                    asset = std::move(loaded);
                    currentName = preferred;
                    std::snprintf(assetName, sizeof(assetName), "%s", preferred.c_str());
                    selected = static_cast<int>(asset.Nodes.front().ID);
                    dirty = false;
                    message = "Restored " + assetPath(preferred).string();
                } catch (const std::exception& ex) {
                    message = "Could not restore saved tree: " + std::string(ex.what());
                }
            }
        }
        if (ImGui::BeginCombo("Saved trees",currentName.c_str())) {
            for (const auto& name : available) {
                if (ImGui::Selectable(name.c_str(),name == currentName)) {
                    try {
                        auto loaded = BTAsset::Load(assetPath(name));
                        asset = std::move(loaded); currentName = name;
                        std::snprintf(assetName,sizeof(assetName),"%s",name.c_str());
                        selected = static_cast<int>(asset.Nodes.front().ID);
                        dirty = false; rememberAsset(name); message = "Loaded " + assetPath(name).string();
                    } catch (const std::exception& ex) { message = ex.what(); }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputText("Tree name",assetName,sizeof(assetName));
        if (ImGui::Button("Load")) {
            try {
                auto loaded = BTAsset::Load(assetPath(assetName));
                asset = std::move(loaded); currentName = assetName;
                selected = static_cast<int>(asset.Nodes.front().ID);
                dirty = false; rememberAsset(currentName); message = "Loaded " + assetPath(currentName).string();
            } catch (const std::exception& ex) { message = ex.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            try {
                if (!validName(currentName)) throw std::runtime_error("Invalid tree name");
                asset.Save(assetPath(currentName)); dirty = false; rememberAsset(currentName);
                message = "Saved " + std::filesystem::absolute(assetPath(currentName)).string();
            } catch (const std::exception& ex) { message = ex.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As")) {
            try {
                const std::string name(assetName);
                if (!validName(name)) throw std::runtime_error("Invalid tree name");
                if (name != currentName && std::filesystem::exists(assetPath(name)))
                    throw std::runtime_error("A tree with that name already exists");
                asset.Save(assetPath(name)); currentName = name; dirty = false; rememberAsset(name);
                message = "Saved " + std::filesystem::absolute(assetPath(name)).string();
            } catch (const std::exception& ex) { message = ex.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("New")) {
            asset = BTAsset::Default(); selected = 1; dirty = true;
            currentName = "Untitled";
            std::snprintf(assetName,sizeof(assetName),"%s",currentName.c_str());
            message = "New tree. Enter a name and click Save As.";
        }
        if (!message.empty()) ImGui::TextWrapped("%s",message.c_str());
        ImGui::Separator();
        // Layout lives in an editor-only sidecar. Runtime .njbt format stays unchanged.
        static std::unordered_map<BTNodeID, ImVec2> layout;
        static std::string layoutAsset;
        static bool layoutDirty = false;
        const auto layoutPath = [&](const std::string& name) { return folder / (name + ".njbt.layout"); };
        const auto loadLayout = [&](const std::string& name) {
            layout.clear();
            std::ifstream in(layoutPath(name));
            BTNodeID id; float x, y;
            while (in >> id >> x >> y) layout[id] = ImVec2(x,y);
            layoutDirty = false;
        };
        const auto saveLayout = [&](const std::string& name) {
            std::filesystem::create_directories(folder);
            std::ofstream out(layoutPath(name), std::ios::trunc);
            if (!out) throw std::runtime_error("Cannot save visual layout");
            for (const auto& [id, pos] : layout) out << id << ' ' << pos.x << ' ' << pos.y << '\n';
            if (!out) throw std::runtime_error("Cannot write visual layout");
            layoutDirty = false;
        };
        if (layoutAsset != currentName) { loadLayout(currentName); layoutAsset = currentName; }
        if (ImGui::Button("Save Layout")) {
            try { saveLayout(currentName); message = "Layout saved for " + currentName; }
            catch (const std::exception& e) { message = e.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Auto Arrange")) { layout.clear(); layoutDirty = true; }
        ImGui::SameLine();
        ImGui::TextDisabled("Drag nodes to arrange | Layout: %s", layoutDirty ? "unsaved" : "saved");
        ImGui::BeginChild("BTCanvas", ImVec2(0, -155), true, ImGuiWindowFlags_HorizontalScrollbar);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        constexpr float w = 188.f, h = 68.f, dx = 240.f, dy = 105.f;
        std::unordered_map<BTNodeID,ImVec2> positions;
        for (std::size_t i=0; i<asset.Nodes.size(); ++i) {
            const auto& n=asset.Nodes[i];
            int depth=0; BTNodeID ancestor=n.Parent;
            while (ancestor && depth<32) {
                ++depth; BTNodeID next=0;
                for (const auto& parent:asset.Nodes) if (parent.ID==ancestor) {next=parent.Parent;break;}
                ancestor=next;
            }
            if (!layout.contains(n.ID)) {
                layout[n.ID] = ImVec2(24.f + depth*dx, 22.f + static_cast<float>(i)*dy);
                layoutDirty = true;
            }
            positions[n.ID] = ImVec2(origin.x + layout.at(n.ID).x, origin.y + layout.at(n.ID).y);
        }
        auto* draw=ImGui::GetWindowDrawList();
        const ImU32 wireColor = IM_COL32(130,150,175,220);
        for (const auto& n:asset.Nodes) if (n.Parent && positions.contains(n.Parent)) {
            const auto a=positions.at(n.Parent), b=positions.at(n.ID);
            draw->AddBezierCubic(ImVec2(a.x+w,a.y+h/2), ImVec2(a.x+w+50,a.y+h/2),
                ImVec2(b.x-50,b.y+h/2),ImVec2(b.x,b.y+h/2),wireColor,2.5f);
        }
        float maxX=800.f, maxY=400.f;
        for (const auto& n:asset.Nodes) {
            const ImVec2 at=positions.at(n.ID);
            ImGui::SetCursorScreenPos(at);
            ImGui::PushID(static_cast<int>(n.ID));
            const bool active=selected==static_cast<int>(n.ID);
            const ImVec4 tint = n.Kind=="Decorator" ? ImVec4(.58f,.31f,.13f,1.f)
                : n.Kind=="Service" ? ImVec4(.16f,.43f,.55f,1.f)
                : n.Kind=="Parallel" ? ImVec4(.47f,.25f,.49f,1.f)
                : n.Kind=="Selector" ? ImVec4(.38f,.22f,.56f,1.f)
                : n.Kind=="Sequence" ? ImVec4(.20f,.38f,.66f,1.f)
                : n.Kind=="Condition" ? ImVec4(.65f,.43f,.14f,1.f)
                : ImVec4(.17f,.48f,.37f,1.f);
            ImGui::PushStyleColor(ImGuiCol_Button,tint);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(std::min(tint.x+.12f,1.f),std::min(tint.y+.12f,1.f),std::min(tint.z+.12f,1.f),1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,tint);
            std::string label = n.Kind + "##node";
            if (ImGui::Button(label.c_str(),ImVec2(w,h))) selected=static_cast<int>(n.ID);
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left,2.f)) {
                const auto delta=ImGui::GetIO().MouseDelta;
                auto& pos=layout[n.ID];
                pos.x=std::max(0.f,pos.x+delta.x);
                pos.y=std::max(0.f,pos.y+delta.y);
                layoutDirty=true;
                selected=static_cast<int>(n.ID);
            }
            ImGui::PopStyleColor(3);
            if (active) draw->AddRect(ImVec2(at.x-3,at.y-3),ImVec2(at.x+w+3,at.y+h+3),IM_COL32(245,220,125,255),7.f,0,2.5f);
            const std::string subtitle = (n.Native=="-" ? "Composite" : n.Native);
            draw->AddText(ImVec2(at.x+12,at.y+38),IM_COL32(240,244,249,255),subtitle.c_str());
            draw->AddCircleFilled(ImVec2(at.x+w,at.y+h/2),5.f,IM_COL32(225,232,245,255));
            if (n.Parent) draw->AddCircleFilled(ImVec2(at.x,at.y+h/2),5.f,IM_COL32(225,232,245,255));
            ImGui::PopID();
            maxX=std::max(maxX,layout.at(n.ID).x+w+55.f);
            maxY=std::max(maxY,layout.at(n.ID).y+h+55.f);
        }
        ImGui::SetCursorScreenPos(origin);
        ImGui::Dummy(ImVec2(maxX,maxY));
        ImGui::EndChild();
        ImGui::SeparatorText("Selected node");
        BTAssetNode* current=nullptr;
        for (auto& n:asset.Nodes) if (n.ID==static_cast<BTNodeID>(selected)) current=&n;
        if (current) {
            if (childDraftAsset != currentName) {
                childDrafts.clear();
                childDraftAsset = currentName;
            }
            ChildDraft& childDraft = childDrafts[current->ID];
            ImGui::Text("#%llu  %s",static_cast<unsigned long long>(current->ID),current->Kind.c_str());
            // This changes the SELECTED node; Child type below only affects new nodes.
            const char* nodeKinds[] = {"Sequence","Selector","Action","Condition","Decorator","Service","Parallel"};
            int selectedKind = 0;
            for (int k = 0; k < 7; ++k)
                if (current->Kind == nodeKinds[k]) { selectedKind = k; break; }
            if (ImGui::Combo("Node type##selected", &selectedKind, nodeKinds, 7)) {
                const bool hasChildren = std::any_of(asset.Nodes.begin(), asset.Nodes.end(),
                    [&](const BTAssetNode& n) { return n.Parent == current->ID; });
                const bool leaf = selectedKind == 2 || selectedKind == 3;
                if (leaf && hasChildren) {
                    message = "Remove child nodes before converting this node to Action/Condition.";
                } else {
                    current->Kind = nodeKinds[selectedKind];
                    if (selectedKind == 2 || selectedKind == 3) {
                        if (current->Native == "-" || current->Native.empty())
                            current->Native = selectedKind == 2 ? "StopMoving" : "CanSeeTarget";
                    } else if (selectedKind == 0 || selectedKind == 1 || selectedKind == 6) {
                        current->Native = "-";
                    } else if (current->Native == "-" || current->Native.empty()) {
                        current->Native = "StopMoving";
                    }
                    dirty = true;
                }
            }
            if (current->Kind=="Action" || current->Kind=="Condition" ||
                current->Kind=="Decorator" || current->Kind=="Service") {
                ImGui::Text("Native callback: %s",current->Native.c_str());
            }
            if (current->Kind=="Decorator" || current->Kind=="Service") {
                ImGui::TextDisabled("Decorator/Service must have exactly one child");
            }
            if (current->Kind=="Decorator" || current->Kind=="Service" ||
                current->Kind=="Action" || current->Kind=="Condition") {
                // The callback belongs to the selected BTAssetNode, never to a global field.
                // Reload the edit buffer on node/asset changes (including same ID in another tree).
                static BTNodeID editID = 0;
                static std::string editAsset;
                static std::string editKind;
                static char editNative[128] = {};
                static char editKey[128] = {};
                if (editID != current->ID || editAsset != currentName || editKind != current->Kind) {
                    editID = current->ID;
                    editAsset = currentName;
                    editKind = current->Kind;
                    std::snprintf(editNative, sizeof(editNative), "%s", current->Native.c_str());
                    std::snprintf(editKey, sizeof(editKey), "%s", current->Parameter.c_str());
                }
                callbackPicker("Registered callback##selected", current->Kind.c_str(), current->Native);
                // Synchronize after selecting a registered name.
                if (current->Native != editNative)
                    std::snprintf(editNative, sizeof(editNative), "%s", current->Native.c_str());
                if (ImGui::InputText("Native name##selected", editNative, sizeof(editNative))) {
                    current->Native = editNative;
                    dirty = true;
                }
                if (current->Kind=="Decorator" && ImGui::InputText("Observed Blackboard key",editKey,sizeof(editKey))) {
                    current->Parameter=editKey[0] ? editKey : "-"; dirty=true;
                }
                if (current->Kind=="Service" && ImGui::DragFloat("Interval (seconds)",&current->Interval,.05f,.05f,60.f)) {
                    current->Interval=std::max(.05f,current->Interval); dirty=true;
                }
            }
            const char* kinds[]={"Sequence","Selector","Action","Condition","Decorator","Service","Parallel"};
            ImGui::SeparatorText("Create child (not selected node properties)");
            ImGui::PushID(static_cast<int>(current->ID));
            ImGui::Combo("Child type", &childDraft.Kind, kinds, 7);
            if (childDraft.Kind >= 2 && childDraft.Kind != 6) {
                std::string proposed(childDraft.Native);
                callbackPicker("Registered callback##new", kinds[childDraft.Kind], proposed);
                if (proposed != childDraft.Native)
                    std::snprintf(childDraft.Native, sizeof(childDraft.Native), "%s", proposed.c_str());
                ImGui::InputText("New child native name", childDraft.Native, sizeof(childDraft.Native));
            }
            ImGui::PopID();
            if (ImGui::Button("Add child")) {
                if (current->Kind=="Action" || current->Kind=="Condition") message="Cannot add children to a leaf";
                else {
                    BTNodeID id=1; for (const auto& n:asset.Nodes) id=std::max(id,n.ID+1);
                    asset.Nodes.push_back({id,static_cast<BTNodeID>(selected),kinds[childDraft.Kind],
                        (childDraft.Kind>=2 && childDraft.Kind!=6)?childDraft.Native:"-"});
                    selected=static_cast<int>(id); dirty=true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete branch") && current->Parent) {
                std::unordered_set<BTNodeID> removed{current->ID};
                bool changed=true;
                while (changed) {
                    changed=false;
                    for (const auto& n:asset.Nodes) if (removed.contains(n.Parent) && !removed.contains(n.ID)) {
                        removed.insert(n.ID); changed=true;
                    }
                }
                asset.Nodes.erase(std::remove_if(asset.Nodes.begin(),asset.Nodes.end(),
                    [&](const BTAssetNode& n){return removed.contains(n.ID);}),asset.Nodes.end());
                selected=static_cast<int>(asset.Nodes.front().ID);
                for (BTNodeID id : removed) childDrafts.erase(id);
                dirty = true;
            }
        }
        ImGui::End();
    }

    void EditorLayer::DrawAIDebugger()
    {
        if (!m_ShowAIDebugger) return;
        if (!ImGui::Begin("AI Debugger", &m_ShowAIDebugger)) { ImGui::End(); return; }
        if (!m_Scene) { ImGui::TextDisabled("No scene loaded."); ImGui::End(); return; }

        ImGui::TextColored(m_IsPlaying ? ImVec4(0.3f, 0.9f, 0.5f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
            "%s", m_IsPlaying ? (m_IsPaused ? "PAUSED" : "LIVE - PLAY MODE") : "EDIT MODE - runtime data unavailable");
        ImGui::SameLine();
        ImGui::Checkbox("Active only", &m_AIDebugOnlyActive);
        ImGui::Separator();
        auto entities = m_Scene->GetEntities();
        std::vector<Entity> agents;
        int visibleTotal = 0, rememberedTotal = 0;
        for (auto entity : entities)
        {
            if (!entity.HasComponent<PerceptionComponent>() && !entity.HasComponent<NavAgentComponent>() &&
                (!entity.HasComponent<NativeScriptComponent>() || (entity.GetComponent<NativeScriptComponent>().ScriptName != "BTAssetAgent" && entity.GetComponent<NativeScriptComponent>().ScriptName != "NeuralTrainingAgent"))) continue;
            if (m_AIDebugOnlyActive && entity.HasComponent<PerceptionComponent>() && !entity.GetComponent<PerceptionComponent>().Enabled) continue;
            agents.push_back(entity);
            if (entity.HasComponent<PerceptionComponent>() && m_IsPlaying)
                for (const auto& target : m_Scene->GetPerceivedTargets(entity.GetHandle()))
                    target.IsVisible ? ++visibleTotal : ++rememberedTotal;
        }
        ImGui::Text("Agents: %d    Visible: %d    Remembered: %d", (int)agents.size(), visibleTotal, rememberedTotal);
        ImGui::Separator();
        if (ImGui::BeginChild("##AI_Agents", ImVec2(190, 0), true))
        {
            for (auto agent : agents)
            {
                const auto& name = agent.GetComponent<TagComponent>().Tag;
                const std::string label = name + "##AI" + std::to_string(agent.GetHandle());
                if (ImGui::Selectable(label.c_str(), m_AIDebugAgent == agent.GetHandle()))
                    m_AIDebugAgent = agent.GetHandle();
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (!m_Scene->IsValid(m_AIDebugAgent) ||
            std::none_of(agents.begin(), agents.end(), [&](Entity e) { return e.GetHandle() == m_AIDebugAgent; }))
            m_AIDebugAgent = agents.empty() ? 0 : agents.front().GetHandle();
        if (m_AIDebugAgent)
        {
            Entity agent(m_AIDebugAgent, m_Scene);
            if (ImGui::CollapsingHeader("Agent Information")) {
            ImGui::Text("Agent: %s", agent.GetComponent<TagComponent>().Tag.c_str());
            }
            if (ImGui::Button("Select in Hierarchy")) SetSelectedEntity(agent);
            if (ImGui::CollapsingHeader("Behavior Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (m_IsPlaying) {
                if (const auto bt=m_Scene->GetBehaviorTreeDebug(m_AIDebugAgent)) {
                    ImGui::TextDisabled("Per-agent runtime snapshot (read-only)");
                    const char* result=bt->LastResult==0?"Success":bt->LastResult==1?"Failure":"Running";
                    ImGui::Text("Root: %s",result);
                    // The graph and timeline observe the SAME per-agent runtime snapshot.
                    // Never tick the BT from the editor; never mistake historical events
                    // for complete historical snapshots of every node.
                    static std::uint32_t timelineAgent = 0;
                    static std::uint64_t timelineNode = 0;
                    static std::uint64_t timelineTick = 0;
                    static std::string timelineKind;
                    static float graphZoom = 1.0f;
                    if (timelineAgent != m_AIDebugAgent) {
                        timelineAgent = m_AIDebugAgent;
                        timelineNode = 0;
                        timelineTick = 0;
                        timelineKind.clear();
                    }
                    ImGui::SetNextItemWidth(145.0f);
                    ImGui::SliderFloat("Graph zoom", &graphZoom, 0.65f, 1.65f, "%.2fx");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear event selection")) {
                        timelineNode = 0;
                        timelineTick = 0;
                        timelineKind.clear();
                    }
                    if (bt->Nodes.empty()) {
                        ImGui::TextDisabled("No compiled nodes to display.");
                    } else {
                        // Preorder + Depth are captured from the compiled BT runtime.
                        // Compute subtree widths bottom-up so sibling branches do not overlap.
                        const int count = static_cast<int>(bt->Nodes.size());
                        std::vector<int> parents(count, -1);
                        std::vector<std::vector<int>> children(count);
                        std::vector<int> stack;
                        for (int i = 0; i < count; ++i) {
                            const int depth = std::max(0, bt->Nodes[i].Depth);
                            while (static_cast<int>(stack.size()) > depth) stack.pop_back();
                            if (!stack.empty()) {
                                parents[i] = stack.back();
                                children[stack.back()].push_back(i);
                            }
                            stack.push_back(i);
                        }
                        std::vector<float> widths(count, 1.0f), centers(count, 0.0f);
                        for (int i = count - 1; i >= 0; --i) {
                            if (!children[i].empty()) {
                                float total = 0.0f;
                                for (int child : children[i]) total += widths[child];
                                widths[i] = std::max(1.0f, total);
                            }
                        }
                        float cursor = 0.0f;
                        std::function<void(int, float)> layout = [&](int index, float left) {
                            centers[index] = left + widths[index] * 0.5f;
                            float childLeft = left;
                            for (int child : children[index]) {
                                layout(child, childLeft);
                                childLeft += widths[child];
                            }
                            if (!children[index].empty())
                                centers[index] = (centers[children[index].front()] + centers[children[index].back()]) * 0.5f;
                        };
                        int maxDepth = 0;
                        for (int i = 0; i < count; ++i) {
                            maxDepth = std::max(maxDepth, std::max(0, bt->Nodes[i].Depth));
                            if (parents[i] < 0) { layout(i, cursor); cursor += widths[i]; }
                        }
                        const float nodeW = 156.0f * graphZoom;
                        const float nodeH = 49.0f * graphZoom;
                        const float stepX = 180.0f * graphZoom;
                        const float stepY = 90.0f * graphZoom;
                        const float canvasW = std::max(330.0f, (cursor + 0.4f) * stepX);
                        const float canvasH = (maxDepth + 1) * stepY + 36.0f * graphZoom;
                        ImGui::BeginChild("##BTLiveGraph", ImVec2(0, 320), true,
                            ImGuiWindowFlags_HorizontalScrollbar);
                        const ImVec2 origin = ImGui::GetCursorScreenPos();
                        ImGui::InvisibleButton("##BTGraphCanvas", ImVec2(canvasW, canvasH));
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        auto pos = [&](int i) {
                            return ImVec2(origin.x + centers[i] * stepX,
                                origin.y + 12.0f * graphZoom + std::max(0, bt->Nodes[i].Depth) * stepY);
                        };
                        for (int i = 0; i < count; ++i) {
                            if (parents[i] < 0) continue;
                            const ImVec2 a = pos(parents[i]), b = pos(i);
                            const ImVec2 start(a.x, a.y + nodeH);
                            const ImVec2 end(b.x, b.y);
                            const float middle = (start.y + end.y) * 0.5f;
                            const ImU32 lineColor = IM_COL32(105, 116, 137, 210);
                            draw->AddLine(start, ImVec2(start.x, middle), lineColor, 1.5f);
                            draw->AddLine(ImVec2(start.x, middle), ImVec2(end.x, middle), lineColor, 1.5f);
                            draw->AddLine(ImVec2(end.x, middle), end, lineColor, 1.5f);
                        }
                        for (int i = 0; i < count; ++i) {
                            const auto& node = bt->Nodes[i];
                            const ImVec2 center = pos(i);
                            const ImVec2 lo(center.x - nodeW * 0.5f, center.y);
                            const ImVec2 hi(center.x + nodeW * 0.5f, center.y + nodeH);
                            const bool selected = timelineNode == node.ID && timelineTick != 0;
                            const ImU32 tint = node.Status < 0 ? IM_COL32(120, 130, 144, 255)
                                : node.Status == 0 ? IM_COL32(60, 195, 110, 255)
                                : node.Status == 1 ? IM_COL32(225, 85, 78, 255)
                                : IM_COL32(92, 162, 245, 255);
                            draw->AddRectFilled(lo, hi, IM_COL32(38, 44, 57, 255), 7.0f);
                            draw->AddRect(lo, hi, selected ? IM_COL32(255, 215, 100, 255) : tint,
                                7.0f, 0, selected ? 3.0f : (node.Active ? 2.5f : 1.5f));
                            const float fontScale = graphZoom < 0.85f ? 0.85f : 1.0f;
                            (void)fontScale;
                            std::string label = node.Label;
                            if (label.size() > 24) label = label.substr(0, 21) + "...";
                            const char* state = node.Status < 0 ? "IDLE" : node.Status == 0 ? "SUCCESS"
                                : node.Status == 1 ? "FAILURE" : "RUNNING";
                            draw->AddText(ImVec2(lo.x + 8.0f, lo.y + 7.0f), IM_COL32(240, 242, 247, 255), label.c_str());
                            draw->AddText(ImVec2(lo.x + 8.0f, lo.y + 26.0f * graphZoom), tint, state);
                            if (node.Active)
                                draw->AddCircleFilled(ImVec2(hi.x - 11.0f, lo.y + 11.0f), 4.0f, tint);
                            if (ImGui::IsMouseHoveringRect(lo, hi, true)) {
                                ImGui::SetTooltip("%s\nNode ID: %llu\nTicks: %zu\nExecuted this tick: %s",
                                    node.Label.c_str(), static_cast<unsigned long long>(node.ID),
                                    node.Ticks, node.Active ? "Yes" : "No");
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                                    timelineNode = node.ID;
                                    timelineTick = 0;
                                    timelineKind.clear();
                                }
                            }
                        }
                        ImGui::EndChild();
                        ImGui::TextDisabled("Blue Running  |  Green Success  |  Red Failure  |  Gray Idle");
                        ImGui::TextDisabled("Scroll to pan; Shift+wheel for horizontal scroll. Click a node for details.");
                        if (timelineNode != 0) {
                            for (const auto& node : bt->Nodes) if (node.ID == timelineNode) {
                                ImGui::SeparatorText("Selected node");
                                ImGui::Text("%s  |  ID %llu  |  Ticks %zu", node.Label.c_str(),
                                    static_cast<unsigned long long>(node.ID), node.Ticks);
                                if (timelineTick != 0)
                                    ImGui::Text("Selected event: tick %llu (%s)",
                                        static_cast<unsigned long long>(timelineTick), timelineKind.c_str());
                                ImGui::TextDisabled("Node color represents the LIVE snapshot, not the historical event.");
                                break;
                            }
                        }
                    }
                    if (ImGui::CollapsingHeader("Execution Timeline", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::TextDisabled("Per-agent bounded event history (latest first; ticks, not wall-clock time)");
                        // Presentation-only filters: the recorded event history remains unchanged.
                        static bool showActions = true;
                        static bool showConditions = true;
                        static bool showPriority = true;
                        static bool showBlackboard = true;
                        static char blackboardKeyFilter[128] = {};
                        ImGui::Checkbox("Actions", &showActions);
                        ImGui::SameLine(); ImGui::Checkbox("Conditions", &showConditions);
                        ImGui::SameLine(); ImGui::Checkbox("Priority / Other", &showPriority);
                        ImGui::SameLine(); ImGui::Checkbox("Blackboard", &showBlackboard);
                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::InputTextWithHint("##BlackboardKeyFilter", "Blackboard key filter (e.g. TargetPosition)",
                            blackboardKeyFilter, sizeof(blackboardKeyFilter));
                        ImGui::BeginChild("##BTEventTimeline", ImVec2(0, 190), true);
                        if (bt->Events.empty()) ImGui::TextDisabled("No events recorded yet.");
                        int eventIndex = 0;
                        for (auto it = bt->Events.rbegin(); it != bt->Events.rend(); ++it, ++eventIndex) {
                            const auto& e = *it;
                            const bool isBlackboard = e.Kind == "Blackboard";
                            const bool isAction = !isBlackboard && e.Detail.find("Action:") != std::string::npos;
                            const bool isCondition = !isBlackboard && e.Detail.find("Condition:") != std::string::npos;
                            if (isBlackboard && !showBlackboard) continue;
                            if (isAction && !showActions) continue;
                            if (isCondition && !showConditions) continue;
                            if (!isBlackboard && !isAction && !isCondition && !showPriority) continue;
                            if (isBlackboard && blackboardKeyFilter[0] != '\0') {
                                const std::string key = e.Detail.substr(0, e.Detail.find(':'));
                                if (key.find(blackboardKeyFilter) == std::string::npos) continue;
                            }
                            const ImVec4 tint = e.Kind == "Blackboard" ? ImVec4(.55f,.82f,.98f,1.f)
                                : e.Kind == "Aborted" || e.Kind == "Failed"
                                ? ImVec4(.95f,.42f,.32f,1.f)
                                : e.Kind == "Succeeded" ? ImVec4(.3f,.85f,.5f,1.f)
                                : e.Kind == "Running" ? ImVec4(.4f,.68f,1.f,1.f)
                                : ImVec4(.95f,.8f,.35f,1.f);
                            std::string nodeLabel = "Blackboard / Tree";
                            for (const auto& node : bt->Nodes)
                                if (node.ID == e.Node) { nodeLabel = node.Label; break; }
                            ImGui::PushID(eventIndex);
                            const std::string caption = "Tick " + std::to_string(e.Tick) + " | " + e.Kind + " | " + nodeLabel;
                            const bool chosen = timelineTick == e.Tick && timelineNode == e.Node && timelineKind == e.Kind;
                            if (ImGui::Selectable(caption.c_str(), chosen)) {
                                timelineNode = e.Node;
                                timelineTick = e.Tick;
                                timelineKind = e.Kind;
                            }
                            if (ImGui::IsItemHovered() && !e.Detail.empty()) ImGui::SetTooltip("%s", e.Detail.c_str());
                            if (!e.Detail.empty()) {
                                ImGui::Indent();
                                ImGui::TextColored(tint, "%s", e.Detail.c_str());
                                ImGui::Unindent();
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndChild();
                    }
                    if (ImGui::CollapsingHeader("Blackboard")) {
                        ImGui::TextDisabled("Agent memory | %zu keys | revision %llu", bt->BlackboardEntries.size(),
                            static_cast<unsigned long long>(bt->BlackboardRevision));
                        if (bt->BlackboardEntries.empty()) ImGui::TextDisabled("No Blackboard keys set");
                        else if (ImGui::BeginTable("##BTBlackboard", 3,
                            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                            ImVec2(0, 180))) {
                            ImGui::TableSetupColumn("Key");
                            ImGui::TableSetupColumn("Type");
                            ImGui::TableSetupColumn("Value");
                            ImGui::TableHeadersRow();
                            for (const auto& entry : bt->BlackboardEntries) {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(entry.Key.c_str());
                                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("%s", entry.Type.c_str());
                                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(entry.Value.c_str());
                            }
                            ImGui::EndTable();
                        }
                    }
                } else ImGui::TextDisabled("No Behavior Tree debug data for this agent.");
            } else ImGui::TextDisabled("Available during Play.");
            }
            if (ImGui::CollapsingHeader("Behavior State")) {
            if (!m_IsPlaying)
                ImGui::TextDisabled("Available during Play.");
            else if (const auto debug = m_Scene->GetAIDebugState(m_AIDebugAgent))
            {
                const ImVec4 stateColor =
                    debug->Current == "FLEE" ? ImVec4(1.0f, 0.4f, 0.35f, 1.0f) :
                    debug->Current == "CHASE" ? ImVec4(1.0f, 0.78f, 0.25f, 1.0f) :
                    ImVec4(0.4f, 0.85f, 0.55f, 1.0f);
                ImGui::TextColored(stateColor, "CURRENT: %s", debug->Current.c_str());
                ImGui::Text("Previous: %s", debug->Previous.empty() ? "-" : debug->Previous.c_str());
                ImGui::Text("Time in state: %.2f s", debug->TimeInState);
                if (ImGui::TreeNode("State Transitions"))
                {
                    if (debug->History.empty()) ImGui::TextDisabled("No transitions yet.");
                    for (auto it = debug->History.rbegin(); it != debug->History.rend(); ++it)
                        ImGui::Text("%.1f s  %s -> %s", it->AtSeconds, it->From.c_str(), it->To.c_str());
                    ImGui::TreePop();
                }
            }
            else
                ImGui::TextDisabled("No runtime AI state (script missing or inactive).");
            }
            if (agent.HasComponent<PerceptionComponent>())
            {
                const auto& perception = agent.GetComponent<PerceptionComponent>();
                if (ImGui::CollapsingHeader("Perception")) {
                ImGui::Text("Enabled: %s | Debug: %s", perception.Enabled ? "Yes" : "No", perception.DebugDraw ? "On" : "Off");
                ImGui::Text("Radius: %.2f m | FOV: %.1f deg", perception.DetectionRadius, perception.FieldOfView);
                ImGui::Text("Eye: %.2f m | Interval: %.2f s", perception.EyeHeight, perception.UpdateInterval);
                ImGui::Text("Memory: %.2f s | Layer mask: 0x%08X", perception.MemoryDuration, (unsigned)perception.DetectionMask);
                ImGui::SeparatorText("Last Seen Target");
                ImGui::Checkbox("Show Last Seen Position in Scene", &m_AIDebugShowLastSeenMarker);
                if (!m_IsPlaying)
                    ImGui::TextDisabled("Available during Play.");
                else if (const auto lastSeen = m_Scene->GetLastSeenTarget(m_AIDebugAgent))
                {
                    const std::string name = m_Scene->IsValid(lastSeen->EntityHandle)
                        ? Entity(lastSeen->EntityHandle, m_Scene).GetComponent<TagComponent>().Tag
                        : "Unknown";
                    ImGui::Text("Last target: %s (%u)", name.c_str(), lastSeen->EntityHandle);
                    ImGui::TextColored(lastSeen->IsVisible ? ImVec4(0.3f, 0.9f, 0.4f, 1) : ImVec4(1, 0.8f, 0.3f, 1),
                        "%s", lastSeen->IsVisible ? "VISIBLE" : "LOST SIGHT");
                    ImGui::Text("Last detected: %.2f s ago", lastSeen->TimeSinceLastSeen);
                    ImGui::Text("Last known position: X %.2f  Y %.2f  Z %.2f",
                        lastSeen->Position.x, lastSeen->Position.y, lastSeen->Position.z);
                }
                else
                    ImGui::TextDisabled("No target seen yet.");
                ImGui::SeparatorText("Tracked targets");
                const auto targets = m_IsPlaying ? m_Scene->GetPerceivedTargets(m_AIDebugAgent) : std::vector<Scene::PerceptionTargetInfo>{};
                if (targets.empty()) ImGui::TextDisabled("No tracked targets.");
                for (const auto& target : targets)
                {
                    const std::string name = m_Scene->IsValid(target.EntityHandle)
                        ? Entity(target.EntityHandle, m_Scene).GetComponent<TagComponent>().Tag : "Unknown";
                    ImGui::PushID((int)target.EntityHandle);
                    if (ImGui::TreeNodeEx("Target", ImGuiTreeNodeFlags_None, "%s (%u)", name.c_str(), target.EntityHandle))
                    {
                        ImGui::TextColored(target.IsVisible ? ImVec4(0.3f, 0.9f, 0.4f, 1) : ImVec4(1, 0.8f, 0.3f, 1),
                            "%s", target.IsVisible ? "VISIBLE" : "REMEMBERED");
                        ImGui::Text("Visible for: %.2f s", target.VisibleDuration);
                        ImGui::Text("Tracked for: %.2f s", target.TrackedDuration);
                        ImGui::Text("Last seen: %.2f s ago", target.TimeSinceLastSeen);
                        ImGui::Text("Last position: %.2f, %.2f, %.2f", target.LastKnownPosition.x, target.LastKnownPosition.y, target.LastKnownPosition.z);
                        const float remaining = target.IsVisible ? 1.0f :
                            (perception.MemoryDuration > 0 ? std::clamp(1.0f - target.TimeSinceLastSeen / perception.MemoryDuration, 0.0f, 1.0f) : 0.0f);
                        ImGui::ProgressBar(remaining, ImVec2(-1, 0), "Memory remaining");
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                }
            }
            if (agent.HasComponent<NativeScriptComponent>() &&
                (agent.GetComponent<NativeScriptComponent>().ScriptName == "NeuralTrainingAgent" ||
                 agent.GetComponent<NativeScriptComponent>().ScriptName == "BTAssetAgent")) {
                NeuralAgent::Snapshot snap;
                if (m_IsPlaying && NeuralAgent::Read(m_AIDebugAgent,snap) &&
                    ImGui::CollapsingHeader("Neural Inspector - Live",ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Text("Model: %s",snap.model.c_str());
                        ImGui::Text("Status: %s | Target distance: %.2f",snap.status.c_str(),snap.distance);
                        auto drawValues=[](const char* title,const std::vector<float>& values) {
                            ImGui::TextUnformatted(title);
                            for(size_t i=0;i<values.size();++i) {
                                ImGui::PushID(static_cast<int>(i));
                                ImGui::ProgressBar(std::clamp((values[i]+1.f)*0.5f,0.f,1.f),ImVec2(120,0));
                                ImGui::SameLine(); ImGui::Text("[%d] %.4f",static_cast<int>(i),values[i]);
                                ImGui::PopID();
                            }
                        };
                        drawValues("Inputs",snap.inputs);
                        drawValues("Predictions",snap.outputs);
                        // Animated feed-forward graph. Read-only: no editor-side
                        // mutation of the model or live agent state.
                        static bool showNeuralGraph = true;
                        ImGui::Checkbox("Animated neural graph", &showNeuralGraph);
                        if (showNeuralGraph && snap.activations.size() >= 2) {
                            const float graphWidth = std::max(260.f, ImGui::GetContentRegionAvail().x);
                            const float graphHeight = 230.f;
                            const ImVec2 origin = ImGui::GetCursorScreenPos();
                            ImGui::InvisibleButton("##NeuralFeedForwardGraph", ImVec2(graphWidth, graphHeight));
                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            dl->AddRectFilled(origin, ImVec2(origin.x+graphWidth,origin.y+graphHeight),
                                              IM_COL32(17,23,34,255), 5.f);
                            const size_t count = snap.activations.size();
                            const float time = static_cast<float>(ImGui::GetTime());
                            const float phase = std::fmod(time * 0.65f, 1.f);
                            auto nodePos = [&](size_t layer, size_t neuron, size_t n) {
                                return ImVec2(origin.x + 22.f + (graphWidth-44.f)*float(layer)/float(count-1),
                                    origin.y + 24.f + (graphHeight-48.f)*(float(neuron)+0.5f)/float(n));
                            };
                            for (size_t l=1; l<count; ++l) {
                                const auto& previous = snap.activations[l-1];
                                const auto& current = snap.activations[l];
                                const size_t a = std::min(previous.size(),size_t(24));
                                const size_t b = std::min(current.size(),size_t(24));
                                for(size_t i=0;i<a;++i) for(size_t j=0;j<b;++j) {
                                    const ImVec2 from=nodePos(l-1,i,a), to=nodePos(l,j,b);
                                    const float strength=std::clamp(std::abs(previous[i]*current[j]),0.f,1.f);
                                    dl->AddLine(from,to,IM_COL32(75,128,172,int(28+90*strength)),1.f);
                                    // Moving particle indicates propagation direction,
                                    // not a measured propagation delay.
                                    const float t=std::fmod(phase*float(count-1)+float(count-1)-(float(l)-1.f),1.f);
                                    const float visiblePhase=phase*float(count-1)-(float(l)-1.f);
                                    if(visiblePhase>=0.f && visiblePhase<=1.f && strength>0.08f) {
                                        const ImVec2 dot(from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t);
                                        dl->AddCircleFilled(dot,1.3f+1.5f*strength,IM_COL32(255,205,90,205));
                                    }
                                }
                            }
                            for(size_t l=0;l<count;++l) {
                                const auto& layer=snap.activations[l];
                                const size_t n=std::min(layer.size(),size_t(24));
                                for(size_t j=0;j<n;++j) {
                                    const ImVec2 center=nodePos(l,j,n);
                                    const float value=std::clamp(layer[j],-1.f,1.f);
                                    const ImU32 color=value>=0.f
                                        ? IM_COL32(70, int(115+125*value), 225, 255)
                                        : IM_COL32(int(115+120*(-value)), 95, 165, 255);
                                    dl->AddCircleFilled(center,5.f,color);
                                    dl->AddCircle(center,5.f,IM_COL32(215,225,240,190),0,1.f);
                                }
                            }
                            ImGui::TextDisabled("Live activations | animated signal flow (illustrative)");
                            if (std::any_of(snap.activations.begin(),snap.activations.end(),
                                [](const auto& layer){return layer.size()>24;}))
                                ImGui::TextDisabled("Graph displays first 24 neurons per layer");
                        }
                        if(ImGui::TreeNode("Layer activations")) {
                            for(size_t i=0;i<snap.activations.size();++i) {
                                ImGui::PushID(static_cast<int>(i));
                                if(ImGui::TreeNodeEx("##neural_layer", ImGuiTreeNodeFlags_None, "Layer %d (%d neurons)", static_cast<int>(i), static_cast<int>(snap.activations[i].size()))) {
                                    drawValues("Activations",snap.activations[i]);
                                    ImGui::TreePop();
                                }
                                ImGui::PopID();
                            }
                            ImGui::TreePop();
                        }
                        if(ImGui::TreeNode("Weights and biases (saved model)")) {
                            try {
                                // Disk snapshot. To inspect changes made by training, reopen this section after saving.
                                const auto model=Neural::NeuralNetwork::Load(NeuralAssetPath(snap.model).string());
                                for(size_t layer=0;layer<model.Layers().size();++layer) {
                                    ImGui::PushID(static_cast<int>(layer));
                                    const auto& dense=model.Layers()[layer];
                                    if(ImGui::TreeNodeEx("##neural_dense", ImGuiTreeNodeFlags_None, "Dense %d (%d x %d)", static_cast<int>(layer), static_cast<int>(dense.Outputs()), static_cast<int>(dense.Inputs()))) {
                                        for(size_t neuron=0;neuron<dense.Outputs();++neuron) {
                                            ImGui::Text("Neuron %d | bias %.5f",static_cast<int>(neuron),dense.Biases()[neuron]);
                                            for(size_t input=0;input<dense.Inputs();++input) {
                                                ImGui::SameLine();
                                                ImGui::TextDisabled("w%d:%.3f",static_cast<int>(input),dense.Weights()(neuron,input));
                                            }
                                        }
                                        ImGui::TreePop();
                                    }
                                    ImGui::PopID();
                                }
                            } catch(const std::exception& e) { ImGui::TextDisabled("Cannot load model: %s",e.what()); }
                            ImGui::TreePop();
                        }
                }
            }
            if (agent.HasComponent<NavAgentComponent>())
            {
                const auto& nav = agent.GetComponent<NavAgentComponent>();
                if (ImGui::CollapsingHeader("Navigation")) {
                ImGui::Text("Enabled: %s | Speed: %.2f", nav.Enabled ? "Yes" : "No", nav.Speed);
                ImGui::Text("Destination: %s", nav.HasDestination ? "Set" : "None");
                if (nav.HasDestination) ImGui::Text("(%.2f, %.2f, %.2f)", nav.Destination.x, nav.Destination.y, nav.Destination.z);
                ImGui::Text("Stopping distance: %.2f", nav.StoppingDistance);
                }
            }
        }
        else ImGui::TextDisabled("No AI agents in this scene.");
        ImGui::EndGroup();
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

    bool EditorLayer::ConsumeSaveSceneAsRequest()
    {
        const bool requested = m_SaveSceneAsRequested;
        m_SaveSceneAsRequested = false;
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
