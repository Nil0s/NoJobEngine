/**
 * @file OpenGLContext.h
 * @brief OpenGLContext declarations for the OpenGL Backend subsystem.
 * @ingroup OpenGL
 */
#pragma once
struct GLFWwindow;
namespace NoJob {
class OpenGLContext {
public:
    static void Init(GLFWwindow* window);
};
}
