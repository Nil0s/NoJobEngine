#include "Engine/Core/Application.h"
#include <iostream>

int main() {
    NoJob::Application app;
    if (!app.Initialize()) {
        std::cerr << "Failed to initialize NoJobEngine.\n";
        return 1;
    }
    app.Run();
    return 0;
}
