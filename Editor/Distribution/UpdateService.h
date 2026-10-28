/** @file UpdateService.h @brief Installed Alpha update checks with an explicit save-before-install gate. */
#pragma once
#include <functional>
namespace NoJob::Updates {
/** @brief Initialize the signed Alpha feed after the main window exists. */
void Initialize(std::function<bool()> saveScene);
/** @brief Stop update workers before editor objects and callbacks are destroyed. */
void Shutdown();
/** @brief Update the thread-safe in-editor job/Play guard. */
void SetBusy(bool busy);
/** @brief Freeze editing while a saved scene waits for the installer handoff. */
bool IsInstalling();
/** @brief Read a worker's graceful shutdown request on the main thread. */
bool QuitRequested();
/** @brief Draw update preferences and manual commands in the Help menu. */
void DrawMenu();
/** @brief Draw main-thread save approval and progress messages. */
void Draw();
}
