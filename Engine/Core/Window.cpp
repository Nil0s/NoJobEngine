#include "Engine/Core/Window.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace NoJob {
Window::Window(int width, int height, std::string title)
    : m_Width(width), m_Height(height), m_Title(std::move(title)) {}
Window::~Window() {
    if (m_Window) glfwDestroyWindow(m_Window);
    glfwTerminate();
}
bool Window::Initialize() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
    if (!m_Window) return false;
    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) return false;
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
    return true;
}
void Window::PollEvents() const { glfwPollEvents(); }
void Window::SwapBuffers() const { glfwSwapBuffers(m_Window); }
bool Window::ShouldClose() const { return glfwWindowShouldClose(m_Window); }
}
