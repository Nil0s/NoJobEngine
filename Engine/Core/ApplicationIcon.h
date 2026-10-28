/** @file ApplicationIcon.h @brief Apply the embedded NoJobEngine logo to native app windows. */
#pragma once
#include <GLFW/glfw3.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#endif
namespace NoJob {
/** @brief Set taskbar/title-bar icons from executable resources without external image paths. */
inline void ApplyApplicationIcon(GLFWwindow* window) {
#ifdef _WIN32
    if(!window)return;
    const auto module=GetModuleHandleW(nullptr);
    const auto hwnd=glfwGetWin32Window(window);
    const auto smallIcon=LoadImageW(module,MAKEINTRESOURCEW(101),IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED);
    const auto largeIcon=LoadImageW(module,MAKEINTRESOURCEW(101),IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED);
    if(smallIcon)SendMessageW(hwnd,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(smallIcon));
    if(largeIcon)SendMessageW(hwnd,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(largeIcon));
#else
    (void)window;
#endif
}
}
