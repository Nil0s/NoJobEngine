#include "Editor/EditorLayer.h"

#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>

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
                ImGui::MenuItem("Undo", "Ctrl+Z");
                ImGui::MenuItem("Redo", "Ctrl+Y");
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

        if (m_Scene)
        {
            for (Entity entity : m_Scene->GetEntities())
            {
                auto& tag = entity.GetComponent<TagComponent>().Tag;

                const bool selected = entity == m_SelectedEntity;
                if (ImGui::Selectable(tag.c_str(), selected))
                    m_SelectedEntity = entity;
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

            ImGui::Text("%s", tag.c_str());
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");

        m_ViewportHovered = ImGui::IsWindowHovered();
        m_ViewportFocused = ImGui::IsWindowFocused();

        const ImVec2 available = ImGui::GetContentRegionAvail();

        m_ViewportWidth = std::max(1.0f, available.x);
        m_ViewportHeight = std::max(1.0f, available.y);

        if (m_ViewportTextureID != 0)
        {
            ImGui::Image(
                static_cast<ImTextureID>(
                    static_cast<intptr_t>(m_ViewportTextureID)),
                ImVec2(m_ViewportWidth, m_ViewportHeight),
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));

            ImGui::SetCursorPos(ImVec2(12.0f, 32.0f));
            ImGui::TextDisabled(
                "RMB + mouse: look | WASD: move | Q/E: down/up | Shift: faster");
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::DrawConsole()
    {
        ImGui::Begin("Console");
        ImGui::Text("[Info] NoJobEngine editor started.");
        ImGui::Text("[Info] OpenGL 4.6 renderer active.");
        ImGui::Text("[Info] Scene rendered to editor framebuffer.");
        ImGui::End();
    }
}
