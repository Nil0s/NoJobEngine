#pragma once
#include "Engine/Scene/Entity.h"

#include <cstdint>
#include <memory>

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
    };
}
