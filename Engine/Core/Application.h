#pragma once
#include "Engine/Core/Window.h"
namespace NoJob {
class Application {
public:
    Application();
    bool Initialize();
    void Run();
private:
    Window m_Window;
};
}
