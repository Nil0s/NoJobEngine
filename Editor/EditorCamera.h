#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

namespace NoJob
{
    class EditorCamera
    {
    public:
        EditorCamera(
            float fovDegrees = 45.0f,
            float aspectRatio = 16.0f / 9.0f,
            float nearClip = 0.1f,
            float farClip = 1000.0f);

        void OnUpdate(
            GLFWwindow* window,
            float deltaTime,
            bool viewportHovered);

        void SetViewportSize(float width, float height);

        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
        glm::mat4 GetViewProjection() const
        {
            return m_ProjectionMatrix * m_ViewMatrix;
        }

        const glm::vec3& GetPosition() const { return m_Position; }

    private:
        void RecalculateView();
        void RecalculateProjection();

        glm::vec3 GetForwardDirection() const;
        glm::vec3 GetRightDirection() const;

        float m_FOV = 45.0f;
        float m_AspectRatio = 16.0f / 9.0f;
        float m_NearClip = 0.1f;
        float m_FarClip = 1000.0f;

        glm::vec3 m_Position{ 0.0f, 1.5f, 5.0f };
        float m_Yaw = -90.0f;
        float m_Pitch = -12.0f;

        float m_MoveSpeed = 4.0f;
        float m_MouseSensitivity = 0.12f;

        bool m_WasLooking = false;
        double m_LastMouseX = 0.0;
        double m_LastMouseY = 0.0;

        glm::mat4 m_ViewMatrix{ 1.0f };
        glm::mat4 m_ProjectionMatrix{ 1.0f };
    };
}
