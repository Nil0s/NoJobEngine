#pragma once
#include "Engine/Scene/Entity.h"
#include "Engine/Renderer/Framebuffer.h"

#include <filesystem>
#include <cstdint>
#include <memory>
#include <vector>
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
        Entity GetSelectedEntity() const { return m_SelectedEntity; }
        void SetScene(Scene* scene);
        void SetViewportTexture(std::uint32_t textureID);
        void SetCameraPreviewTexture(std::uint32_t textureID)
        {
            m_CameraPreviewTextureID = textureID;
        }

        bool ConsumePlayRequest();
        bool ConsumePauseRequest();
        bool ConsumeStopRequest();
        bool ConsumeSaveSceneRequest();
        bool ConsumeLoadSceneRequest();
        bool ConsumeGraphicsTestSceneRequest();
        void SetRuntimeState(bool playing, bool paused);

        void SetGraphicsSettings(const FramebufferSpecification& settings)
        {
            m_GraphicsSettings = settings;
        }
        const FramebufferSpecification& GetGraphicsSettings() const
        {
            return m_GraphicsSettings;
        }

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
        void DrawPlayToolbar();
        void DrawHierarchy();
        void DrawEntityNode(Entity entity);
        void DrawInspector();
        void DrawViewport();
        void DrawConsole();
        void DrawProjectPanel();
        void DrawGraphicsSettings();

        Entity CreateEmptyEntity();
        Entity CreateCubeEntity();
        Entity CreateModelEntity(const std::filesystem::path& modelPath);
        void DeleteSelectedEntity();
        void DuplicateSelectedEntity();

        void PushUndoSnapshot(std::unique_ptr<Scene> snapshot);
        void CaptureUndoSnapshot();
        void Undo();
        void Redo();
        void ClearRedoHistory();

        Scene* m_Scene = nullptr;
        Entity m_SelectedEntity;
        std::size_t m_SelectedMaterialSlot = 0;

        std::shared_ptr<Mesh> m_DefaultCubeMesh;
        std::shared_ptr<Material> m_DefaultCubeMaterial;

        std::uint32_t m_ViewportTextureID = 0;
        std::uint32_t m_CameraPreviewTextureID = 0;
        float m_ViewportWidth = 1280.0f;
        float m_ViewportHeight = 720.0f;
        bool m_ViewportHovered = false;
        bool m_ViewportFocused = false;

        glm::mat4 m_EditorView{ 1.0f };
        glm::mat4 m_EditorProjection{ 1.0f };
        int m_GizmoOperation = 0;

        std::filesystem::path m_ProjectDirectory;
        bool m_IsPlaying = false;
        bool m_IsPaused = false;
        bool m_PlayRequested = false;
        bool m_PauseRequested = false;
        bool m_StopRequested = false;
        bool m_SaveSceneRequested = false;
        bool m_LoadSceneRequested = false;
        bool m_GraphicsTestSceneRequested = false;
        bool m_ShowGraphicsSettings = true;
        FramebufferSpecification m_GraphicsSettings{};

        static constexpr std::size_t MaxHistoryEntries = 64;
        std::vector<std::unique_ptr<Scene>> m_UndoHistory;
        std::vector<std::unique_ptr<Scene>> m_RedoHistory;
        std::unique_ptr<Scene> m_TransformEditSnapshot;
        bool m_GizmoWasUsing = false;
    };
}
