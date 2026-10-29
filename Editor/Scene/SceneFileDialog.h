/**
 * @file SceneFileDialog.h
 * @brief SceneFileDialog declarations for the Scenes and Native Scripting subsystem.
 * @ingroup Scene
 */
#pragma once
#include <filesystem>

namespace NoJob
{
    /** @brief Result of protecting the active scene before a document replacement. */
    enum class SceneSaveChoice { Save, Discard, Cancel };
    class SceneFileDialog
    {
    public:
        /** @brief Offer Save, Discard or Cancel before replacing an edited scene. */
        static SceneSaveChoice ConfirmSceneReplacement();
        /** @brief Explain a failed scene write while retaining the active document. */
        static void ShowSaveError();
        static std::filesystem::path Open(const std::filesystem::path& initialDirectory);
        /** @brief Select an existing neural policy without requiring an asset name. */
        static std::filesystem::path OpenNeuralPolicy(const std::filesystem::path& initialDirectory);
        static std::filesystem::path SaveAs(const std::filesystem::path& initialDirectory);
    };
}
