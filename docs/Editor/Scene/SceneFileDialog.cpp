#include "Editor/Scene/SceneFileDialog.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <commdlg.h>
#endif

namespace NoJob
{
    namespace
    {
#ifdef _WIN32
        std::filesystem::path ShowSceneDialog(
            bool save,
            const std::filesystem::path& initialDirectory)
        {
            wchar_t fileName[32768]{};
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.lpstrFile = fileName;
            dialog.nMaxFile = static_cast<DWORD>(std::size(fileName));
            dialog.lpstrFilter =
                L"NoJob Scene (*.nojobscene)\0*.nojobscene\0All Files (*.*)\0*.*\0";
            const std::wstring initial = initialDirectory.wstring();
            dialog.lpstrInitialDir = initial.empty() ? nullptr : initial.c_str();
            dialog.lpstrDefExt = L"nojobscene";
            dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

            const BOOL accepted =
                save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
            if (!accepted) return {};
            return std::filesystem::path(fileName);
        }
#endif
    }

    std::filesystem::path SceneFileDialog::Open(
        const std::filesystem::path& initialDirectory)
    {
#ifdef _WIN32
        return ShowSceneDialog(false, initialDirectory);
#else
        (void)initialDirectory;
        return {};
#endif
    }

    std::filesystem::path SceneFileDialog::SaveAs(
        const std::filesystem::path& initialDirectory)
    {
#ifdef _WIN32
        return ShowSceneDialog(true, initialDirectory);
#else
        (void)initialDirectory;
        return {};
#endif
    }
}
