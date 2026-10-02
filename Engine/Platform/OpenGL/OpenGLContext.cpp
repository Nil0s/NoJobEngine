#include "Engine/Platform/OpenGL/OpenGLContext.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

namespace NoJob {
void OpenGLContext::Init(GLFWwindow* window) {
    glfwMakeContextCurrent(window);

    const int version = gladLoadGL(
        reinterpret_cast<GLADloadfunc>(glfwGetProcAddress));

    if (version == 0)
        throw std::runtime_error("NoJobEngine: GLAD failed to load OpenGL.");

    std::cout << "NoJobEngine OpenGL context\n"
              << "Vendor: " << glGetString(GL_VENDOR) << '\n'
              << "Renderer: " << glGetString(GL_RENDERER) << '\n'
              << "Version: " << glGetString(GL_VERSION) << '\n';
}
}
