/**
 * @file SceneFileDialog.h
 * @brief SceneFileDialog declarations for the Scenes and Native Scripting subsystem.
 * @ingroup Scene
 */
#pragma once
#include <filesystem>

namespace NoJob
{
    class SceneFileDialog
    {
    public:
        static std::filesystem::path Open(const std::filesystem::path& initialDirectory);
        /** @brief Select an existing neural policy without requiring an asset name. */
        static std::filesystem::path OpenNeuralPolicy(const std::filesystem::path& initialDirectory);
        static std::filesystem::path SaveAs(const std::filesystem::path& initialDirectory);
    };
}
