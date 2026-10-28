/** @file ProjectPaths.cpp @brief Keep native executable discovery isolated from public headers. */
#include "ProjectPaths.h"
#include <fstream>
#include <stdexcept>
#include <cstdlib>
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
namespace {
#ifdef _WIN32
struct ProcessMutex {
    HANDLE handle=nullptr;
    ~ProcessMutex(){if(handle)CloseHandle(handle);}
} applicationMutex;
struct SeedLock {
    HANDLE handle=CreateMutexW(nullptr,FALSE,L"NJE.WorkspaceSeed");
    bool owned=false;
    SeedLock(){
        if(!handle)throw std::runtime_error("Cannot lock the NJE sample workspace.");
        const auto result=WaitForSingleObject(handle,30000);
        owned=result==WAIT_OBJECT_0||result==WAIT_ABANDONED;
        if(!owned){CloseHandle(handle);handle=nullptr;throw std::runtime_error("NJE workspace is busy. Try opening the app again.");}
    }
    ~SeedLock(){if(owned)ReleaseMutex(handle);if(handle)CloseHandle(handle);}
};
std::wstring EnvironmentPath(const wchar_t* name) {
    wchar_t value[32768]{};const auto n=GetEnvironmentVariableW(name,value,32768);
    return n>0&&n<32768?std::wstring(value,n):std::wstring{};
}
#endif
void CopyMissing(const std::filesystem::path& source,const std::filesystem::path& destination) {
    if(std::filesystem::exists(destination))return;
    std::filesystem::create_directories(destination.parent_path());
#ifdef _WIN32
    const auto temporary=destination.wstring()+L".nje-seed-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
#else
    const auto temporary=destination.string()+".nje-seed.tmp";
#endif
    std::filesystem::copy_file(source,temporary,std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(temporary,destination);
}
}
bool IsInstalledApplication() {
    std::error_code ec;return std::filesystem::is_regular_file(ExecutableDirectory()/"NJE.installed",ec);
}
std::filesystem::path UserDataDirectory() {
#ifdef _WIN32
    auto value=EnvironmentPath(L"NJE_USER_HOME");
    if(!value.empty())return std::filesystem::absolute(value).lexically_normal();
    value=EnvironmentPath(L"LOCALAPPDATA");
    if(value.empty())throw std::runtime_error("Windows user data folder is unavailable.");
    return std::filesystem::path(value)/"NJE";
#else
    if(const auto* home=std::getenv("NJE_USER_HOME"))return std::filesystem::absolute(home);
    if(const auto* home=std::getenv("HOME"))return std::filesystem::path(home)/".local/share/NJE";
    throw std::runtime_error("User data folder is unavailable.");
#endif
}
std::filesystem::path EnsureWorkspaceFrom(const std::filesystem::path& application,const std::filesystem::path& userData) {
#ifdef _WIN32
    SeedLock lock;
#endif
    if(!IsProject(application))throw std::runtime_error("Installed NJE templates are missing. Reinstall the complete application.");
    const auto workspace=userData/"Projects"/"Default";
    std::filesystem::create_directories(workspace/"Assets");
    std::filesystem::create_directories(userData/"Settings");
    for(const auto& entry:std::filesystem::recursive_directory_iterator(application/"Assets")) {
        if(entry.is_regular_file()&&!entry.is_symlink())CopyMissing(entry.path(),workspace/"Assets"/entry.path().lexically_relative(application/"Assets"));
    }
    // Descriptor comes last so an interrupted first copy can be completed next launch.
    if(!IsProject(workspace)) {
        for(const auto& entry:std::filesystem::directory_iterator(application))
            if(entry.is_regular_file()&&entry.path().extension()==".nojobproject"){CopyMissing(entry.path(),workspace/entry.path().filename());break;}
    }
    return workspace;
}
std::filesystem::path InitializeProjectWorkspace() {
    if(!IsInstalledApplication())return FindProjectRoot();
    const auto root=EnsureWorkspaceFrom(ExecutableDirectory(),UserDataDirectory());
#ifdef _WIN32
    if(!applicationMutex.handle) {
        wchar_t file[32768]{};GetModuleFileNameW(nullptr,file,32768);
        const auto name=std::filesystem::path(file).stem().wstring();
        const wchar_t* mutex=name==L"NJETrainer"?L"NJE.Trainer":name==L"NJERuntime"?L"NJE.Runtime":L"NJE.Editor";
        applicationMutex.handle=CreateMutexW(nullptr,FALSE,mutex);
    }
#endif
    return root;
}
std::filesystem::path SettingsDirectory() {
    const auto directory=IsInstalledApplication()?UserDataDirectory()/"Settings":FindProjectRoot();
    if(!directory.empty())std::filesystem::create_directories(directory);
    return directory;
}
bool OtherInstalledAppsRunning() {
#ifdef _WIN32
    for(const auto* name:{L"NJE.Trainer",L"NJE.Runtime"}) {
        if(const auto handle=OpenMutexW(SYNCHRONIZE,FALSE,name)){CloseHandle(handle);return true;}
    }
#endif
    return false;
}

}
