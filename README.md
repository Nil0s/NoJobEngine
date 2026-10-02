# NoJobEngine

A C++20 game engine project built incrementally with OpenGL, Vulkan and Dear ImGui.

## Current milestone: v0.1
- C++20 / CMake
- GLFW window
- OpenGL 4.6 context through GLAD
- Basic renderer layer
- Dear ImGui editor window
- Vulkan directory reserved for the future backend

## Visual Studio
1. Install Visual Studio 2022 with **Desktop development with C++**.
2. Ensure **CMake tools for Windows** and Git are installed.
3. Open Visual Studio -> **Open a local folder** -> select `NoJobEngine`.
4. Let CMake configure and FetchContent download the dependencies.
5. Select `NoJobEditor.exe` as the startup target and run it.

The first launch requires internet access so CMake can fetch the dependencies.
