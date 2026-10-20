#pragma once
#include <cstdint>
#include <string>
struct GLFWwindow;

namespace NoJob {
struct WindowProps {
    std::string Title = "NoJobEngine";
    std::uint32_t Width = 1600;
    std::uint32_t Height = 900;
};

class Window {
public:
    explicit Window(const WindowProps& props);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const;
    void PollEvents() const;
    void SwapBuffers() const;
    GLFWwindow* GetNativeWindow() const { return m_Window; }

private:
    GLFWwindow* m_Window = nullptr;
};
}
