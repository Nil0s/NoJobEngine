#include "Engine/Renderer/Renderer.h"
#include <glad/gl.h>

namespace NoJob {
RendererAPI Renderer::s_API = RendererAPI::OpenGL;

void Renderer::Init() { glEnable(GL_DEPTH_TEST); }
void Renderer::Shutdown() {}

void Renderer::BeginFrame() {
    glViewport(0, 0, 1600, 900);
    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void Renderer::EndFrame() {}
}
