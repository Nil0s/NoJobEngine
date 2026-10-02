#include "Engine/Platform/OpenGL/OpenGLRenderer.h"
#include <glad/glad.h>
namespace NoJob {
void OpenGLRenderer::Initialize() { glEnable(GL_DEPTH_TEST); }
void OpenGLRenderer::BeginFrame() {
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
}
