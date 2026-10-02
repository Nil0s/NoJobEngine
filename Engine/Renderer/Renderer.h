#pragma once
namespace NoJob {
enum class RendererAPI { OpenGL, Vulkan };

class Renderer {
public:
    static void Init();
    static void Shutdown();
    static void BeginFrame();
    static void EndFrame();
    static RendererAPI GetAPI() { return s_API; }

private:
    static RendererAPI s_API;
};
}
