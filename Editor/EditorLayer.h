#pragma once
#include "Engine/Scene/Entity.h"

#include <cstdint>

struct GLFWwindow;

namespace NoJob
{
    class Scene;

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

        Scene* m_Scene = nullptr;
        Entity m_SelectedEntity;

        std::uint32_t m_ViewportTextureID = 0;
        float m_ViewportWidth = 1280.0f;
        float m_ViewportHeight = 720.0f;
        bool m_ViewportHovered = false;
        bool m_ViewportFocused = false;
    };
}
