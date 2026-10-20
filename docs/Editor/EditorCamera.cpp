#include "Editor/EditorCamera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace NoJob
{
    EditorCamera::EditorCamera(
        float fovDegrees,
        float aspectRatio,
        float nearClip,
        float farClip)
        : m_FOV(fovDegrees),
          m_AspectRatio(aspectRatio),
          m_NearClip(nearClip),
          m_FarClip(farClip)
    {
        RecalculateProjection();
        RecalculateView();
    }

    void EditorCamera::OnUpdate(
        GLFWwindow* window,
        float deltaTime,
        bool viewportHovered)
    {
        const bool rightMouseDown =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)
            == GLFW_PRESS;

        const bool looking = viewportHovered && rightMouseDown;

        if (!looking)
        {
            m_WasLooking = false;
            return;
        }

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (!m_WasLooking)
        {
            m_LastMouseX = mouseX;
            m_LastMouseY = mouseY;
            m_WasLooking = true;
        }

        const float deltaX =
            static_cast<float>(mouseX - m_LastMouseX);

        const float deltaY =
            static_cast<float>(m_LastMouseY - mouseY);

        m_LastMouseX = mouseX;
        m_LastMouseY = mouseY;

        m_Yaw += deltaX * m_MouseSensitivity;
        m_Pitch += deltaY * m_MouseSensitivity;
        m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);

        const glm::vec3 forward = GetForwardDirection();
        const glm::vec3 right = GetRightDirection();
        const glm::vec3 worldUp{ 0.0f, 1.0f, 0.0f };

        float speed = m_MoveSpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            speed *= 2.5f;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            m_Position += forward * speed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            m_Position -= forward * speed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            m_Position += right * speed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            m_Position -= right * speed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            m_Position += worldUp * speed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            m_Position -= worldUp * speed * deltaTime;

        RecalculateView();
    }

    void EditorCamera::SetViewportSize(float width, float height)
    {
        if (width <= 0.0f || height <= 0.0f)
            return;

        const float aspect = width / height;
        if (std::abs(aspect - m_AspectRatio) < 0.0001f)
            return;

        m_AspectRatio = aspect;
        RecalculateProjection();
    }

    glm::vec3 EditorCamera::GetForwardDirection() const
    {
        const float yaw = glm::radians(m_Yaw);
        const float pitch = glm::radians(m_Pitch);

        glm::vec3 direction;
        direction.x = std::cos(yaw) * std::cos(pitch);
        direction.y = std::sin(pitch);
        direction.z = std::sin(yaw) * std::cos(pitch);

        return glm::normalize(direction);
    }

    glm::vec3 EditorCamera::GetRightDirection() const
    {
        return glm::normalize(
            glm::cross(
                GetForwardDirection(),
                glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    void EditorCamera::RecalculateView()
    {
        const glm::vec3 forward = GetForwardDirection();

        m_ViewMatrix = glm::lookAt(
            m_Position,
            m_Position + forward,
            glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void EditorCamera::RecalculateProjection()
    {
        m_ProjectionMatrix = glm::perspective(
            glm::radians(m_FOV),
            m_AspectRatio,
            m_NearClip,
            m_FarClip);
    }
}
