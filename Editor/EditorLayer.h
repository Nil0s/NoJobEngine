#pragma once
#include "Engine/Scene/Entity.h"

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

    private:
        void DrawMainMenu();
        void DrawHierarchy();
        void DrawInspector();
        void DrawViewport();
        void DrawConsole();

        Scene* m_Scene = nullptr;
        Entity m_SelectedEntity;
    };
}
