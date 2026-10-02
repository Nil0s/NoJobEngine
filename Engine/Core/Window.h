#pragma once
#include <string>

struct GLFWwindow;

namespace NoJob {
class Window {
public:
    Window(int width, int height, std::string title);
    ~Window();
    bool Initialize();
    void PollEvents() const;
    void SwapBuffers() const;
    bool ShouldClose() const;
    GLFWwindow* GetNativeWindow() const { return m_Window; }
private:
    int m_Width;
    int m_Height;
    std::string m_Title;
    GLFWwindow* m_Window = nullptr;
};
}
