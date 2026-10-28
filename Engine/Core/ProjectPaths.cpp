/** @file ProjectPaths.cpp @brief Keep native executable discovery isolated from public headers. */
#include "ProjectPaths.h"
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
namespace NoJob::Paths {
std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    wchar_t filename[32768]{};
    const auto length=GetModuleFileNameW(nullptr,filename,32768);
    if(length>0&&length<32768)return std::filesystem::path(filename).parent_path();
#endif
    std::error_code ec;return std::filesystem::current_path(ec);
}
}
