#pragma once
struct GLFWwindow;
namespace NoJob {
class EditorLayer {
public:
    void Init(GLFWwindow* window);
    void Shutdown();
    void BeginFrame();
    void Draw();
    void EndFrame();
};
}
