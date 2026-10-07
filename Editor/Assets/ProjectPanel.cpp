#include "Editor/Assets/ProjectPanel.h"
#include "Editor/Assets/ProjectAssetOperations.h"
#include "Editor/Scripting/VisualStudioIntegration.h"

#include "Engine/Asset/AssetManager.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Assets/MaterialSerializer.h"
#include "Engine/Assets/PrefabSerializer.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace NoJob
{
    void ProjectPanel::Initialize()
    {
        m_CurrentDirectory = AssetManager::GetProjectRoot();
    }

    void ProjectPanel::Draw(Scene* scene,
                            Entity& selectedEntity,
                            const std::shared_ptr<Mesh>& defaultCubeMesh,
                            const std::shared_ptr<Material>& defaultCubeMaterial,
                            const CreateModelFunction& createModel,
                            const LogFunction& log)
    {
        ImGui::Begin("Project");

        if (m_CurrentDirectory.empty())
            m_CurrentDirectory = AssetManager::GetProjectRoot();

        const auto projectRoot = AssetManager::GetProjectRoot();
        const auto assetsRoot = AssetManager::GetAssetsDirectory();

        if (m_CurrentDirectory != projectRoot)
        {
            if (ImGui::Button("< Back"))
            {
                const auto parent = m_CurrentDirectory.parent_path();
                m_CurrentDirectory = parent.string().size() < projectRoot.string().size()
                    ? projectRoot : parent;
            }
            ImGui::SameLine();
        }

        ImGui::TextDisabled("%s",
            m_CurrentDirectory.lexically_relative(projectRoot).generic_string().c_str());
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##ProjectSearch", "Search current folder...",
            m_Search, sizeof(m_Search));
        ImGui::Separator();

        const bool folderInsideAssets =
            m_CurrentDirectory == assetsRoot ||
            m_CurrentDirectory.string().rfind(assetsRoot.string(), 0) == 0;

        if (folderInsideAssets)
        {
            ImGui::InvisibleButton("##ProjectPrefabDropTarget",
                ImVec2(ImGui::GetContentRegionAvail().x, 28.0f));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NOJOB_ENTITY"))
                {
                    const auto handle = *static_cast<const std::uint32_t*>(payload->Data);
                    if (scene && scene->IsValid(handle))
                        ProjectAssetOperations::CreatePrefab(
                            Entity(handle, scene), m_CurrentDirectory, log);
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 28.0f);
            ImGui::TextDisabled("Drop a Hierarchy object here to create a Prefab");
            ImGui::Separator();
        }

        if (ImGui::BeginPopupContextWindow("ProjectCreateContext",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("Create"))
            {
                const bool canCreatePrefab = folderInsideAssets && static_cast<bool>(selectedEntity);
                if (ImGui::MenuItem("C++ Script", nullptr, false, folderInsideAssets))
                    m_RequestCreateCppScript = true;
                if (ImGui::MenuItem("Prefab From Selected", nullptr, false, canCreatePrefab))
                    ProjectAssetOperations::CreatePrefab(selectedEntity, m_CurrentDirectory, log);
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        if (m_RequestCreateCppScript)
        {
            ImGui::OpenPopup("Create C++ Script");
            m_RequestCreateCppScript = false;
        }

        static char newScriptName[128] = "NewScript";
        if (ImGui::BeginPopupModal("Create C++ Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Script name");
            ImGui::SetNextItemWidth(280.0f);
            const bool enter = ImGui::InputText("##CppScriptName", newScriptName,
                sizeof(newScriptName), ImGuiInputTextFlags_EnterReturnsTrue);

            const auto scriptsRoot = assetsRoot / "Scripts";
            const std::string sanitized = ProjectAssetOperations::SanitizeCppIdentifier(newScriptName);
            const bool exists = std::filesystem::exists(scriptsRoot / (sanitized + ".h")) ||
                                std::filesystem::exists(scriptsRoot / (sanitized + ".cpp"));
            if (exists) ImGui::TextDisabled("A script with this name already exists.");
            else ImGui::TextDisabled("Creates Assets/Scripts/%s.h and %s.cpp",
                sanitized.c_str(), sanitized.c_str());

            const bool create = (ImGui::Button("Create") || enter) && !exists;
            ImGui::SameLine();
            const bool cancel = ImGui::Button("Cancel");
            if (create && ProjectAssetOperations::CreateCppScript(scriptsRoot, sanitized, log))
            {
                m_CurrentDirectory = scriptsRoot;
                newScriptName[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            if (cancel) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        std::error_code error;
        if (std::filesystem::exists(m_CurrentDirectory, error))
        {
            for (const auto& entry : std::filesystem::directory_iterator(
                m_CurrentDirectory, std::filesystem::directory_options::skip_permission_denied, error))
            {
                const auto path = entry.path();
                const std::string name = path.filename().string();

                if (m_Search[0] != '\0')
                {
                    std::string filter = m_Search;
                    std::string lowerName = name;
                    std::transform(filter.begin(), filter.end(), filter.begin(),
                        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                    if (lowerName.find(filter) == std::string::npos) continue;
                }

                if (m_CurrentDirectory == projectRoot &&
                    path.lexically_normal() != assetsRoot.lexically_normal())
                    continue;

                ImGui::PushID(path.string().c_str());
                if (entry.is_directory())
                {
                    if (ImGui::Selectable(("[Folder] " + name).c_str(), false,
                        ImGuiSelectableFlags_AllowDoubleClick) &&
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        m_CurrentDirectory = path;

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NOJOB_ENTITY"))
                        {
                            const auto handle = *static_cast<const std::uint32_t*>(payload->Data);
                            if (scene && scene->IsValid(handle))
                                ProjectAssetOperations::CreatePrefab(Entity(handle, scene), path, log);
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                else
                {
                    const std::string extension = path.extension().string();
                    const bool isTexture = extension == ".png" || extension == ".jpg" ||
                        extension == ".jpeg" || extension == ".bmp" || extension == ".tga";
                    const bool isModel = extension == ".obj" || extension == ".fbx" ||
                        extension == ".gltf" || extension == ".glb" || extension == ".dae" ||
                        extension == ".stl" || extension == ".ply" || extension == ".3ds" ||
                        extension == ".blend";
                    const bool isMaterial = extension == ".nojobmat";
                    const bool isPrefab = extension == ".nojobprefab";
                    const bool isScript = extension == ".cpp" || extension == ".h" || extension == ".hpp";
                    const bool isAudio = extension == ".wav" || extension == ".mp3" || extension == ".flac";

                    if (isAudio)
                    {
                        ImGui::Selectable(("[Audio] " + name).c_str());
                        const std::string relative = AssetManager::ToProjectRelative(path).generic_string();
                        if (ImGui::BeginDragDropSource())
                        {
                            ImGui::SetDragDropPayload("NOJOB_AUDIO_ASSET", relative.c_str(), relative.size() + 1);
                            ImGui::Text("Audio: %s", name.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                    else if (isScript)
                    {
                        const bool clicked = ImGui::Selectable(("[C++] " + name).c_str(), false,
                            ImGuiSelectableFlags_AllowDoubleClick);
#ifdef _WIN32
                        if (clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            VisualStudioIntegration::OpenScript(path, projectRoot, log);
                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Open in Visual Studio"))
                                VisualStudioIntegration::OpenScript(path, projectRoot, log);
                            ImGui::Separator();
                            ImGui::TextDisabled("%s", path.lexically_relative(projectRoot).generic_string().c_str());
                            ImGui::EndPopup();
                        }
#endif
                    }
                    else if (isModel)
                    {
                        if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && createModel)
                            createModel(AssetManager::ToProjectRelative(path));
                    }
                    else if (isPrefab)
                    {
                        auto instantiate = [&]()
                        {
                            if (!scene) return;
                            auto entity = PrefabSerializer::Instantiate(*scene, path, defaultCubeMesh, defaultCubeMaterial);
                            if (entity) selectedEntity = entity;
                        };
                        if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) instantiate();
                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Instantiate Prefab")) instantiate();
                            ImGui::EndPopup();
                        }
                    }
                    else if (isMaterial)
                    {
                        const bool clicked = ImGui::Selectable(name.c_str(), false,
                            ImGuiSelectableFlags_AllowDoubleClick);
                        const std::string relative = AssetManager::ToProjectRelative(path).generic_string();
                        if (clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && selectedEntity &&
                            selectedEntity.HasComponent<MeshRendererComponent>())
                        {
                            auto& renderer = selectedEntity.GetComponent<MeshRendererComponent>();
                            if (renderer.MaterialAsset)
                            {
                                AssetRegistry registry(assetsRoot);
                                registry.Load();
                                auto loaded = MaterialSerializer::Load(path, renderer.MaterialAsset->GetShader(), registry);
                                if (loaded) renderer.MaterialAsset = loaded;
                            }
                        }
                        if (ImGui::BeginDragDropSource())
                        {
                            ImGui::SetDragDropPayload("NOJOB_MATERIAL_ASSET", relative.c_str(), relative.size() + 1);
                            ImGui::Text("Material: %s", name.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                    else if (isTexture)
                    {
                        ImGui::Selectable(name.c_str());
                        if (ImGui::BeginDragDropSource())
                        {
                            const std::string relative = AssetManager::ToProjectRelative(path).generic_string();
                            ImGui::SetDragDropPayload("NOJOB_TEXTURE_ASSET", relative.c_str(), relative.size() + 1);
                            ImGui::Text("Texture: %s", name.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                    else ImGui::TextDisabled("%s", name.c_str());
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
    }
}
