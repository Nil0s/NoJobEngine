/** @file TrainingProcess.h @brief Launch independent native training windows. */
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace NoJob::RL {
#ifdef _WIN32
/** @brief Quote one native Windows argument without invoking a command shell. */
inline std::wstring QuoteTrainingArgument(const std::wstring& input){
    std::wstring out=L"\"";size_t slashes=0;
    for(wchar_t c:input){if(c==L'\\'){++slashes;continue;}if(c==L'\"'){out.append(slashes*2+1,L'\\');out+=c;}else{out.append(slashes,L'\\');out+=c;}slashes=0;}
    out.append(slashes*2,L'\\');out+=L'\"';return out;
}
#endif
/** @brief Start a visible independent process with its project directory captured. */
inline bool LaunchTrainingWindow(const std::filesystem::path& root,std::string& error,int task=1){
#ifdef _WIN32
    wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);
    const std::vector<std::filesystem::path> candidates={
        std::filesystem::path(module).parent_path()/"NJETrainer.exe",root/"NJETrainer.exe",std::filesystem::path(module).parent_path()/"NoJobTrainer.exe",root/"NoJobTrainer.exe",root/"out/build/trainer-release/NoJobTrainer.exe",root/"out/build/x64-Debug/NoJobTrainer.exe"};
    std::filesystem::path executable;for(const auto&p:candidates)if(std::filesystem::is_regular_file(p)){executable=p;break;}
    if(executable.empty()){error="The trainer executable is missing. Keep NJETrainer.exe beside the editor or build the NoJobTrainer target.";return false;}
    std::wstring command=QuoteTrainingArgument(executable.wstring())+L" --project "+QuoteTrainingArgument(root.wstring())+(task==0?L" --task tracking":L" --task obstacle");
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_SHOWNORMAL;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NEW_PROCESS_GROUP,nullptr,root.c_str(),&startup,&process)){
        error="Cannot open trainer (Windows error "+std::to_string(GetLastError())+").";return false;
    }
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
#else
    (void)root;(void)task;error="Native training process launching is currently available on Windows.";return false;
#endif
}
}
