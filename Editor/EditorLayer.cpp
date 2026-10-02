#include "Editor/EditorLayer.h"

#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

namespace NoJob
{
    void EditorLayer::Init(GLFWwindow* window, Scene* scene)
    {
        m_Scene = scene;

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
        DrawMainMenu();
        DrawHierarchy();
        DrawViewport();
        DrawInspector();
        DrawConsole();

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

                copy.AddComponent<MeshRendererComponent>(material);
            }
        }

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

        if (m_Scene)
        {
            for (Entity entity : m_Scene->GetEntities())
            {
                auto& tag = entity.GetComponent<TagComponent>().Tag;

                ImGui::PushID(
                    static_cast<int>(entity.GetHandle()));

                const bool selected = entity == m_SelectedEntity;

                if (ImGui::Selectable(tag.c_str(), selected))
                    m_SelectedEntity = entity;

                if (ImGui::BeginPopupContextItem("EntityContext"))
                {
                    m_SelectedEntity = entity;

                    if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                        DuplicateSelectedEntity();

                    if (ImGui::MenuItem("Delete", "Delete"))
                        DeleteSelectedEntity();

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
        }

        ImGui::End();
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
            auto& transform =
                m_SelectedEntity.GetComponent<TransformComponent>();

            glm::mat4 transformMatrix =
                transform.GetTransform();

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

                transform.Position =
                {
                    translation[0],
                    translation[1],
                    translation[2]
                };

                transform.Rotation =
                    glm::radians(glm::vec3(
                        rotationDegrees[0],
                        rotationDegrees[1],
                        rotationDegrees[2]));

                transform.Scale =
                {
                    scale[0],
                    scale[1],
                    scale[2]
                };
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
}
