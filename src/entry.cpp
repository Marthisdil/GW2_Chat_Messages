// GW2 Chat Messages - Nexus addon entry point.
//
// Build as an x64 DLL via GW2_Chat_Messages.sln. Dependencies live in vendor/
// as git submodules (see CLAUDE.md). Design notes: docs/design.md.

#include <windows.h>

#include <atomic>
#include <cstring>

#include "imgui.h"

#include "Globals.h"
#include "Ui.h"
#include "resource.h"

AddonAPI_t* g_Api = nullptr;

namespace
{
    constexpr const char* kToggleKeybind   = "KB_CHATMSG_TOGGLE";
    constexpr const char* kShortcutId      = "SC_CHATMSG";
    constexpr const char* kIconId          = "ICON_CHATMSG";
    constexpr const char* kIconHoverId     = "ICON_CHATMSG_HOVER";
    constexpr const char* kShortcutTooltip = "Chat Messages\nLeft-click: show/hide the macro library";

    HMODULE           s_Module = nullptr;
    AddonDefinition_t s_AddonDef{};

    // Set by the keybind handler (its own thread), consumed on the render thread.
    std::atomic<bool> s_ToggleRequested{false};

    void AddonLoad(AddonAPI_t* aApi);
    void AddonUnload();
    void AddonRender();
    void AddonOptions();
    void OnToggleKeybind(const char* aIdentifier, bool aIsRelease);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) s_Module = hModule; // needed for embedded resources
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    // No side effects and no API calls allowed here.
    s_AddonDef.Signature   = 0x523CBA0E; // Fixed for the lifetime of the addon. Never change.
    s_AddonDef.APIVersion  = NEXUS_API_VERSION;
    s_AddonDef.Name        = kAddonName;
    s_AddonDef.Version     = { kVersionMajor, kVersionMinor, kVersionBuild, kVersionRevision };
    s_AddonDef.Author      = "Marthisdil";
    s_AddonDef.Description = "Chat macros organised in folders: send one or more lines to the channels you choose.";
    s_AddonDef.Load        = AddonLoad;
    s_AddonDef.Unload      = AddonUnload;
    s_AddonDef.Flags       = AF_None;
    s_AddonDef.Provider    = UP_GitHub;
    s_AddonDef.UpdateLink  = kRepositoryUrl;
    return &s_AddonDef;
}

namespace
{
    void AddonLoad(AddonAPI_t* aApi)
    {
        g_Api = aApi;

        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(g_Api->ImguiContext));
        ImGui::SetAllocatorFunctions(
            reinterpret_cast<void* (*)(size_t, void*)>(g_Api->ImguiMalloc),
            reinterpret_cast<void (*)(void*, void*)>(g_Api->ImguiFree));

        // Initialise state before registering anything that can call back into us.
        Ui::Init();

        // Quick Access textures must exist before the shortcut is added.
        g_Api->Textures_LoadFromResource(kIconId, IDB_ICON, s_Module, nullptr);
        g_Api->Textures_LoadFromResource(kIconHoverId, IDB_ICON_HOVER, s_Module, nullptr);

        // No default key: the addon never claims a key the user didn't choose (docs/design.md).
        g_Api->InputBinds_RegisterWithString(kToggleKeybind, OnToggleKeybind, "(null)");
        g_Api->Localization_Set(kToggleKeybind, "en", "Show/hide Chat Messages library");

        g_Api->QuickAccess_Add(kShortcutId, kIconId, kIconHoverId, kToggleKeybind, kShortcutTooltip);

        g_Api->GUI_Register(RT_Render, AddonRender);
        g_Api->GUI_Register(RT_OptionsRender, AddonOptions);
        g_Api->GUI_RegisterCloseOnEscape(Ui::kMainWindowName, Ui::MainWindowOpenFlag());

        LogF(LOGL_INFO, "Loaded %s.", kVersionText);
    }

    void AddonUnload()
    {
        g_Api->GUI_DeregisterCloseOnEscape(Ui::kMainWindowName);
        g_Api->GUI_Deregister(AddonOptions);
        g_Api->GUI_Deregister(AddonRender);
        g_Api->QuickAccess_Remove(kShortcutId);
        g_Api->InputBinds_Deregister(kToggleKeybind);

        Ui::Shutdown(); // saves pending changes

        LogF(LOGL_INFO, "Unloaded.");
        g_Api = nullptr;
    }

    void AddonRender()
    {
        if (s_ToggleRequested.exchange(false)) Ui::ToggleMainWindow();
        Ui::Render();
    }

    void AddonOptions()
    {
        Ui::RenderOptions();
    }

    void OnToggleKeybind(const char* aIdentifier, bool aIsRelease)
    {
        if (!aIsRelease && std::strcmp(aIdentifier, kToggleKeybind) == 0)
        {
            s_ToggleRequested = true;
        }
    }
}
