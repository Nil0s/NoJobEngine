#include "Engine/Core/Window.h"
#include "Engine/Renderer/Renderer.h"
#include "Editor/EditorLayer.h"
#include <exception>
#include <iostream>

int main() {
    try {
        NoJob::Window window({"NoJobEngine", 1600, 900});
        NoJob::Renderer::Init();

        NoJob::EditorLayer editor;
        editor.Init(window.GetNativeWindow());

        while (!window.ShouldClose()) {
            window.PollEvents();
            NoJob::Renderer::BeginFrame();

            editor.BeginFrame();
            editor.Draw();
            editor.EndFrame();

            NoJob::Renderer::EndFrame();
            window.SwapBuffers();
        }

        editor.Shutdown();
        NoJob::Renderer::Shutdown();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
