#include "Engine/Renderer/Renderer.h"
#include "Engine/Platform/OpenGL/OpenGLRenderer.h"
namespace NoJob {
void Renderer::Initialize() { OpenGLRenderer::Initialize(); }
void Renderer::BeginFrame() { OpenGLRenderer::BeginFrame(); }
void Renderer::EndFrame() {}
}
