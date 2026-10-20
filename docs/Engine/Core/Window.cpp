#include "Engine/Core/Window.h"
#include "Engine/Platform/OpenGL/OpenGLContext.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace NoJob {
namespace { std::uint32_t s_WindowCount = 0; }

Window::Window(const WindowProps& props) {
    if (s_WindowCount == 0 && glfwInit() != GLFW_TRUE)
        throw std::runtime_error("NoJobEngine: GLFW initialization failed.");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(
        static_cast<int>(props.Width),
        static_cast<int>(props.Height),
        props.Title.c_str(), nullptr, nullptr);

    if (!m_Window) {
        if (s_WindowCount == 0) glfwTerminate();
        throw std::runtime_error("NoJobEngine: window creation failed.");
    }

    ++s_WindowCount;
    OpenGLContext::Init(m_Window);
    glfwSwapInterval(1);
}

Window::~Window() {
    if (!m_Window) return;
    glfwDestroyWindow(m_Window);
    m_Window = nullptr;
    if (--s_WindowCount == 0) glfwTerminate();
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_Window) == GLFW_TRUE;
}
void Window::PollEvents() const { glfwPollEvents(); }
void Window::SwapBuffers() const { glfwSwapBuffers(m_Window); }
}
