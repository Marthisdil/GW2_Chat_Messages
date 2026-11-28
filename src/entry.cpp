// GW2 Chat Messages - Nexus addon entry point.
//
// Build as an x64 DLL via GW2_Chat_Messages.sln. Dependencies live in vendor/
// as git submodules (see CLAUDE.md). Design notes: docs/design.md.

#include <windows.h>

#include <atomic>
#include <cstring>

#include "imgui.h"

#include "BuildStamp.h"
#include "Globals.h"
#include "Hotkeys.h"
#include "Languages.h"
#include "Localization.h"
#include "MapNames.h"
#include "PerfRecorder.h"
#include "Sender.h"
#include "Storage.h"
#include "Ui.h"
#include "resource.h"

AddonAPI_t* g_Api = nullptr;

namespace
{
    constexpr const char* kToggleKeybind   = "KB_CHATMSG_TOGGLE";
    constexpr const char* kMenuKeybind     = "KB_CHATMSG_QUICKMENU";
    constexpr const char* kShortcutId      = "SC_CHATMSG";
    constexpr const char* kShortcutMenuId  = "QAM_CHATMSG";
    constexpr const char* kIconId          = "ICON_CHATMSG";
    constexpr const char* kIconHoverId     = "ICON_CHATMSG_HOVER";
    constexpr const char* kShortcutTooltip = TX("Chat Messages\nLeft-click: show/hide the macro library\n"
                                                "Right-click: send a macro from the menu");
    // Texts Nexus shows for us (Keybinds page, Quick Access tooltip); Languages tells Nexus their translations.
    constexpr const char* kToggleName = TX("Show/hide Chat Messages library");
    constexpr const char* kMenuName   = TX("Open Chat Messages quick menu");

    HMODULE           s_Module = nullptr;
    AddonDefinition_t s_AddonDef{};

    // Set by the keybind handler (its own thread), consumed on the render thread.
    std::atomic<bool> s_ToggleRequested{false};
    std::atomic<bool> s_MenuRequested{false};

    void AddonLoad(AddonAPI_t* aApi);
    void AddonUnload();
    void AddonRender();
    void AddonOptions();
    void AddonQuickAccessMenu();
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
        Storage::Init();  // first: the addon folder and the writer thread every save goes through
        MapNames::Init(); // before Ui and Sender, which ask it for names
        Languages::AddNexusText(kToggleKeybind, kToggleName);
        Languages::AddNexusText(kMenuKeybind, kMenuName);
        Languages::AddNexusText(kShortcutTooltip, kShortcutTooltip); // in case Nexus translates the tooltip by its text
        Languages::Init(); // before Ui, whose text it translates
        Ui::Init();
        Sender::Init();  // its WndProc hook only reacts to our own private message
        Hotkeys::Init(); // its WndProc hook only reads key presses, never blocks them
        PerfRec::Init(); // the frame recorder: only in builds made on the owner's PC (PerfRecorder.h)

        // Quick Access textures must exist before the shortcut is added.
        g_Api->Textures_LoadFromResource(kIconId, IDB_ICON, s_Module, nullptr);
        g_Api->Textures_LoadFromResource(kIconHoverId, IDB_ICON_HOVER, s_Module, nullptr);

        // No default key: the addon never claims a key the user didn't choose (docs/design.md).
        g_Api->InputBinds_RegisterWithString(kToggleKeybind, OnToggleKeybind, "(null)");
        g_Api->Localization_Set(kToggleKeybind, "en", kToggleName);
        g_Api->InputBinds_RegisterWithString(kMenuKeybind, OnToggleKeybind, "(null)");
        g_Api->Localization_Set(kMenuKeybind, "en", kMenuName);

        g_Api->QuickAccess_Add(kShortcutId, kIconId, kIconHoverId, kToggleKeybind, kShortcutTooltip);
        g_Api->QuickAccess_AddContextMenu(kShortcutMenuId, kShortcutId, AddonQuickAccessMenu);

        g_Api->GUI_Register(RT_Render, AddonRender);
        g_Api->GUI_Register(RT_OptionsRender, AddonOptions);
        g_Api->GUI_RegisterCloseOnEscape(Ui::kMainWindowName, Ui::MainWindowOpenFlag());

        LogF(LOGL_INFO, "Loaded %s (build %s).", kVersionText, BuildStamp::Text());
    }

    void AddonUnload()
    {
        g_Api->GUI_DeregisterCloseOnEscape(Ui::kMainWindowName);
        g_Api->GUI_Deregister(AddonOptions);
        g_Api->GUI_Deregister(AddonRender);
        g_Api->QuickAccess_RemoveContextMenu(kShortcutMenuId);
        g_Api->QuickAccess_Remove(kShortcutId);
        g_Api->InputBinds_Deregister(kMenuKeybind);
        g_Api->InputBinds_Deregister(kToggleKeybind);

        PerfRec::Shutdown(); // a recording stops (its last rows go to the writer thread)
        Hotkeys::Shutdown(); // no new presses from here on
        Sender::Shutdown(); // joins the send thread, then removes the WndProc hook
        Ui::Shutdown();     // saves pending changes
        Languages::Shutdown(); // after Ui: nothing draws translated text any more
        MapNames::Shutdown(); // stops its lookup thread, saves maps.json
        Storage::Shutdown();  // last saver: writes what's still waiting (macros.json, settings.json), stops its thread
        Localization::ClearShared(); // every thread that could hold a TR() text has stopped

        LogF(LOGL_INFO, "Unloaded.");
        g_Api = nullptr;
    }

    void AddonRender()
    {
        PerfRec::FrameBegin(); // the frame recorder (only while recording): the previous frame ends here
        if (s_ToggleRequested.exchange(false)) Ui::ToggleMainWindow();
        if (s_MenuRequested.exchange(false)) Ui::ToggleQuickMenu();
        {
            const PerfRec::Scope perf(Perf::Section::Languages);
            Languages::Update(); // a language change or reload takes effect here, before anything is drawn
        }
        Ui::Render();
        PerfRec::FrameEnd();
    }

    void AddonOptions()
    {
        const PerfRec::Scope perf(Perf::Section::Options, true);
        Ui::RenderOptions();
    }

    void AddonQuickAccessMenu()
    {
        const PerfRec::Scope perf(Perf::Section::QuickMenu, true);
        Ui::RenderQuickAccessMenu();
    }

    void OnToggleKeybind(const char* aIdentifier, bool aIsRelease)
    {
        if (aIsRelease) return;
        if (std::strcmp(aIdentifier, kToggleKeybind) == 0) s_ToggleRequested = true;
        else if (std::strcmp(aIdentifier, kMenuKeybind) == 0) s_MenuRequested = true;
    }
}
