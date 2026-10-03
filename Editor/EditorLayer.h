#pragma once
#include "Engine/Scene/Entity.h"

#include <filesystem>
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace NoJob
{
    class Scene;
    class Mesh;
    class Material;

    class EditorLayer
    {
    public:
        void Init(GLFWwindow* window, Scene* scene);
        void Shutdown();

        void BeginFrame();
        void Draw();
        void EndFrame();

        void SetSelectedEntity(Entity entity);
        void SetViewportTexture(std::uint32_t textureID);

        void SetEditorCameraMatrices(
            const glm::mat4& view,
            const glm::mat4& projection);

        void SetDefaultCubeAssets(
            std::shared_ptr<Mesh> mesh,
            std::shared_ptr<Material> material);

        std::uint32_t GetViewportWidth() const;
        std::uint32_t GetViewportHeight() const;

        bool IsViewportHovered() const { return m_ViewportHovered; }
        bool IsViewportFocused() const { return m_ViewportFocused; }

    private:
        void DrawMainMenu();
        void DrawHierarchy();
        void DrawInspector();
        void DrawViewport();
        void DrawConsole();
        void DrawProjectPanel();

        Entity CreateEmptyEntity();
        Entity CreateCubeEntity();
        void DeleteSelectedEntity();
        void DuplicateSelectedEntity();

        Scene* m_Scene = nullptr;
        Entity m_SelectedEntity;

        std::shared_ptr<Mesh> m_DefaultCubeMesh;
        std::shared_ptr<Material> m_DefaultCubeMaterial;

        std::uint32_t m_ViewportTextureID = 0;
        float m_ViewportWidth = 1280.0f;
        float m_ViewportHeight = 720.0f;
        bool m_ViewportHovered = false;
        bool m_ViewportFocused = false;

        glm::mat4 m_EditorView{ 1.0f };
        glm::mat4 m_EditorProjection{ 1.0f };
        int m_GizmoOperation = 0;

        std::filesystem::path m_ProjectDirectory;
    };
}
