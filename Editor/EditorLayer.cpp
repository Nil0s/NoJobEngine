#include "Editor/EditorLayer.h"

#include "Engine/Asset/AssetManager.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Texture.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

#include <imgui.h>
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

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <commdlg.h>
#endif

namespace NoJob
{
    namespace
    {
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
            }

            drawList->PopClipRect();
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

        AssetManager::Init(std::filesystem::current_path());
        m_ProjectDirectory = AssetManager::GetAssetsDirectory();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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

        ImGui::DockSpaceOverViewport(
            0,
            ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_PassthruCentralNode);
    }

    void EditorLayer::Draw()
    {
        DrawPlayToolbar();
        DrawMainMenu();
        DrawHierarchy();
        DrawViewport();
        DrawInspector();
        DrawConsole();
        DrawProjectPanel();

        if (m_SelectedEntity && ImGui::IsKeyPressed(ImGuiKey_Delete))
            DeleteSelectedEntity();

        const ImGuiIO& io = ImGui::GetIO();
        if (m_SelectedEntity
            && io.KeyCtrl
            && ImGui::IsKeyPressed(ImGuiKey_D))
        {
            DuplicateSelectedEntity();
        }
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

        Entity entity = m_Scene->CreateEntity("Empty Entity");
        m_SelectedEntity = entity;
        return entity;
    }

    Entity EditorLayer::CreateCubeEntity()
    {
        if (!m_Scene)
            return {};

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

            entity.AddComponent<MeshRendererComponent>(material);
        }

        m_SelectedEntity = entity;
        return entity;
    }

    void EditorLayer::DeleteSelectedEntity()
    {
        if (!m_Scene || !m_SelectedEntity)
            return;

        m_Scene->DestroyEntity(m_SelectedEntity);
        m_SelectedEntity = {};
    }

    void EditorLayer::DuplicateSelectedEntity()
    {
        if (!m_Scene || !m_SelectedEntity)
            return;

        const Entity source = m_SelectedEntity;
        const auto& sourceTag = source.GetComponent<TagComponent>().Tag;

        Entity copy =
            m_Scene->CreateEntity(sourceTag + " Copy");

        copy.GetComponent<TransformComponent>() =
            source.GetComponent<TransformComponent>();

        if (source.HasComponent<MeshComponent>())
        {
            copy.AddComponent<MeshComponent>(
                source.GetComponent<MeshComponent>().MeshAsset);
        }

        if (source.HasComponent<MeshRendererComponent>())
        {
            const auto& sourceRenderer =
                source.GetComponent<MeshRendererComponent>();

            if (sourceRenderer.MaterialAsset)
            {
                auto material = std::make_shared<Material>(
                    sourceRenderer.MaterialAsset->GetShader(),
                    sourceRenderer.MaterialAsset->GetColor());

                material->SetTexture(
                    sourceRenderer.MaterialAsset->GetTexture());
                material->UseTexture() =
                    sourceRenderer.MaterialAsset->UseTexture();

                copy.AddComponent<MeshRendererComponent>(material);
            }
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

        m_SelectedEntity = copy;
    }

    void EditorLayer::DrawMainMenu()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::MenuItem("New Scene");
                ImGui::MenuItem("Open Scene");
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

            if (ImGui::BeginMenu("GameObject"))
            {
                if (ImGui::MenuItem("Create Empty"))
                    CreateEmptyEntity();

                if (ImGui::MenuItem("3D Object/Cube"))
                    CreateCubeEntity();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Hierarchy");
                ImGui::MenuItem("Inspector");
                ImGui::MenuItem("Console");
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

    void EditorLayer::DrawHierarchy()
    {
        ImGui::Begin("Hierarchy");

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
                    m_Scene->Unparent(Entity(handle, m_Scene), true);
            }
            ImGui::EndDragDropTarget();
        }

        if (m_Scene)
        {
            for (Entity entity : m_Scene->GetEntities())
            {
                if (entity.GetComponent<RelationshipComponent>().Parent == 0)
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

        if (entity == m_SelectedEntity)
            flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID(static_cast<int>(entity.GetHandle()));

        const bool open =
            ImGui::TreeNodeEx("EntityNode", flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked())
            m_SelectedEntity = entity;

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
                    Entity child(childHandle, m_Scene);
                    m_Scene->SetParent(child, entity, true);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("EntityContext"))
        {
            m_SelectedEntity = entity;

            if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                DuplicateSelectedEntity();

            if (relationship.Parent != 0
                && ImGui::MenuItem("Unparent"))
            {
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

            if (ImGui::InputText(
                    "##EntityName",
                    tagBuffer,
                    sizeof(tagBuffer)))
            {
                tag = tagBuffer;
            }

            ImGui::Separator();

            if (ImGui::CollapsingHeader(
                    "Transform",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& transform =
                    m_SelectedEntity.GetComponent<TransformComponent>();

                ImGui::DragFloat3(
                    "Position",
                    &transform.Position.x,
                    0.01f);

                ImGui::DragFloat3(
                    "Rotation",
                    &transform.Rotation.x,
                    0.01f);

                ImGui::DragFloat3(
                    "Scale",
                    &transform.Scale.x,
                    0.01f);
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
                        m_Scene->Unparent(m_SelectedEntity, true);
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
                if (ImGui::CollapsingHeader("Native Script: Rotator",
                    ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& script = m_SelectedEntity.GetComponent<NativeScriptComponent>();
                    ImGui::Checkbox("Enabled##Rotator", &script.Enabled);
                    ImGui::DragFloat("Speed (rad/s)", &script.RotationSpeed, 0.05f);
                    if (ImGui::Button("Remove Rotator"))
                        m_SelectedEntity.RemoveComponent<NativeScriptComponent>();
                }
            }
            else if (ImGui::Button("Add Rotator Script"))
                m_SelectedEntity.AddComponent<NativeScriptComponent>();

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
            else if (ImGui::Button("Add Rigidbody"))
            {
                m_SelectedEntity.AddComponent<RigidbodyComponent>();
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
            else if (!m_SelectedEntity.HasComponent<SphereColliderComponent>() &&
                     !m_SelectedEntity.HasComponent<CapsuleColliderComponent>() &&
                     ImGui::Button("Add Box Collider"))
            {
                m_SelectedEntity.AddComponent<BoxColliderComponent>();
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
            else if (!m_SelectedEntity.HasComponent<BoxColliderComponent>() &&
                     !m_SelectedEntity.HasComponent<CapsuleColliderComponent>() &&
                     ImGui::Button("Add Sphere Collider"))
            {
                m_SelectedEntity.AddComponent<SphereColliderComponent>();
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
            else if (!m_SelectedEntity.HasComponent<BoxColliderComponent>() &&
                     !m_SelectedEntity.HasComponent<SphereColliderComponent>() &&
                     ImGui::Button("Add Capsule Collider"))
            {
                m_SelectedEntity.AddComponent<CapsuleColliderComponent>();
            }

            if (m_SelectedEntity.HasComponent<MeshRendererComponent>())
            {
                ImGui::Separator();

                if (ImGui::CollapsingHeader(
                        "Mesh Renderer",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& renderer =
                        m_SelectedEntity
                            .GetComponent<MeshRendererComponent>();

                    if (renderer.MaterialAsset)
                    {
                        auto& color =
                            renderer.MaterialAsset->GetColor();

                        ImGui::ColorEdit4(
                            "Material Color",
                            &color.x);

                        ImGui::Checkbox(
                            "Use Texture",
                            &renderer.MaterialAsset->UseTexture());

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

                                    renderer.MaterialAsset->SetTexture(
                                        AssetManager::LoadTexture(importedPath));

                                    renderer.MaterialAsset->UseTexture() = true;
                                }
                                catch (const std::exception& exception)
                                {
                                    // Keep the previous texture if loading fails.
                                    // A proper editor notification system comes later.
                                    (void)exception;
                                }
                            }
                        }

                        ImGui::SameLine();

                        if (ImGui::Button("Checkerboard"))
                        {
                            renderer.MaterialAsset->SetTexture(
                                Texture2D::CreateCheckerboard());
                            renderer.MaterialAsset->UseTexture() = true;
                        }

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload =
                                    ImGui::AcceptDragDropPayload(
                                        "NOJOB_TEXTURE_ASSET"))
                            {
                                const char* relativePath =
                                    static_cast<const char*>(payload->Data);

                                try
                                {
                                    renderer.MaterialAsset->SetTexture(
                                        AssetManager::LoadTexture(
                                            relativePath));

                                    renderer.MaterialAsset->UseTexture() = true;
                                }
                                catch (const std::exception&)
                                {
                                }
                            }

                            ImGui::EndDragDropTarget();
                        }

                        if (renderer.MaterialAsset->GetTexture())
                        {
                            const auto& texture =
                                renderer.MaterialAsset->GetTexture();

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
        if (m_Scene)
        {
            DrawSceneColliderGizmos(
                *m_Scene,
                m_SelectedEntity,
                m_EditorView,
                m_EditorProjection,
                viewportMin,
                ImVec2(m_ViewportWidth, m_ViewportHeight));
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

            if (ImGuizmo::IsUsing())
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
        ImGui::End();
    }
    void EditorLayer::DrawProjectPanel()
    {
        ImGui::Begin("Project");

        if (m_ProjectDirectory.empty())
            m_ProjectDirectory = AssetManager::GetAssetsDirectory();

        const auto assetsRoot =
            AssetManager::GetAssetsDirectory();

        if (m_ProjectDirectory != assetsRoot)
        {
            if (ImGui::Button("< Back"))
                m_ProjectDirectory =
                    m_ProjectDirectory.parent_path();

            ImGui::SameLine();
        }

        ImGui::TextDisabled(
            "%s",
            m_ProjectDirectory.lexically_relative(
                AssetManager::GetProjectRoot()).generic_string().c_str());

        ImGui::Separator();

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

                    if (isTexture)
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

}
