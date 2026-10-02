#include "Engine/Core/Application.h"
#include "Engine/Renderer/Renderer.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace NoJob {
Application::Application() : m_Window(1600, 900, "NoJobEngine") {}
bool Application::Initialize() {
    if (!m_Window.Initialize()) return false;
    Renderer::Initialize();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_Window.GetNativeWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 460");
    return true;
}
void Application::Run() {
    while (!m_Window.ShouldClose()) {
        m_Window.PollEvents();
        Renderer::BeginFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("NoJobEngine");
        ImGui::Text("NoJobEngine v0.1");
        ImGui::Separator();
        ImGui::Text("Renderer: OpenGL");
        ImGui::Text("Vulkan backend: planned");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        Renderer::EndFrame();
        m_Window.SwapBuffers();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
}
