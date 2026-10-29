/** @file UpdateService.cpp @brief WinSparkle lifecycle and thread-safe installer handoff. */
#include "UpdateService.h"
#include "Engine/Core/ProjectPaths.h"
#include "Resources/Version.h"
#include "Resources/UpdateConfig.h"
#include <imgui.h>
#include <atomic>
#include <fstream>
#include <chrono>
#include <cstdlib>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif
#ifdef NJE_WITH_WINSPARKLE
#include <winsparkle.h>
#endif
namespace NoJob::Updates {
namespace {
std::atomic<bool> busy{false},approved{false},locked{false},quit{false},requestApproval{false};
std::atomic<int> state{0}; // 0 disabled, 1 ready, 2 checking, 3 available, 4 current, 5 error.
bool initialized=false,automatic=true;
long long lastCheck=0;
std::function<bool()> save;
std::filesystem::path preferences;
std::string feedback;
void Persist() {
    std::ofstream out(preferences);out<<"automatic "<<automatic<<"\nlast_check "<<lastCheck<<'\n';
}
void Unlock(){approved=false;locked=false;}
#ifdef NJE_WITH_WINSPARKLE
void __cdecl OnError(){state=5;Unlock();}
void __cdecl OnCurrent(){state=4;Unlock();}
void __cdecl OnFound(){state=3;}
void __cdecl OnCancelled(){state=1;Unlock();}
int __cdecl CanShutdown() {
    if(!approved.load()||busy.load()||Paths::OtherInstalledAppsRunning()) {
        Unlock();requestApproval=true;return 0;
    }
    return 1;
}
void __cdecl RequestShutdown(){quit=true;}
void Check(bool visible) {
    if(!initialized)return;
    lastCheck=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    Persist();state=2;
    if(visible)win_sparkle_check_update_with_ui();else win_sparkle_check_update_without_ui();
}
#endif
}
void Initialize(std::function<bool()> saveScene) {
    save=std::move(saveScene);
#ifdef NJE_WITH_WINSPARKLE
    if(!Paths::IsInstalledApplication()||std::getenv("NJE_DISABLE_UPDATES"))return;
    preferences=Paths::SettingsDirectory()/"Updates.ini";
    std::ifstream in(preferences);std::string key;long long value;
    while(in>>key>>value){if(key=="automatic")automatic=value!=0;else if(key=="last_check")lastCheck=value;}
    win_sparkle_set_app_details(L"NJE",L"NJE",NJE_WIDEN(NJE_ALPHA_VERSION));
    win_sparkle_set_app_build_version(NJE_WIDEN(NJE_VERSION_STRING));
    win_sparkle_set_appcast_url(FeedURL);win_sparkle_set_eddsa_public_key(PublicKey);
    win_sparkle_set_lang("en");
    // The editor owns its per-user preference and 24h schedule; no first-run registry prompt.
    win_sparkle_set_automatic_check_for_updates(0);
    win_sparkle_set_error_callback(OnError);win_sparkle_set_did_not_find_update_callback(OnCurrent);
    win_sparkle_set_did_find_update_callback(OnFound);win_sparkle_set_update_cancelled_callback(OnCancelled);
    win_sparkle_set_update_postponed_callback(OnCancelled);win_sparkle_set_update_dismissed_callback(OnCancelled);
    win_sparkle_set_can_shutdown_callback(CanShutdown);win_sparkle_set_shutdown_request_callback(RequestShutdown);
    win_sparkle_init();initialized=true;state=1;
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(automatic&&(lastCheck==0||now-lastCheck>=24*60*60))Check(false);
#endif
}
void Shutdown() {
#ifdef NJE_WITH_WINSPARKLE
    if(initialized)win_sparkle_cleanup();
#endif
    initialized=false;save={};Unlock();
}
void SetBusy(bool value){busy=value;}
bool IsInstalling(){return locked.load();}
bool QuitRequested(){return quit.load();}
void DrawMenu() {
    ImGui::TextDisabled("NJE " NJE_PUBLIC_VERSION);
    ImGui::BeginDisabled(!initialized||locked.load());
    if(ImGui::MenuItem("Check for updates")) {
#ifdef NJE_WITH_WINSPARKLE
        Check(true);
#endif
    }
    if(ImGui::MenuItem("Automatically check for updates",nullptr,&automatic))Persist();
    ImGui::EndDisabled();
    if(!initialized)ImGui::TextDisabled(Paths::IsInstalledApplication()?"Updates disabled for this diagnostic session.":"Install NJE to enable automatic updates.");
    const auto status=state.load();
    if(status==2)ImGui::TextDisabled("Checking Alpha updates...");
    else if(status==4)ImGui::TextDisabled("NJE is up to date.");
    else if(status==5)ImGui::TextDisabled("Update check unavailable. Try again later.");
#ifdef _WIN32
    if(Paths::IsInstalledApplication()&&ImGui::MenuItem("Open user data folder"))ShellExecuteW(nullptr,L"open",Paths::UserDataDirectory().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#endif
}
void Draw() {
    if(requestApproval.exchange(false))ImGui::OpenPopup("Save before updating NJE");
    if(ImGui::BeginPopupModal("Save before updating NJE",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("An update is ready. Save the current scene before closing NJE.");
        const bool blocked=busy.load()||Paths::OtherInstalledAppsRunning();
        if(blocked)ImGui::TextUnformatted("Stop Play, finish in-editor jobs and close other NJE windows first.");
        ImGui::BeginDisabled(blocked);
        if(ImGui::Button("Save scene and install update")) {
            if(save&&save()) {
                approved=true;locked=true;feedback.clear();ImGui::CloseCurrentPopup();
#ifdef NJE_WITH_WINSPARKLE
                win_sparkle_check_update_with_ui_and_install();
#endif
            }else feedback="The scene could not be saved. The update was not started.";
        }
        ImGui::EndDisabled();ImGui::SameLine();
        if(ImGui::Button("Cancel")){Unlock();ImGui::CloseCurrentPopup();}
        if(!feedback.empty())ImGui::TextUnformatted(feedback.c_str());
        ImGui::EndPopup();
    }
    if(locked.load()) {
        ImGui::Begin("Preparing NJE update",nullptr,ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("Scene saved. Editing is paused while the updater prepares the installer.");
        ImGui::TextUnformatted("Continue or cancel in the updater window. NJE will close normally to install.");
        ImGui::End();
    }
}
}
