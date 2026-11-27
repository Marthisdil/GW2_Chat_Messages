#include "Ui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "imgui.h"
#include "imgui_internal.h" // BringWindowToDisplayFront: keep the error window on top

#include "Languages.h"
#include "Localization.h"

#include "Advanced.h"
#include "Backups.h"
#include "BuildStamp.h"
#include "ChatLine.h"
#include "Cooldown.h"
#include "EditorLayout.h"
#include "ErrorReport.h"
#include "Examples.h"
#include "Exchange.h"
#include "GameKeys.h"
#include "Globals.h"
#include "Hotkeys.h"
#include "LinkSync.h"
#include "MapNames.h"
#include "MacroTree.h"
#include "Mumble.h"
#include "Placeholders.h"
#include "QuickLists.h"
#include "ReleaseNotes.h"
#include "resource.h"
#include "Search.h"
#include "SendPlan.h"
#include "Sender.h"
#include "PathText.h"
#include "Storage.h"
#include "UiFont.h"
#include "Welcome.h"

using namespace MacroTree;
using UiFont::Px;
using Localization::Fmt;

namespace
{
    constexpr const char* kDragPayload     = "CHATMSG_NODE";
    // Pop-up titles: translated with T() where drawn; "###" keeps the ID the same in every language.
    constexpr char kNamePopup[]    = TX("Name###chatmsg_name");
    constexpr char kDeletePopup[]  = TX("Delete?###chatmsg_delete");
    constexpr char kRestorePopup[] = TX("Restore?###chatmsg_restore");
    constexpr char kPurgePopup[]   = TX("Delete permanently?###chatmsg_purge");
    constexpr char kExamplesReplacePopup[] = TX("Replace the examples?###chatmsg_examples_replace");
    constexpr char kExamplesAgainPopup[]   = TX("Add the examples again?###chatmsg_examples_again");
    constexpr char kWelcomePopup[]         = TX("Welcome to Chat Messages###chatmsg_welcome");
    constexpr const char* kTrashSetting    = "trash_bin";
    // settings.json "examples_added": the starter library was added (or a library already
    // existed), so a macros.json removed later on purpose doesn't bring the examples back.
    constexpr const char* kExamplesSetting = "examples_added";
    bool s_ExamplesAdded   = false; // that setting
    bool s_ExamplesRequest = false; // "Add the example macros" was clicked: the library window asks
    constexpr double      kSaveDelaySec    = 0.75; // batch quick edits (e.g. several expands) into one write
    constexpr double      kStatusShowSec   = 6.0;

    const ImVec4 kWarnColor  {1.00f, 0.80f, 0.35f, 1.0f};
    const ImVec4 kErrorColor {1.00f, 0.45f, 0.40f, 1.0f};
    const ImVec4 kFolderColor{0.88f, 0.86f, 0.70f, 1.0f};
    const ImVec4 kTrashColor {0.62f, 0.62f, 0.62f, 1.0f};
    const ImVec4 kCountNearColor {1.00f, 0.92f, 0.15f, 1.0f}; // character counter, 150+: yellow
    const ImVec4 kCountCloseColor{1.00f, 0.20f, 0.20f, 1.0f}; // 180+ and over: bright red

    ImVec4 ChannelColorVec(EChannel aChannel); // defined with the macro editor

    Tree s_Tree;
    bool s_ShowMainWindow = false;

    // Saving
    bool   s_Dirty      = false;
    double s_SaveAt     = 0.0;
    uint64_t s_TreeGen  = 0; // counts the library's changes (MarkDirty), for the backups
    bool   s_SaveFailed = false;

    // Messages from loading, shown until dismissed.
    std::vector<std::string> s_LoadMessages;
    // Short-lived status line (e.g. why a move was refused).
    std::string s_Status; // good news only; errors open the error window (ShowError)
    double      s_StatusUntil = 0.0;
    bool        s_StatusExport = false; // the status is "Exported to ...": offer the exports folder

    uint64_t s_SelectedId = 0;

    // Search box above the tree (docs/design.md, "Search"). s_Search is worked out every frame
    // before the tree is drawn; while it's active, only matches (and their folders) are shown,
    // and dragging and Move up/down are off.
    char           s_SearchBuf[128] = {};
    bool           s_SearchFocus    = false; // Ctrl+F: put the cursor in the box next frame
    bool           s_ActiveOnly     = false; // settings.json "active_only": only macros active here
    constexpr const char* kActiveOnlySetting = "active_only";
    constexpr const char* kRecentSetting      = "recently_sent"; // the macros sent last (QuickLists.h)
    constexpr const char* kRecentCountSetting = "recent_count";  // how many the quick menu shows
    Search::Result s_Search;
    const ImU32    kSearchHighlight = IM_COL32(255, 215, 0, 80);

    // What s_Search was worked out for (UpdateSearch).
    struct SearchCache
    {
        bool        Valid = false;
        std::string Query;
        bool        ActiveOnly = false;
        uint8_t     Mode  = 0;
        uint32_t    MapId = 0;
        uint64_t    Gen   = 0;
        const void* Lang  = nullptr; // Localization::Active(): the labels are translated
        double      At    = 0.0;
    };
    SearchCache s_SearchCache;

    // A tree row's text, and where the search matched in its name, built once per search change
    // (UpdateSearch clears them) instead of every frame.
    struct RowLabel
    {
        std::string Text;
        bool        Highlight = false;
        size_t      At = 0, Len = 0; // the match in the name
    };
    std::unordered_map<uint64_t, RowLabel> s_RowLabels;

    // Name popup (new folder, new macro, rename).
    enum class ENameMode { NewFolder, NewMacro, Rename };
    ENameMode   s_NameMode     = ENameMode::NewFolder;
    uint64_t    s_NameTargetId = 0; // parent folder for New*, the node for Rename (0 = root)
    char        s_NameBuf[256] = {};
    bool        s_NameFocus    = false;
    uint8_t     s_NameModes    = MacroTree::GM_All; // a folder's game modes, ticked in the pop-up

    uint64_t s_DeleteId = 0;

    // Trash bin (docs/design.md, "Trash bin"). Items are selected by the id of their top node.
    bool     s_TrashOn    = true;  // settings.json "trash_bin"; off = Delete removes for good
    bool     s_TrashOpen  = false; // the bin's tree node
    uint64_t s_TrashSelId = 0;     // selected bin item (shown when nothing in the tree is selected)
    uint64_t s_RestoreId  = 0;     // restore pop-up: its folder is gone
    uint64_t s_PurgeId    = 0;     // delete-permanently pop-up (0 with s_PurgeAll = empty the bin)
    bool     s_PurgeAll   = false;

    // Requests raised while drawing the tree, handled after it (never change the tree mid-draw).
    enum class EAction { None, Move, Shift, Sort, OpenName, OpenDelete, ExpandAll,
                         Trash, OpenRestore, RestoreNow, OpenPurge, OpenEmpty, Favourite, Duplicate };
    struct Action
    {
        EAction  Type   = EAction::None;
        uint64_t Id     = 0;
        uint64_t Target = 0;
        EDrop    Drop   = EDrop::Into;
        int      Delta  = 0;
        ENameMode Name  = ENameMode::NewFolder;
        bool     Flag   = false;
    };
    Action s_Pending;

    std::string s_DragError; // why the current drop target refuses, shown in the drag tooltip

    void MarkDirty()
    {
        s_Dirty  = true;
        s_SaveAt = ImGui::GetTime() + kSaveDelaySec;
        ++s_TreeGen;
    }

    // Opens the error window: aAction is what the user did, in their words ("Clicked Export all").
    // aFocus brings it to the front; false for errors from hotkeys and the quick menu, which happen
    // while the user is playing. Defined with the error window below.
    void ShowError(const std::string& aAction, const std::string& aMessage, bool aFocus = true);

    uint64_t s_SaveSeq = 0; // the newest save handed to the writer thread, until its result is in (0 = none)

    // The tree becomes JSON here (only the render thread may read it); the writer thread writes
    // the file, so a save never holds up a frame. PollSave picks up how it went.
    void SaveNow()
    {
        if (!s_Dirty) return;
        // A newer version's library is read-only (DrawMessages says so): never saved over.
        if (s_Tree.FromNewerVersion()) { s_Dirty = false; return; }
        const uint64_t seq = Storage::SaveMacrosLater(s_Tree);
        if (seq == 0) return;
        s_SaveSeq = seq;
        s_Dirty   = false;
    }

    // Once a frame: the result of the newest save. A failure retries in 5 seconds (without
    // hammering the disk) and says so once.
    void PollSave()
    {
        if (s_SaveSeq == 0) return;
        const Storage::SaveResult r = Storage::LastMacrosSave();
        if (r.Seq != s_SaveSeq) return; // still being written
        s_SaveSeq = 0;
        const bool failedBefore = s_SaveFailed;
        s_SaveFailed = !r.Ok;
        if (r.Ok) return;
        s_Dirty  = true;
        s_SaveAt = ImGui::GetTime() + 5.0;
        if (!failedBefore)
            ShowError(T("Changed the library (it's saved to macros.json automatically)"),
                      T("Your macros couldn't be saved. Check the Nexus log. The addon keeps trying every 5 seconds."), false);
    }

    // Good news ("Exported to ..."), shown for a few seconds under the toolbar.
    void SetInfo(const std::string& aText, bool aExported = false)
    {
        s_Status       = aText;
        s_StatusUntil  = ImGui::GetTime() + kStatusShowSec;
        s_StatusExport = aExported;
    }

    // --- Where the player is (for filters) ------------------------------------------------------

    Mumble::Data* s_Mumble = nullptr;

    struct Here
    {
        uint8_t  Mode  = GM_None; // GM_PvE / GM_WvW / GM_PvP, or GM_None when unknown
        uint32_t MapId = 0;
    };

    Here WhereAmI()
    {
        Here h;
        if (!s_Mumble) return h;
        h.Mode  = GameModeOfMapType(static_cast<uint32_t>(s_Mumble->Context.MapType));
        h.MapId = s_Mumble->Context.MapID;
        return h;
    }

    // Which folders and macros are active where the player is (their filters and the folders'
    // above them), worked out once per map change or library change and reused: the tree and the
    // quick menu ask for every row every frame, and each answer walks the node's folders. Also
    // worked out again twice a second, in case a change ever skips MarkDirty.
    struct ActiveHere
    {
        bool     Valid = false;
        uint8_t  Mode  = GM_None;
        uint32_t MapId = 0;
        uint64_t Gen   = 0;
        double   At    = 0.0;
        bool     RootHasActive = false;
        std::unordered_map<uint64_t, bool> Active;    // every node in the tree
        std::unordered_map<uint64_t, bool> HasActive; // folders: an active macro somewhere inside
    };
    ActiveHere       s_ActiveHere;
    constexpr double kCacheRefreshSec = 0.5; // the caches below are rebuilt at least this often

    // Fills aOut for aFolder's contents; true when an active macro is somewhere inside.
    bool FillActive(const Node& aFolder, const Here& aHere, ActiveHere& aOut)
    {
        bool any = false;
        for (const auto& c : aFolder.Children)
        {
            const bool active = IsActive(*c, aHere.Mode, aHere.MapId);
            aOut.Active[c->Id] = active;
            if (c->IsMacro()) any = any || active;
            else
            {
                const bool inside = FillActive(*c, aHere, aOut);
                aOut.HasActive[c->Id] = inside;
                any = any || inside;
            }
        }
        return any;
    }

    const ActiveHere& ActiveFor(const Here& aHere)
    {
        ActiveHere& a = s_ActiveHere;
        const double now = ImGui::GetTime();
        if (a.Valid && a.Mode == aHere.Mode && a.MapId == aHere.MapId && a.Gen == s_TreeGen && now >= a.At &&
            now - a.At < kCacheRefreshSec)
            return a;
        a.Active.clear();
        a.HasActive.clear();
        a.Valid = true;
        a.Mode  = aHere.Mode;
        a.MapId = aHere.MapId;
        a.Gen   = s_TreeGen;
        a.At    = now;
        a.RootHasActive = FillActive(s_Tree.Root(), aHere, a);
        return a;
    }

    // A macro or folder, with the filters of the folders above it.
    bool IsActiveHere(const Node& aNode, const Here& aHere)
    {
        const ActiveHere& a = ActiveFor(aHere);
        const auto it = a.Active.find(aNode.Id);
        if (it != a.Active.end()) return it->second;
        return IsActive(aNode, aHere.Mode, aHere.MapId); // not in the tree (the trash bin, an import)
    }

    // An active macro somewhere inside aFolder.
    bool HasActiveMacros(const Node& aFolder, const Here& aHere)
    {
        const ActiveHere& a = ActiveFor(aHere);
        if (&aFolder == &s_Tree.Root()) return a.RootHasActive;
        const auto it = a.HasActive.find(aFolder.Id);
        if (it != a.HasActive.end()) return it->second;
        for (const auto& c : aFolder.Children) // not in the tree
            if (c->IsMacro() ? IsActive(*c, aHere.Mode, aHere.MapId) : HasActiveMacros(*c, aHere)) return true;
        return false;
    }

    const char* ModeName(uint8_t aMode)
    {
        switch (aMode)
        {
        case GM_PvE: return T("PvE");
        case GM_WvW: return T("WvW");
        case GM_PvP: return T("PvP");
        default:     return T("no game mode");
        }
    }

    // "Everywhere", "WvW", "PvE, PvP", "PvE on 2 maps"...
    std::string FilterSummary(const Node& aMacro)
    {
        std::string modes;
        const uint8_t m = aMacro.GameModes == GM_None ? static_cast<uint8_t>(GM_All) : aMacro.GameModes;
        if (m != GM_All)
        {
            const uint8_t bits[] = {GM_PvE, GM_WvW, GM_PvP};
            for (uint8_t b : bits)
                if (m & b) modes += (modes.empty() ? "" : ", ") + std::string(ModeName(b));
        }
        if (aMacro.MapIds.empty()) return modes.empty() ? std::string(T("Everywhere")) : modes;
        const std::string maps = aMacro.MapIds.size() == 1 ? std::string(T("1 map")) : Fmt(T("%zu maps"), aMacro.MapIds.size());
        return modes.empty() ? Fmt(T("On %s"), maps.c_str()) : Fmt(T("%s on %s"), modes.c_str(), maps.c_str());
    }

    // A folder's filter as a short tag after its name in the tree ("  [WvW, 2 maps]"), or "".
    std::string FolderTag(const Node& aFolder)
    {
        if (!aFolder.IsFolder() || !HasFilter(aFolder)) return std::string();
        std::string tag;
        const uint8_t m = aFolder.GameModes == GM_None ? static_cast<uint8_t>(GM_All) : aFolder.GameModes;
        if (m != GM_All)
        {
            const uint8_t bits[] = {GM_PvE, GM_WvW, GM_PvP};
            for (uint8_t b : bits)
                if (m & b) tag += (tag.empty() ? "" : ", ") + std::string(ModeName(b));
        }
        if (!aFolder.MapIds.empty())
            tag += (tag.empty() ? "" : ", ") + (aFolder.MapIds.size() == 1 ? std::string(T("1 map"))
                                                                          : Fmt(T("%zu maps"), aFolder.MapIds.size()));
        return "  [" + tag + "]";
    }

    // The folders above a node that have a filter, "Raids (PvE), Wing (On 2 maps)", or "".
    std::string FolderLimits(const Node& aNode)
    {
        std::vector<const Node*> chain;
        for (const Node* p = aNode.Parent; p && p->Parent; p = p->Parent)
            if (HasFilter(*p)) chain.push_back(p);
        std::string text;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
            text += (text.empty() ? "" : ", ") + (*it)->Name + " (" + FilterSummary(**it) + ")";
        return text;
    }

    // --- Errors -----------------------------------------------------------------------------------
    // Every error opens the error window: what went wrong, what the user did, OK, and Copy to
    // clipboard (src/ErrorReport.*). It's a normal window, not an ImGui pop-up: while a pop-up is
    // open ImGui takes every mouse click, which would block the game after a hotkey error. Only an
    // error inside our Name pop-up is shown as a pop-up within it (a pop-up blocks other windows).

    constexpr char kErrorWindow[] = TX("Chat Messages: error###chatmsg_error");
    constexpr char kErrorPopup[]  = TX("Chat Messages: error###chatmsg_errorpopup");

    std::vector<ErrorReport::Report> s_Errors; // oldest first; the window shows the first
    bool   s_ErrorFocus       = false; // bring the window to the front next frame
    bool   s_ErrorInPopup     = false; // show it inside the Name pop-up instead of the window
    double s_ErrorCopiedUntil = 0.0;

    std::string NowText()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
        return buf;
    }

    // When a bin item was deleted, in local time ("2026-11-21 14:03"). Saved in UTC (UtcStamp);
    // anything else in the file is shown as it is.
    std::string DeletedText(const MacroTree::TrashItem& aItem)
    {
        std::time_t when = 0;
        if (!MacroTree::ParseUtcStamp(aItem.DeletedAt, when)) return aItem.DeletedAt;
        std::tm local{};
        if (localtime_s(&local, &when) != 0) return aItem.DeletedAt;
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &local);
        return buf;
    }

    // One line as it's typed, in its channel's colour; a newer version's channel as "? key", in red
    // (that line is never sent).
    void DrawLinePreview(const Line& aLine)
    {
        if (aLine.UnknownChannel.empty())
            ImGui::TextColored(ChannelColorVec(aLine.Channel), "%s", ChatLine::TypedText(aLine).c_str());
        else
            ImGui::TextColored(kErrorColor, "? %s  %s", aLine.UnknownChannel.c_str(), aLine.Message.c_str());
    }

    std::string WhereText()
    {
        const Here h = WhereAmI();
        if (h.Mode == GM_None && h.MapId == 0) return {};
        return std::string(ModeName(h.Mode)) + ", " + (h.MapId ? MapNames::Label(h.MapId) : std::string(T("no map")));
    }

    void ShowError(const std::string& aAction, const std::string& aMessage, bool aFocus)
    {
        ErrorReport::Add(s_Errors, {aAction, aMessage, NowText(), WhereText()});
        if (aFocus) s_ErrorFocus = true;
    }

    // The first error and its buttons. Returns true when OK was clicked (that error is gone).
    bool DrawErrorBody()
    {
        if (s_Errors.empty()) return true;
        const ErrorReport::Report& e = s_Errors.front();
        if (s_Errors.size() > 1) ImGui::TextDisabled(T("Error 1 of %zu"), s_Errors.size());
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kErrorColor, "%s", e.Message.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled(T("What you did: %s"), e.Action.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        const bool ok = ImGui::Button(T("OK"), Px(100.0f, 0.0f));
        ImGui::SameLine();
        if (ImGui::Button(T("Copy to clipboard")))
        {
            ImGui::SetClipboardText(ErrorReport::ToText(e, std::string(kVersionText) + " (build " + BuildStamp::Text() + ")").c_str());
            s_ErrorCopiedUntil = ImGui::GetTime() + 3.0;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Copies the error, what you did, the time and where you were in game,\nready to paste into a bug report."));
        if (ImGui::GetTime() < s_ErrorCopiedUntil)
        {
            ImGui::SameLine();
            ImGui::TextDisabled(T("Copied."));
        }
        if (ok)
        {
            s_Errors.erase(s_Errors.begin());
            s_ErrorCopiedUntil = 0.0;
        }
        return ok;
    }

    // True when the library, Import or Export window is drawn above the error window and no pop-up
    // is open. Only then is the error window brought forward: never over a pop-up (a combo list,
    // a menu, a modal it couldn't be clicked through) or another addon's window.
    bool OurWindowAboveError(ImGuiWindow* aError)
    {
        if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup)) return false;
        const ImGuiWindow* ours[] = {ImGui::FindWindowByName(Ui::kMainWindowName),
                                     ImGui::FindWindowByName("###chatmsg_import"),
                                     ImGui::FindWindowByName("###chatmsg_export")};
        bool pastError = false;
        for (const ImGuiWindow* w : GImGui->Windows) // back to front
        {
            if (w == aError) { pastError = true; continue; }
            if (!pastError || !w->Active) continue;
            for (const ImGuiWindow* o : ours)
                if (o && w == o) return true;
        }
        return false;
    }

    void DrawErrorWindow()
    {
        if (s_Errors.empty() || s_ErrorInPopup) return;
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.35f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(Px(460.0f), 0.0f), ImGuiCond_Always);
        if (s_ErrorFocus)
        {
            ImGui::SetNextWindowFocus();
            s_ErrorFocus = false;
        }
        if (ImGui::Begin(T(kErrorWindow), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            // Stay in front of the library while there's an error, without taking keyboard focus:
            // focusing it once wasn't enough, a click in the library hid it behind (in game, 2026-11-25).
            if (OurWindowAboveError(ImGui::GetCurrentWindow())) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            DrawErrorBody();
        }
        ImGui::End();
    }

    // Inside one of our pop-ups: the error as a pop-up within it. Returns true when it was closed.
    bool DrawErrorPopupInside()
    {
        if (!s_ErrorInPopup) return false;
        if (s_Errors.empty()) { s_ErrorInPopup = false; return true; }
        if (!ImGui::IsPopupOpen(kErrorPopup)) ImGui::OpenPopup(kErrorPopup);
        ImGui::SetNextWindowSize(ImVec2(Px(420.0f), 0.0f), ImGuiCond_Always);
        bool closed = false;
        if (ImGui::BeginPopupModal(T(kErrorPopup), nullptr, ImGuiWindowFlags_NoResize))
        {
            if (DrawErrorBody() && s_Errors.empty())
            {
                s_ErrorInPopup = false;
                closed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        return closed;
    }

    // --- Sending ------------------------------------------------------------------------------
    // The sender runs on its own thread; the UI only starts it and shows its status.

    uint64_t       s_SendSeenSeq = 0; // Sender::Status::Seq of the last ended send we reported
    std::string    s_SendNote;                        // "Sent 2 lines." (failures open the error window)
    double         s_SendNoteUntil = 0.0;
    std::string    s_SendAction;                      // what started the current/last send, for its error

    // Where a send was started. Hotkey and menu sends report failures as on-screen alerts, because
    // the library window (and its status line) may not be visible.
    enum class ESendFrom { Window, Hotkey, Menu };
    bool s_SendAlertOnFail = false; // for the current/last send

    // The last send of each macro, for hotkey cooldowns and the 1 s guard (Cooldown.h). Sends from
    // anywhere count, but only hotkeys wait: Send now and the quick menu always send.
    Cooldown::Tracker s_Cooldowns;

    // The macros sent last, from anywhere, for the quick menu's "Recently sent" (QuickLists.h).
    QuickLists::Recent s_Recent;
    int                s_RecentCount = QuickLists::kDefaultRecent;

    // Advanced options (Advanced.h): settings.json "advanced_options". Off hides the features
    // behind it, and a macro that uses one can't be sent; its settings are kept.
    bool s_AdvancedOn = false;
    constexpr const char* kAdvancedHelpUrl = "https://github.com/Marthisdil/GW2_Chat_Messages/wiki/Advanced-Options";

    bool IsBlocked(const Node& aMacro) { return Advanced::Blocked(aMacro, s_AdvancedOn); }

    // The last "its hotkey does nothing" alert for a blocked macro (HandleHotkeys).
    uint64_t         s_BlockedAlertId = 0;
    double           s_BlockedAlertAt = 0.0;
    constexpr double kBlockedAlertEverySec = 5.0;

    void SaveRecent()
    {
        if (!Storage::SaveSetting(kRecentSetting, s_Recent.ToJson()))
            LogF(LOGL_WARNING, "The recently sent list couldn't be saved.");
    }

    // A short Nexus on-screen alert: the only feedback a hotkey has when the library is closed.
    void Alert(const std::string& aText)
    {
        const std::string text = std::string(kAddonName) + ": " + aText;
        g_Api->GUI_SendAlert(text.c_str());
    }

    void SendMacro(const Node& aMacro, ESendFrom aFrom = ESendFrom::Window)
    {
        const bool alert = aFrom != ESendFrom::Window;
        if (aFrom == ESendFrom::Hotkey && Sender::IsBusy()) return; // one macro at a time; extra presses are ignored
        if (aFrom == ESendFrom::Hotkey)
        {
            // Still cooling down: the 1 s guard alone is silent; a macro's own cooldown shows one
            // alert, on the first ignored press.
            // A macro's own cooldown is an advanced option (ignored while off); the 1 s guard always applies.
            const Cooldown::Wait wait = s_Cooldowns.ForHotkey(aMacro.Id, Advanced::CooldownSec(aMacro, s_AdvancedOn), ImGui::GetTime());
            if (wait.Seconds > 0.0)
            {
                if (wait.ByCooldown && s_Cooldowns.TakeAlert(aMacro.Id))
                    Alert(Fmt(T("%s: cooldown, wait %d s."), aMacro.Name.c_str(), Cooldown::WholeSeconds(wait.Seconds)));
                return;
            }
        }
        const std::string name = "\"" + aMacro.Name + "\"";
        const std::string action = aFrom == ESendFrom::Hotkey ? Fmt(T("Pressed the hotkey %s to send %s"), Hotkey::Display(aMacro.Hotkey).c_str(), name.c_str())
                                 : aFrom == ESendFrom::Menu   ? Fmt(T("Sent %s from the quick menu"), name.c_str())
                                                              : Fmt(T("Sent %s from the library (Send now)"), name.c_str());
        std::string error;
        if (IsBlocked(aMacro))
        {
            error = T("This macro is linked to a text file, which is an advanced option, and advanced options are off. "
                      "Turn on \"Advanced options\" on the addon's options page to send it.");
            ShowError(action, error, !alert);
            if (alert) Alert(aMacro.Name + ": " + error);
            return;
        }
        if (Sender::Start(aMacro.Id, aMacro.Lines, error))
        {
            s_Cooldowns.Started(aMacro.Id, ImGui::GetTime());
            s_Recent.Sent(aMacro.Id);
            SaveRecent();
            s_SendAlertOnFail = alert;
            s_SendAction      = action;
            return;
        }
        s_SendSeenSeq = Sender::GetStatus().Seq; // already reported here
        ShowError(action, error, !alert);
        if (alert) Alert(aMacro.Name + ": " + error);
    }

    void PollSender()
    {
        if (Sender::StatusSeq() == s_SendSeenSeq) return; // nothing ended: skip copying the status
        const Sender::Status st = Sender::GetStatus();
        if (st.Seq == s_SendSeenSeq) return;
        s_SendSeenSeq = st.Seq;
        if (st.State == Sender::EState::Done || st.State == Sender::EState::Failed)
            s_Cooldowns.Ended(st.MacroId, ImGui::GetTime(), st.State == Sender::EState::Done);
        if (st.State == Sender::EState::Done)
        {
            s_SendNote      = st.Message;
            s_SendNoteUntil = ImGui::GetTime() + kStatusShowSec;
        }
        else if (st.State == Sender::EState::Failed)
        {
            s_SendNote.clear();
            ShowError(s_SendAction, st.Message, !s_SendAlertOnFail);
            if (s_SendAlertOnFail)
            {
                const Node* m = s_Tree.Find(st.MacroId);
                Alert((m ? m->Name + ": " : std::string()) + st.Message);
            }
        }
    }

    // --- Hotkeys ------------------------------------------------------------------------------
    // Presses are read on the window thread (Hotkeys.cpp) and handled here, on the render thread.

    uint64_t s_CaptureFor = 0; // macro getting a new hotkey (0 = none)

    void CancelCapture()
    {
        Hotkeys::CancelCapture();
        s_CaptureFor = 0;
    }

    // Looks up the current map's name in the background whenever the map changes, so {map} and
    // the filter editor rarely have to wait for it.
    uint32_t s_LastMapId = 0;

    void WatchMap()
    {
        const uint32_t id = WhereAmI().MapId;
        if (id == s_LastMapId) return;
        s_LastMapId = id;
        MapNames::NoteVisit(id); // remembered for the map picker, and its name looked up
    }

    void HandleHotkeys()
    {
        Hotkeys::SetUiTyping(ImGui::GetIO().WantTextInput);

        // A capture only lasts while its macro is on screen.
        if (s_CaptureFor && (!s_ShowMainWindow || s_SelectedId != s_CaptureFor)) CancelCapture();

        Hotkey::Combo combo;
        bool cancelled = false;
        if (Hotkeys::PopCapture(combo, cancelled))
        {
            Node* m = s_Tree.Find(s_CaptureFor);
            s_CaptureFor = 0;
            if (!cancelled && m && m->IsMacro())
            {
                const std::string why = Hotkey::Refusal(combo);
                if (!why.empty()) ShowError(Fmt(T("Set a hotkey for \"%s\" and pressed %s"), m->Name.c_str(), Hotkey::Display(combo).c_str()), why);
                else
                {
                    m->Hotkey = combo;
                    MarkDirty();
                }
            }
        }

        while (Hotkeys::PopPress(combo))
        {
            if (s_CaptureFor) continue;
            // An old plain Mouse 3/4/5 hotkey never fires (mouse buttons need Ctrl, Alt or Shift).
            if (!Hotkey::Fires(combo)) continue;
            // Only macros active here count: the same hotkey can be used for, say, a WvW macro and a PvE one.
            const Here here = WhereAmI();
            // A macro blocked by Advanced options still counts here, so a shared key sends nothing,
            // as the hotkey editor's "Also set on" warning says.
            std::vector<Node*> macros;
            for (Node* m : s_Tree.MacrosWithHotkey(combo))
                if (IsActiveHere(*m, here)) macros.push_back(m);
            if (macros.empty()) continue;
            if (macros.size() == 1 && IsBlocked(*macros[0]))
            {
                // One alert per macro every few seconds, so mashing the key doesn't stack them.
                const double now = ImGui::GetTime();
                if (s_BlockedAlertId != macros[0]->Id || now - s_BlockedAlertAt >= kBlockedAlertEverySec)
                {
                    s_BlockedAlertId = macros[0]->Id;
                    s_BlockedAlertAt = now;
                    Alert(Fmt(T("%s is linked to a text file, an advanced option that's off, so its hotkey does nothing."),
                              macros[0]->Name.c_str()));
                }
                continue;
            }
            if (macros.size() > 1)
            {
                const std::string why = Fmt(T("%s is set on %zu macros, so nothing was sent. Give each macro its own hotkey."),
                                            Hotkey::Display(combo).c_str(), macros.size());
                ShowError(Fmt(T("Pressed the hotkey %s"), Hotkey::Display(combo).c_str()), why, false);
                Alert(why);
                continue;
            }
            SendMacro(*macros[0], ESendFrom::Hotkey);
        }
    }

    void DrawHotkeyRow(Node& aMacro)
    {
        UiFont::Row row;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Hotkey:"));
        if (s_CaptureFor == aMacro.Id && Hotkeys::IsCapturing())
        {
            ImGui::SameLine();
            ImGui::TextColored(kWarnColor, T("Press a key or mouse button (Escape cancels)..."));
            row.Next(UiFont::ButtonWidth(T("Cancel")));
            if (ImGui::SmallButton(T("Cancel"))) CancelCapture();
            return;
        }

        const std::string keys = aMacro.Hotkey.Empty() ? std::string(T("None")) : Hotkey::Display(aMacro.Hotkey);
        row.Next(UiFont::TextWidth(keys.c_str()));
        if (aMacro.Hotkey.Empty()) ImGui::TextDisabled(T("None"));
        else if (Hotkey::NeedsModifier(aMacro.Hotkey)) ImGui::TextColored(kErrorColor, "%s", keys.c_str());
        else ImGui::TextUnformatted(keys.c_str());
        const char* setLabel = aMacro.Hotkey.Empty() ? T("Set...") : T("Change...");
        row.Next(UiFont::ButtonWidth(setLabel));
        if (ImGui::SmallButton(setLabel))
        {
            s_CaptureFor = aMacro.Id;
            Hotkeys::StartCapture();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Then press the key you want, with Ctrl, Alt or Shift if you like.\n"
                              "Mouse 3 (middle), Mouse 4 and Mouse 5 work too, together with Ctrl, Alt or Shift.\n"
                              "The key still does its normal job in game, so pick one you don't use there.\n"
                              "Hotkeys don't fire while you're typing in chat or in a text box."));
        if (!aMacro.Hotkey.Empty())
        {
            row.Next(UiFont::ButtonWidth(T("Clear##hotkey")));
            if (ImGui::SmallButton(T("Clear##hotkey")))
            {
                aMacro.Hotkey = {};
                MarkDirty();
            }
            if (Hotkey::NeedsModifier(aMacro.Hotkey))
            {
                ImGui::TextColored(kErrorColor, T("This hotkey doesn't work any more: mouse buttons need Ctrl, Alt or "
                                                "Shift. Click Change... to set a new one."));
                return;
            }
            // A shared hotkey is only a conflict where both macros can be active at once. Worked
            // out again when the macro, its hotkey or the library changed (or twice a second).
            static struct { uint64_t Id = 0; Hotkey::Combo Key; uint64_t Gen = 0; double At = -1.0; std::string Names; } shared;
            const double now = ImGui::GetTime();
            if (shared.Id != aMacro.Id || !(shared.Key == aMacro.Hotkey) || shared.Gen != s_TreeGen || now < shared.At ||
                now - shared.At >= kCacheRefreshSec)
            {
                shared.Id  = aMacro.Id;
                shared.Key = aMacro.Hotkey;
                shared.Gen = s_TreeGen;
                shared.At  = now;
                shared.Names.clear();
                for (const Node* n : s_Tree.MacrosWithHotkey(aMacro.Hotkey))
                    if (n != &aMacro && FiltersOverlap(aMacro, *n)) shared.Names += (shared.Names.empty() ? "" : ", ") + n->Name;
            }
            const std::string& names = shared.Names;
            if (!names.empty())
            {
                ImGui::TextColored(kWarnColor, T("Also set on: %s. Where both are active, this hotkey sends nothing."),
                                   names.c_str());
            }
            // GW2's default keys (GameKeys): the user's own binds may differ, so it's a hint.
            if (const char* use = GameKeys::DefaultUse(aMacro.Hotkey))
            {
                ImGui::TextColored(kWarnColor, T("In GW2's default keys, %s is \"%s\". If it's still a game key for "
                                               "you, pressing it does both. Your own keys may differ."),
                                   keys.c_str(), Localization::Lookup(use));
            }
            else if (!aMacro.Hotkey.HasModifier() && aMacro.Hotkey.Key < 0x60 && aMacro.Hotkey.Key > Hotkey::kVkXButton2)
            {
                ImGui::TextColored(kWarnColor, T("Plain letters, numbers and keys like Space are usually game keys. "
                                               "Adding Ctrl, Alt or Shift is safer."));
            }
        }
    }

    // Marks or unmarks a folder or macro as a favourite. Marking one more than
    // QuickLists::kMaxFavourites is refused, with the reason (nothing is dropped quietly).
    void SetFavourite(Node& aNode, bool aOn, const char* aAction)
    {
        if (aOn == aNode.Favourite) return;
        if (aOn && !QuickLists::CanAddFavourite(s_Tree))
        {
            ShowError(aAction, Localization::Fmt(TR("You can have up to %d favourites. Untick one first, then mark \"%s\"."),
                                                 QuickLists::kMaxFavourites, aNode.Name.c_str()));
            return;
        }
        aNode.Favourite = aOn;
        MarkDirty();
    }

    // Favourite: listed at the top of the quick menu, wherever you are.
    void DrawFavouriteRow(Node& aNode)
    {
        bool fav = aNode.Favourite;
        if (ImGui::Checkbox(T("Favourite"), &fav)) SetFavourite(aNode, fav, T("Ticked Favourite"));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Favourites are listed at the top of the quick menu on every map (faded where\n"
                              "they're not active), and marked with * in the library. Up to %d folders and macros."),
                              QuickLists::kMaxFavourites);
    }

    // Cooldown: how long the hotkey waits before sending this macro again (0 = off).
    void DrawCooldownRow(Node& aMacro)
    {
        if (!s_AdvancedOn) // an advanced option: hidden, and ignored, while they're off
        {
            if (aMacro.CooldownSec > 0)
                ImGui::TextDisabled(T("Cooldown: %d s, ignored while advanced options are off."), aMacro.CooldownSec);
            return;
        }
        UiFont::Row row;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Cooldown:"));
        row.Next(Px(70.0f));
        ImGui::SetNextItemWidth(Px(70.0f));
        int sec = aMacro.CooldownSec;
        if (ImGui::InputInt("##cooldown", &sec, 0, 0))
        {
            sec = std::clamp(sec, 0, Cooldown::kMaxSec);
            if (sec != aMacro.CooldownSec)
            {
                aMacro.CooldownSec = sec;
                MarkDirty();
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Seconds the hotkey waits before it sends this macro again (0 = off, at most 600).\n"
                              "Presses during the cooldown are ignored; the first one shows an alert.\n"
                              "Send now and the quick menu always send, but they start the cooldown too.\n"
                              "Even with 0, a hotkey ignores presses for 1 second after its macro was sent."));
        row.Next(UiFont::TextWidth(T("seconds")));
        ImGui::TextDisabled(T("seconds"));
        const Cooldown::Wait wait = s_Cooldowns.ForHotkey(aMacro.Id, aMacro.CooldownSec, ImGui::GetTime());
        if (wait.ByCooldown)
        {
            const std::string left = Fmt(T("(%d s left)"), Cooldown::WholeSeconds(wait.Seconds));
            row.Next(UiFont::TextWidth(left.c_str()));
            ImGui::TextColored(kWarnColor, "%s", left.c_str());
        }
    }

    int s_MapIdInput = 0;

    // The filter editor's map search: names from the GW2 API's full list (MapNames), maps you've
    // been to first. Results are refreshed when the text changes and twice a second (names that
    // arrive meanwhile).
    char                         s_MapSearch[64] = "";
    std::string                  s_MapSearchFor;
    int                          s_MapSearchFrame = -1000;
    std::vector<MapNames::Entry> s_MapResults;
    constexpr size_t             kMapResults = 12;

    std::string MapEntryText(const MapNames::Entry& aEntry)
    {
        std::string text = aEntry.Name.empty() ? Fmt(T("Map %u"), static_cast<unsigned>(aEntry.Id)) : aEntry.Name;
        if (!aEntry.Region.empty()) text += " - " + aEntry.Region;
        text += " (" + std::to_string(aEntry.Id) + ")";
        if (aEntry.Visited) text += std::string("  ") + T("(been there)");
        return text;
    }

    template <typename HasMap, typename AddMap>
    void DrawMapPicker(const HasMap& aHasMap, const AddMap& aAddMap)
    {
        auto pick = [&](const MapNames::Entry& aEntry) {
            ImGui::PushID(static_cast<int>(aEntry.Id));
            const bool have = aHasMap(static_cast<int>(aEntry.Id));
            const std::string text = MapEntryText(aEntry);
            if (have) ImGui::TextDisabled(T("%s (in the list)"), text.c_str());
            else if (ImGui::Selectable(text.c_str())) aAddMap(static_cast<int>(aEntry.Id));
            ImGui::PopID();
        };

        UiFont::Row row;
        ImGui::SetNextItemWidth(Px(240.0f));
        ImGui::InputTextWithHint("##mapsearch", T("Find a map by name or id"), s_MapSearch, sizeof(s_MapSearch));
        const std::vector<MapNames::Entry> visited = MapNames::Visited(30);
        row.Next(UiFont::ButtonWidth(T("Maps you've been to")) + ImGui::GetFrameHeight());
        ImGui::SetNextItemWidth(UiFont::ButtonWidth(T("Maps you've been to")) + ImGui::GetFrameHeight());
        if (ImGui::BeginCombo("##visited", T("Maps you've been to")))
        {
            if (visited.empty()) ImGui::TextDisabled(T("None yet: maps are remembered as you visit them."));
            for (const MapNames::Entry& e : visited) pick(e);
            ImGui::EndCombo();
        }

        const std::string query = Search::Trim(s_MapSearch);
        if (query.empty()) return;
        const int frame = ImGui::GetFrameCount();
        if (query != s_MapSearchFor || frame - s_MapSearchFrame >= 30 || frame < s_MapSearchFrame)
        {
            s_MapResults = MapNames::Find(query, kMapResults);
            s_MapSearchFor = query;
            s_MapSearchFrame = frame;
        }
        if (s_MapResults.empty())
        {
            const MapNames::State st = MapNames::GetState();
            if (st.Listed == 0 && st.Refreshing) ImGui::TextDisabled(T("The map list is still loading from the GW2 API..."));
            else if (st.Listed == 0 && s_AdvancedOn)
                ImGui::TextDisabled(T("The map list isn't loaded yet (it comes from the GW2 API). Try Refresh map list "
                                    "on the addon's options page, or add the map by its id."));
            else if (st.Listed == 0)
                ImGui::TextDisabled(T("The map list isn't loaded yet (it comes from the GW2 API). Try again later, or add "
                                    "the map by its id."));
            else ImGui::TextDisabled(T("No map matches."));
            return;
        }
        ImGui::BeginChild("##mapresults", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() *
                          (static_cast<float>(std::min<size_t>(s_MapResults.size(), 6)) + 0.5f)), true);
        for (const MapNames::Entry& e : s_MapResults) pick(e);
        ImGui::EndChild();
        if (s_MapResults.size() >= kMapResults) ImGui::TextDisabled(T("Only the first %zu matches are shown."), kMapResults);
    }

    // --- The tour (docs/design.md, "First-run welcome and tour") -------------------------------
    // While it runs, the parts of the library window a step can point at note where they were drawn
    // this frame (TourMark); DrawTour then outlines the current step's part and puts its box beside it.

    constexpr size_t kTourTargets = static_cast<size_t>(Welcome::ETarget::Count);
    bool   s_TourOn           = false;
    size_t s_TourStep         = 0;
    bool   s_TourEntered      = false; // the step's setup (selecting a macro) is still to do
    bool   s_TourStartRequest = false; // the options page's "Take the tour"
    ImVec2 s_TourMin[kTourTargets];
    ImVec2 s_TourMax[kTourTargets];
    bool   s_TourSeen[kTourTargets] = {};
    ImVec2 s_TourBoxSize; // the box's size last frame, to place it
    Welcome::ETarget s_TourScrollTo = Welcome::ETarget::None; // scroll this part into view once

    // After the item (or group, or child window) that a step points at. Only the part its pane
    // shows counts; when a step starts, a part scrolled out of its pane is scrolled into view.
    void TourMark(Welcome::ETarget aTarget)
    {
        const size_t i = static_cast<size_t>(aTarget);
        if (!s_TourOn || i == 0 || i >= kTourTargets) return;
        const ImRect clip = ImGui::GetCurrentWindow()->ClipRect;
        const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        if (s_TourScrollTo == aTarget)
        {
            s_TourScrollTo = Welcome::ETarget::None;
            if (min.y < clip.Min.y || min.y > clip.Max.y) ImGui::SetScrollHereY(0.0f);
        }
        s_TourMin[i]  = ImVec2(std::clamp(min.x, clip.Min.x, clip.Max.x), std::clamp(min.y, clip.Min.y, clip.Max.y));
        s_TourMax[i]  = ImVec2(std::clamp(max.x, clip.Min.x, clip.Max.x), std::clamp(max.y, clip.Min.y, clip.Max.y));
        s_TourSeen[i] = true;
    }

    void StartTour()
    {
        s_TourOn      = true;
        s_TourStep    = 0;
        s_TourEntered = true;
    }

    void HandleWelcomeRequest(); // the first-run welcome, below with the tour's box
    void DrawWelcomePopup();

    // --- Folding sections of the details pane (docs/design.md, "Macro editor layout") --------------

    EditorLayout::Sections s_Sections; // settings.json "editor_sections": which sections are open

    // A folding section: its header shows a summary, and a warning (in yellow) even while it's
    // closed, so folding never hides a problem. Open or closed is the same for every macro.
    bool Section(const std::string& aHeader, const char* aWarning, bool& aOpen, const char* aId)
    {
        const std::string label = aWarning ? aHeader + "  " + aWarning : aHeader;
        ImGui::SetNextItemOpen(aOpen, ImGuiCond_Always);
        if (aWarning) ImGui::PushStyleColor(ImGuiCol_Text, kWarnColor);
        // Drawn with "%s": a "##" in a name or a note is just text, not part of the id.
        const bool open = ImGui::TreeNodeEx(aId, ImGuiTreeNodeFlags_CollapsingHeader, "%s", label.c_str());
        if (aWarning) ImGui::PopStyleColor();
        if (open != aOpen)
        {
            aOpen = open;
            if (!Storage::SaveSetting(EditorLayout::kSetting, EditorLayout::ToJson(s_Sections)))
                LogF(LOGL_WARNING, "The editor's open sections couldn't be saved.");
        }
        return open;
    }

    // "Where it's active" for a macro or a folder (a folder's filter limits everything inside it).
    void DrawFilters(Node& aNode)
    {
        const bool folder = aNode.IsFolder();
        const Here here = WhereAmI();
        const bool active = IsActiveHere(aNode, here);
        const std::string header = Fmt(T("Where it's active: %s"), FilterSummary(aNode).c_str());
        const char* warning = NeverActive(aNode) ? T("(can't be active anywhere)") : active ? nullptr : T("(not here)");
        if (!Section(header, warning, s_Sections.Where, "##sec_where")) return;
        ImGui::Indent();
        if (folder)
            ImGui::TextWrapped(T("A folder's filter limits everything inside it, sub-folders too: a macro is active "
                               "only where its own filter and every folder above it allow."));
        else
            ImGui::TextWrapped(T("Hotkeys and the quick menu only use a macro where it's active. "
                               "Send now here in the library always works."));
        const std::string limits = FolderLimits(aNode);
        if (!limits.empty()) ImGui::TextWrapped(T("Also limited by the folders above it: %s"), limits.c_str());
        if (NeverActive(aNode))
            ImGui::TextColored(kWarnColor, "%s", folder ? T("Nothing in this folder can be active anywhere: its filter and the folders above it have no game mode or map in common.")
                                                        : T("This macro can't be active anywhere: its filter and its folders' have no game mode or map in common."));

        // Game modes. At least one stays ticked: "none" would mean the macro can never be used.
        if (aNode.GameModes == GM_None) aNode.GameModes = GM_All;
        UiFont::Row modesRow;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Game modes:"));
        const uint8_t bits[] = {GM_PvE, GM_WvW, GM_PvP};
        for (uint8_t b : bits)
        {
            modesRow.Next(UiFont::CheckboxWidth(ModeName(b)));
            bool on = (aNode.GameModes & b) != 0;
            if (ImGui::Checkbox(ModeName(b), &on))
            {
                const uint8_t next = on ? static_cast<uint8_t>(aNode.GameModes | b)
                                        : static_cast<uint8_t>(aNode.GameModes & ~b);
                if (next == GM_None)
                    ShowError(Fmt(T("Unticked %s under \"Where it's active\" for \"%s\""), ModeName(b), aNode.Name.c_str()),
                              folder ? T("A folder needs at least one game mode.") : T("A macro needs at least one game mode."));
                else
                {
                    aNode.GameModes = next;
                    MarkDirty();
                }
            }
            if (ImGui::IsItemHovered())
            {
                if (b == GM_WvW) ImGui::SetTooltip(T("Includes Armistice Bastion and Edge of the Mists."));
                else if (b == GM_PvE) ImGui::SetTooltip(T("Open world, story, fractals, raids and strikes."));
                else ImGui::SetTooltip(T("PvP maps, including the Heart of the Mists lobby."));
            }
        }

        // Maps: one button per map, flowing onto more rows when there are many.
        UiFont::Row mapsRow;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Maps:"));
        if (aNode.MapIds.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled(T("Any map"));
        }
        int removeAt = -1;
        for (size_t i = 0; i < aNode.MapIds.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            const uint32_t mapId = static_cast<uint32_t>(aNode.MapIds[i]);
            const std::string name = MapNames::Label(mapId);
            // The API's full list doesn't have it: removed or replaced in a game update, maybe.
            const bool unlisted = MapNames::Unlisted(mapId);
            const std::string label = name + (unlisted ? " (!)" : "") + " x";
            mapsRow.Next(UiFont::ButtonWidth(label.c_str()));
            if (unlisted) ImGui::PushStyleColor(ImGuiCol_Text, kWarnColor);
            if (ImGui::SmallButton(label.c_str())) removeAt = static_cast<int>(i);
            if (unlisted) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
            {
                if (unlisted)
                    ImGui::SetTooltip(T("Remove %s\nThe GW2 API's map list doesn't have this id: the map may have been "
                                      "removed or replaced in a game update (then this filter never matches), or "
                                      "it's an instance the API doesn't list."), name.c_str());
                else ImGui::SetTooltip(T("Remove %s"), name.c_str());
            }
            ImGui::PopID();
        }
        if (removeAt >= 0)
        {
            aNode.MapIds.erase(aNode.MapIds.begin() + removeAt);
            MarkDirty();
        }

        auto hasMap = [&](int aId) {
            for (int m : aNode.MapIds)
                if (m == aId) return true;
            return false;
        };
        auto addMap = [&](int aId) {
            const std::string action = Fmt(T("Added map id %d to \"%s\""), aId, aNode.Name.c_str());
            if (aId <= 0) ShowError(action, T("Map ids are positive numbers."));
            else if (hasMap(aId)) ShowError(action, T("That map is already in the list."));
            else if (aNode.MapIds.size() >= kMaxMapIds)
                ShowError(action, folder ? Fmt(T("A folder can list at most %zu maps."), kMaxMapIds)
                                         : Fmt(T("A macro can list at most %zu maps."), kMaxMapIds));
            else
            {
                aNode.MapIds.push_back(aId);
                MarkDirty();
            }
        };

        const int current = static_cast<int>(here.MapId);
        const bool canAddCurrent = current > 0 && !hasMap(current);
        if (!canAddCurrent) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        const std::string addCurrent = current > 0 ? Fmt(T("Add this map: %s"), MapNames::Label(here.MapId).c_str()) : std::string(T("Add this map"));
        UiFont::Row addRow;
        if (ImGui::SmallButton(addCurrent.c_str()) && canAddCurrent) addMap(current);
        if (!canAddCurrent) ImGui::PopStyleVar();
        addRow.Next(Px(90.0f));
        ImGui::SetNextItemWidth(Px(90.0f));
        ImGui::InputInt("##mapid", &s_MapIdInput, 0, 0);
        addRow.Next(UiFont::ButtonWidth(T("Add id")));
        if (ImGui::SmallButton(T("Add id"))) addMap(s_MapIdInput);
        DrawMapPicker(hasMap, addMap);
        ImGui::TextDisabled(T("Map names come from the GW2 API. Ids are also listed on the GW2 wiki."));

        const std::string where = here.MapId ? MapNames::Label(here.MapId) : std::string(T("no map"));
        if (active) ImGui::TextDisabled(T("Active here (%s, %s)."), ModeName(here.Mode), where.c_str());
        else ImGui::TextColored(kWarnColor, T("Not active here (%s, %s)."), ModeName(here.Mode), where.c_str());
        ImGui::Unindent();
    }

    // --- Preview: what Send now would type, without sending ---------------------------------------

    struct PreviewState
    {
        uint64_t                     MacroId = 0; // 0: no preview shown
        std::vector<Line>            Lines;       // the lines it was made from: an edit hides it
        std::vector<SendPlan::Typed> Typed;
        bool                         HasPlaceholders = false;
    };
    PreviewState s_Preview;

    bool SameLines(const std::vector<Line>& a, const std::vector<Line>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].Channel != b[i].Channel || a[i].Message != b[i].Message || a[i].WhisperTo != b[i].WhisperTo ||
                a[i].SquadBroadcast != b[i].SquadBroadcast || a[i].UnknownChannel != b[i].UnknownChannel) return false;
        return true;
    }

    void MakePreview(const Node& aMacro)
    {
        s_Preview = {};
        const SendPlan::Plan plan = Sender::Preview(aMacro.Lines);
        if (!plan.Error.empty())
        {
            ShowError(Fmt(T("Clicked Preview for \"%s\" (nothing was sent)"), aMacro.Name.c_str()), plan.Error);
            return;
        }
        s_Preview.MacroId = aMacro.Id;
        s_Preview.Lines   = aMacro.Lines;
        s_Preview.Typed   = SendPlan::Describe(plan, aMacro.Lines);
        for (const Line& l : aMacro.Lines)
            if (Placeholders::HasPlaceholders(l.Message) || Placeholders::HasPlaceholders(l.WhisperTo))
                s_Preview.HasPlaceholders = true;
    }

    // Under Send now while it matches the macro's current lines; gone after an edit or Close.
    void DrawPreview(const Node& aMacro)
    {
        if (s_Preview.MacroId != aMacro.Id) return;
        if (!SameLines(s_Preview.Lines, aMacro.Lines))
        {
            s_Preview = {};
            return;
        }
        ImGui::BeginGroup();
        ImGui::TextDisabled(T("Preview: what Send now would type (nothing was sent)"));
        ImGui::Indent();
        for (const SendPlan::Typed& t : s_Preview.Typed)
        {
            const ImVec4 colour = ChannelColorVec(t.Channel);
            const std::string label = t.Broadcast ? std::string(T("Squad broadcast"))
                                                  : std::string(Localization::Lookup(ChannelLabel(t.Channel)));
            ImGui::TextDisabled(T("Line %zu, %s:"), t.Line + 1, label.c_str());
            ImGui::Indent();
            ImGui::PushStyleColor(ImGuiCol_Text, colour);
            ImGui::TextWrapped("%s", t.Text.c_str());
            if (t.Tab)
            {
                ImGui::PopStyleColor();
                ImGui::TextDisabled("%s", T("then Tab, then:"));
                ImGui::PushStyleColor(ImGuiCol_Text, colour);
                ImGui::TextWrapped("%s", t.AfterTab.c_str());
            }
            ImGui::PopStyleColor();
            ImGui::Unindent();
        }
        if (s_Preview.HasPlaceholders)
            ImGui::TextDisabled(T("Placeholders are filled in again when it's sent ({random} and the time can differ)."));
        ImGui::Unindent();
        if (ImGui::SmallButton(T("Close preview"))) s_Preview = {};
        ImGui::EndGroup();
    }

    void DrawSendStatus()
    {
        const Sender::Status st = Sender::GetStatus();
        if (st.State == Sender::EState::Sending)
        {
            if (st.Line == 0) ImGui::TextColored(kWarnColor, T("Preparing...")); // filling in placeholders
            else ImGui::TextColored(kWarnColor, T("Sending line %zu of %zu..."), st.Line, st.Lines);
        }
        else if (!s_SendNote.empty() && ImGui::GetTime() < s_SendNoteUntil)
        {
            ImGui::TextDisabled("%s", s_SendNote.c_str());
        }
    }

    Node* NodeOrRoot(uint64_t aId)
    {
        if (aId == 0) return &s_Tree.Root();
        return s_Tree.Find(aId);
    }

    // The folder new items go into: the selected folder, the selected macro's folder, or the top level.
    Node& InsertFolder()
    {
        Node* sel = s_Tree.Find(s_SelectedId);
        if (!sel) return s_Tree.Root();
        return sel->IsFolder() ? *sel : *sel->Parent;
    }

    std::string PathOf(const Node& aNode)
    {
        std::vector<const Node*> chain;
        for (const Node* n = aNode.Parent; n && n->Parent; n = n->Parent) chain.push_back(n);
        std::string path = T("Top level");
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) path += " > " + (*it)->Name;
        return path;
    }

    // --- Export -----------------------------------------------------------------------------------
    // Writes to <addon>\exports\ (never overwriting a file) or copies to the clipboard. Doesn't
    // change the tree, so it's safe from the tree's context menu.

    std::string TrashNotExportedText(); // with the trash bin below

    // " Items in the trash bin ... aren't exported." when the bin holds anything.
    std::string WithTrashNote(const std::string& aText)
    {
        const std::string note = TrashNotExportedText();
        return note.empty() ? aText : aText + " " + note;
    }

    // The export's text. False (logged) if it couldn't be made: an exception must never leave
    // the render callback.
    bool MakeExportText(const std::vector<const Node*>& aNodes, bool aAsText, std::string& aText, std::string& aError)
    {
        aError = T("The export couldn't be written. Check the Nexus log.");
        // As text, a newer version's channel would become the current channel.
        if (aAsText && MacroTree::HasUnknownChannel(*aNodes.front()))
        {
            aError = T("A line's channel is from a newer version of the addon, which a .txt file can't hold. Export it as .json.");
            return false;
        }
        try
        {
            aText = aAsText ? Exchange::ExportText(*aNodes.front()) : Exchange::ExportJsonText(aNodes, s_Tree.Format());
            return true;
        }
        catch (const std::exception& e)
        {
            LogF(LOGL_WARNING, "Could not make the export: %s", e.what());
            return false;
        }
    }

    void ExportToFile(const std::vector<const Node*>& aNodes, const std::string& aName, bool aAsText)
    {
        const std::filesystem::path dir = Storage::ExportsDir();
        if (dir.empty() || aNodes.empty() || !aNodes.front())
        {
            ShowError(Fmt(T("Exported \"%s\""), aName.c_str()), T("The exports folder isn't available."));
            return;
        }
        std::string text, error;
        const bool made = MakeExportText(aNodes, aAsText, text, error);
        if (!made)
        {
            ShowError(Fmt(T("Exported \"%s\" as a %s file"), aName.c_str(), aAsText ? ".txt" : ".json"), error);
            return;
        }
        const std::filesystem::path file = Storage::FreeFile(dir, Exchange::FileNameFor(aName, aAsText ? ".txt" : ".json"));
        if (Storage::WriteText(file, text))
            SetInfo(WithTrashNote(Fmt(T("Exported to %s."), ("<GW2>\\addons\\GW2_Chat_Messages\\exports\\" + PathText::Utf8(file.filename())).c_str())), true);
        else ShowError(Fmt(T("Exported \"%s\" as a %s file"), aName.c_str(), aAsText ? ".txt" : ".json"),
                       T("The export couldn't be written. Check the Nexus log."));
    }

    void ExportToClipboard(const std::vector<const Node*>& aNodes, bool aAsText)
    {
        if (aNodes.empty() || !aNodes.front()) return;
        std::string text, error;
        if (!MakeExportText(aNodes, aAsText, text, error))
        {
            ShowError(Fmt(T("Exported \"%s\""), aNodes.front()->Name.c_str()), error);
            return;
        }
        ImGui::SetClipboardText(text.c_str());
        SetInfo(aAsText ? std::string(T("Copied the macro's lines to the clipboard."))
                        : WithTrashNote(T("Copied to the clipboard. Paste it into the Import window to import it.")));
    }

    std::vector<const Node*> TopLevelNodes()
    {
        std::vector<const Node*> nodes;
        for (const auto& c : s_Tree.Root().Children) nodes.push_back(c.get());
        return nodes;
    }

    // The Import window (defined further down).
    bool     s_ShowImport   = false;
    uint64_t s_ImportTarget = 0; // folder to import into (0 = top level)

    // The Export window (defined further down): the folders and macros ticked to export.
    bool            s_ShowExport = false;
    Exchange::Pick  s_ExportPick;
    std::string     s_ExportName;
    std::string     s_ExportDone; // the last "Exported to ..." line, shown in the window

    // Opens the Export window with aTick (a folder or macro, or nothing) ticked.
    void OpenExport(const Node* aTick)
    {
        s_ExportPick = {};
        if (aTick && aTick->Parent) Exchange::SetPicked(s_ExportPick, *aTick, true);
        s_ExportName = "Chat Messages selection";
        s_ExportDone.clear();
        s_ShowExport = true;
    }

    // Window sizes that follow the text size (UiFont).
    UiFont::WindowFit s_MainFit;
    UiFont::WindowFit s_ImportFit;
    UiFont::WindowFit s_ExportFit;

    void OpenNamePopup(ENameMode aMode, uint64_t aTargetId)
    {
        s_NameMode     = aMode;
        s_NameTargetId = aTargetId;
        s_NameFocus    = true;
        const Node* target = NodeOrRoot(aTargetId);
        const char* initial = aMode == ENameMode::Rename && target ? target->Name.c_str()
                            : aMode == ENameMode::NewFolder        ? T("New folder")
                                                                   : T("New macro");
        std::snprintf(s_NameBuf, sizeof(s_NameBuf), "%s", initial);
        // A folder's game modes can be ticked here too (its maps are in its details).
        s_NameModes = aMode == ENameMode::Rename && target && target->IsFolder() && target->GameModes != GM_None
                          ? target->GameModes : static_cast<uint8_t>(GM_All);
        ImGui::OpenPopup(kNamePopup);
    }

    void SetAllExpanded(Node& aNode, bool aOpen)
    {
        for (auto& c : aNode.Children)
            if (c->IsFolder()) { c->Expanded = aOpen; SetAllExpanded(*c, aOpen); }
    }

    // --- Tree ---------------------------------------------------------------------------------

    // Drop zones on a row: folders take the middle half as "into", macros split top/bottom.
    EDrop DropZone(const Node& aNode)
    {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float  t   = (ImGui::GetMousePos().y - min.y) / (max.y - min.y > 0 ? max.y - min.y : 1.0f);
        if (aNode.IsFolder())
        {
            if (t < 0.25f) return EDrop::Before;
            if (t > 0.75f && !aNode.Expanded) return EDrop::After;
            return EDrop::Into;
        }
        return t < 0.5f ? EDrop::Before : EDrop::After;
    }

    void DrawDropIndicator(EDrop aDrop)
    {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList*  dl  = ImGui::GetWindowDrawList();
        const ImU32  col = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
        if (aDrop == EDrop::Into) dl->AddRect(min, max, col, 2.0f, ImDrawCornerFlags_All, 2.0f);
        else
        {
            const float y = aDrop == EDrop::Before ? min.y : max.y;
            dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), col, 2.0f);
        }
    }

    void HandleDropTarget(Node& aNode)
    {
        if (!ImGui::BeginDragDropTarget()) return;
        const ImGuiPayload* peek = ImGui::GetDragDropPayload();
        if (peek && peek->IsDataType(kDragPayload) && peek->DataSize == sizeof(uint64_t))
        {
            uint64_t draggedId;
            std::memcpy(&draggedId, peek->Data, sizeof(draggedId));
            Node* dragged = s_Tree.Find(draggedId);
            const EDrop drop = DropZone(aNode);
            Result can = dragged ? s_Tree.CanMove(*dragged, aNode, drop) : Result::Fail("");
            if (can.Ok && dragged != &aNode)
            {
                const ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kDragPayload, flags))
                {
                    DrawDropIndicator(drop);
                    if (p->IsDelivery())
                    {
                        s_Pending.Type   = EAction::Move;
                        s_Pending.Id     = draggedId;
                        s_Pending.Target = aNode.Id;
                        s_Pending.Drop   = drop;
                    }
                }
            }
            else if (dragged != &aNode)
            {
                s_DragError = can.Error;
            }
        }
        ImGui::EndDragDropTarget();
    }

    void DrawContextMenu(Node& aNode)
    {
        if (!ImGui::BeginPopupContextItem("##ctx")) return;
        s_SelectedId = aNode.Id;
        if (aNode.IsFolder())
        {
            const bool canNest = Tree::FolderLevel(aNode) < kMaxFolderDepth;
            if (ImGui::MenuItem(T("New folder inside"), nullptr, false, canNest))
                s_Pending = {EAction::OpenName, 0, aNode.Id, EDrop::Into, 0, ENameMode::NewFolder};
            if (ImGui::MenuItem(T("New macro inside")))
                s_Pending = {EAction::OpenName, 0, aNode.Id, EDrop::Into, 0, ENameMode::NewMacro};
        }
        else
        {
            if (ImGui::MenuItem(T("Send now"), nullptr, false, !Sender::IsBusy())) SendMacro(aNode); // doesn't change the tree
            if (ImGui::MenuItem(T("Duplicate"))) s_Pending = {EAction::Duplicate, aNode.Id};
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(T("Makes a copy right below it, with the same lines, filters and note.\n"
                                  "The copy has no hotkey, isn't a favourite and isn't linked to a file."));
        }
        if (ImGui::MenuItem(T("Favourite"), nullptr, aNode.Favourite)) s_Pending = {EAction::Favourite, aNode.Id};
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Favourites are listed at the top of the quick menu, on every map."));
        ImGui::Separator();
        if (ImGui::MenuItem(T("Rename")))
            s_Pending = {EAction::OpenName, 0, aNode.Id, EDrop::Into, 0, ENameMode::Rename};
        // Off while searching: the neighbours it would move past may be hidden.
        if (ImGui::MenuItem(T("Move up"), nullptr, false, !s_Search.Active))   s_Pending = {EAction::Shift, aNode.Id, 0, EDrop::Into, -1};
        if (ImGui::MenuItem(T("Move down"), nullptr, false, !s_Search.Active)) s_Pending = {EAction::Shift, aNode.Id, 0, EDrop::Into, +1};
        if (ImGui::BeginMenu(T("Export")))
        {
            const std::vector<const Node*> one{&aNode};
            if (ImGui::MenuItem(T("Save as a .json file"))) ExportToFile(one, aNode.Name, false);
            if (aNode.IsMacro() && ImGui::MenuItem(T("Save as a .txt file"))) ExportToFile(one, aNode.Name, true);
            if (ImGui::MenuItem(T("Copy to the clipboard"))) ExportToClipboard(one, false);
            if (aNode.IsMacro() && ImGui::MenuItem(T("Copy the lines as text"))) ExportToClipboard(one, true);
            ImGui::Separator();
            if (ImGui::MenuItem(T("Choose more to export..."))) OpenExport(&aNode); // opens a window; the tree is untouched
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Opens the Export window with this ticked, to add other folders and macros to the same file."));
            const std::string note = TrashNotExportedText();
            if (!note.empty())
            {
                ImGui::Separator();
                ImGui::TextColored(kWarnColor, "%s", note.c_str());
            }
            ImGui::EndMenu();
        }
        if (aNode.IsFolder() && ImGui::MenuItem(T("Import into this folder...")))
        {
            s_ImportTarget = aNode.Id;
            s_ShowImport   = true;
        }
        if (aNode.IsFolder())
        {
            if (ImGui::MenuItem(T("Sort contents A-Z"))) s_Pending = {EAction::Sort, aNode.Id};
            if (ImGui::MenuItem(T("Expand all inside"))) s_Pending = {EAction::ExpandAll, aNode.Id, 0, EDrop::Into, 0, ENameMode::NewFolder, true};
            if (ImGui::MenuItem(T("Collapse all inside"))) s_Pending = {EAction::ExpandAll, aNode.Id, 0, EDrop::Into, 0, ENameMode::NewFolder, false};
        }
        ImGui::Separator();
        if (ImGui::MenuItem(s_TrashOn ? T("Delete (to trash bin)") : T("Delete..."))) s_Pending = {EAction::OpenDelete, aNode.Id};
        ImGui::EndPopup();
    }

    // While searching: marks where the search text is in a matching node's name, or, for a
    // macro found by its lines or hotkey, says so after the name.
    std::string SearchSuffix(const Node& aNode)
    {
        if (!s_Search.Active || !s_Search.Matches.count(aNode.Id)) return {};
        size_t at = 0, len = 0;
        if (Search::FindRange(aNode.Name, s_SearchBuf, at, len)) return {};
        if (!aNode.Hotkey.Empty() && Search::FindRange(Hotkey::Display(aNode.Hotkey), s_SearchBuf, at, len))
            return std::string("  ") + T("(found in its hotkey)");
        bool inLines = false;
        for (const Line& l : aNode.Lines)
            if (Search::FindRange(l.Message, s_SearchBuf, at, len) || Search::FindRange(l.WhisperTo, s_SearchBuf, at, len))
            {
                inLines = true;
                break;
            }
        if (!inLines && Search::FindRange(aNode.Note, s_SearchBuf, at, len))
            return std::string("  ") + T("(found in its note)");
        return std::string("  ") + T("(found in its lines)");
    }

    void HighlightSearch(const Node& aNode, const RowLabel& aLabel)
    {
        if (!aLabel.Highlight || !ImGui::IsItemVisible()) return;
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const char* name = aNode.Name.c_str();
        const size_t at  = std::min(aLabel.At, aNode.Name.size());
        const size_t end = std::min(aLabel.At + aLabel.Len, aNode.Name.size());
        const float x0 = min.x + ImGui::GetTreeNodeToLabelSpacing() + ImGui::CalcTextSize(name, name + at).x;
        const float x1 = x0 + ImGui::CalcTextSize(name + at, name + end).x;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x0, min.y), ImVec2(x1, max.y), kSearchHighlight, 2.0f);
    }

    // The row's text: its name, the hotkey (macros), what the search found, the folder's own
    // filter and the favourite mark.
    const RowLabel& LabelOf(const Node& aNode)
    {
        const auto hit = s_RowLabels.find(aNode.Id);
        if (hit != s_RowLabels.end()) return hit->second;
        RowLabel l;
        std::string suffix = SearchSuffix(aNode);
        suffix += FolderTag(aNode); // "[WvW, 2 maps]": the folder's own filter
        if (aNode.Favourite) suffix += "  *"; // a favourite (the quick menu's top)
        if (aNode.IsFolder() || aNode.Hotkey.Empty()) l.Text = aNode.Name + suffix;
        else if (Hotkey::NeedsModifier(aNode.Hotkey))
            l.Text = Fmt(T("%s  [%s: off, needs Ctrl/Alt/Shift]%s"), aNode.Name.c_str(), Hotkey::Display(aNode.Hotkey).c_str(), suffix.c_str());
        else l.Text = aNode.Name + "  [" + Hotkey::Display(aNode.Hotkey) + "]" + suffix;
        if (s_Search.Active && s_Search.Matches.count(aNode.Id))
            l.Highlight = Search::FindRange(aNode.Name, s_SearchBuf, l.At, l.Len);
        return s_RowLabels.emplace(aNode.Id, std::move(l)).first->second;
    }

    void DrawNode(Node& aNode)
    {
        if (!s_Search.Shows(aNode.Id)) return;
        ImGui::PushID(static_cast<int>(aNode.Id ^ (aNode.Id >> 32)));
        ImGui::PushID(static_cast<int>(aNode.Id & 0xFFFFFFFF));

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (aNode.Id == s_SelectedId) flags |= ImGuiTreeNodeFlags_Selected;
        const RowLabel& label = LabelOf(aNode);

        bool open = false;
        if (aNode.IsFolder())
        {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
            if (aNode.Children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
            // A folder with a match inside is held open while searching; its own open state isn't
            // changed, so clearing the search brings the tree back as it was.
            const bool forced = s_Search.ForcedOpen(aNode.Id);
            ImGui::SetNextItemOpen(forced || aNode.Expanded, ImGuiCond_Always);
            ImVec4 folderColor = kFolderColor;
            if (!IsActiveHere(aNode, WhereAmI())) folderColor.w *= 0.45f; // its filter (or a folder above) is off here: dimmed
            ImGui::PushStyleColor(ImGuiCol_Text, folderColor);
            open = ImGui::TreeNodeEx("##node", flags, "%s", label.Text.c_str());
            ImGui::PopStyleColor();
            if (!forced && open != aNode.Expanded && !aNode.Children.empty())
            {
                aNode.Expanded = open;
                MarkDirty();
            }
        }
        else
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet;
            // Macros take their first line's channel colour, like in bhm-chat-macros.
            const EChannel first = aNode.Lines.empty() ? EChannel::Current : aNode.Lines.front().Channel;
            ImVec4 color = ChannelColorVec(first);
            // Filtered out here, or uses an advanced option that's off: dimmed.
            if (!IsActiveHere(aNode, WhereAmI()) || IsBlocked(aNode)) color.w *= 0.45f;
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TreeNodeEx("##node", flags, "%s", label.Text.c_str());
            ImGui::PopStyleColor();
        }
        HighlightSearch(aNode, label);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) s_SelectedId = aNode.Id;

        // While searching the tree isn't complete, so nothing can be dragged or dropped.
        if (!s_Search.Active && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kDragPayload, &aNode.Id, sizeof(aNode.Id));
            UiFont::PushTooltipWrap();
            ImGui::Text("%s %s", aNode.IsFolder() ? T("Folder:") : T("Macro:"), aNode.Name.c_str());
            if (!s_DragError.empty()) ImGui::TextColored(kErrorColor, "%s", s_DragError.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndDragDropSource();
        }
        if (!s_Search.Active) HandleDropTarget(aNode);
        DrawContextMenu(aNode);

        if (aNode.IsFolder() && open)
        {
            for (auto& child : aNode.Children) DrawNode(*child);
            ImGui::TreePop();
        }
        ImGui::PopID();
        ImGui::PopID();
    }

    // --- Trash bin -------------------------------------------------------------------------------
    // Shown under the tree while it's turned on, or while it still holds items. Drawing it never
    // changes the tree: restore and delete go through s_Pending.

    std::string TrashItemLabel(const TrashItem& aItem)
    {
        if (!aItem.Item->IsFolder()) return aItem.Item->Name;
        const size_t inside = Tree::CountDescendants(*aItem.Item);
        return aItem.Item->Name + " (" + (inside ? Fmt(T("folder, %zu inside"), inside) : std::string(T("folder"))) + ")";
    }

    // Folders and macros in the bin, counting what's inside deleted folders.
    void TrashCounts(size_t& aFolders, size_t& aMacros)
    {
        aFolders = aMacros = 0;
        for (const TrashItem& t : s_Tree.Trash())
        {
            if (t.Item->IsFolder()) aFolders++;
            else aMacros++;
            Tree::CountDescendants(*t.Item, &aFolders, &aMacros);
        }
    }

    // Added to export messages and menus while the bin holds anything.
    std::string TrashNotExportedText()
    {
        if (s_Tree.Trash().empty()) return {};
        size_t f = 0, m = 0;
        TrashCounts(f, m);
        return Fmt(T("Items in the trash bin (%zu/%zu) aren't exported."), f, m);
    }

    void DrawTrashBin()
    {
        const auto& trash = s_Tree.Trash();
        if (!s_TrashOn && trash.empty()) return;
        ImGui::Spacing();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (trash.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
        ImGui::SetNextItemOpen(s_TrashOpen && !trash.empty(), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_Text, kTrashColor);
        size_t binFolders = 0, binMacros = 0;
        TrashCounts(binFolders, binMacros);
        const bool open = ImGui::TreeNodeEx("##trashbin", flags, T("Trash bin (%zu/%zu)"), binFolders, binMacros);
        ImGui::PopStyleColor();
        if (!trash.empty()) s_TrashOpen = open;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("%zu folder(s) / %zu macro(s) in the bin, counting everything inside deleted folders.\n"
                              "Right-click an item to restore it, or right-click the bin to empty it."), binFolders, binMacros);
        if (s_TrashOn && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kDragPayload))
                if (p->DataSize == sizeof(uint64_t))
                {
                    uint64_t id;
                    std::memcpy(&id, p->Data, sizeof(id));
                    s_Pending = {EAction::Trash, id};
                }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##trashctx"))
        {
            if (ImGui::MenuItem(T("Empty trash bin..."), nullptr, false, !trash.empty())) s_Pending = {EAction::OpenEmpty};
            ImGui::EndPopup();
        }
        if (!open) return;

        const bool showSel = !s_Tree.Find(s_SelectedId);
        for (const TrashItem& t : trash)
        {
            const uint64_t id = t.Item->Id;
            ImGui::PushID(static_cast<int>(id ^ (id >> 32)));
            ImGui::PushID(static_cast<int>(id & 0xFFFFFFFF));
            ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf |
                                   ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet;
            if (showSel && id == s_TrashSelId) f |= ImGuiTreeNodeFlags_Selected;
            ImGui::PushStyleColor(ImGuiCol_Text, kTrashColor);
            ImGui::TreeNodeEx("##trashitem", f, "%s", TrashItemLabel(t).c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemClicked()) { s_TrashSelId = id; s_SelectedId = 0; }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(T("Deleted %s from %s"), DeletedText(t).c_str(), Tree::FromPathText(t).c_str());
            if (ImGui::BeginPopupContextItem("##trashitemctx"))
            {
                s_TrashSelId = id;
                s_SelectedId = 0;
                if (ImGui::MenuItem(T("Restore"))) s_Pending = {EAction::OpenRestore, id};
                if (ImGui::MenuItem(T("Delete permanently..."))) s_Pending = {EAction::OpenPurge, id};
                ImGui::EndPopup();
            }
            ImGui::PopID();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    // Moves a tree item to the bin, or opens the old "can't be undone" pop-up when the bin is off.
    void DeleteItem(Node& aNode)
    {
        if (!s_TrashOn)
        {
            s_DeleteId = aNode.Id;
            ImGui::OpenPopup(kDeletePopup);
            return;
        }
        if (Node* sel = s_Tree.Find(s_SelectedId))
            if (sel == &aNode || Tree::IsAncestorOf(aNode, *sel)) s_SelectedId = 0;
        const std::string name = aNode.Name;
        const uint64_t id = aNode.Id;
        if (s_Tree.MoveToTrash(aNode, MacroTree::UtcStamp(std::time(nullptr))).Ok)
        {
            if (s_SelectedId == 0) s_TrashSelId = id;
            MarkDirty();
            SetInfo(Fmt(T("Moved \"%s\" to the trash bin. Right-click it there to restore it."), name.c_str()));
        }
    }

    void RestoreItem(uint64_t aId, ERestoreTo aTo)
    {
        const int idx = s_Tree.TrashIndexOf(aId);
        if (idx < 0) return;
        const std::string name = s_Tree.Trash()[static_cast<size_t>(idx)].Item->Name;
        Result r = s_Tree.Restore(static_cast<size_t>(idx), aTo);
        if (!r.Ok)
        {
            const std::string action = aTo == ERestoreTo::TopLevel ? Fmt(T("Restored \"%s\" from the trash bin to the top level"), name.c_str())
                                     : aTo == ERestoreTo::RecreateFolders ? Fmt(T("Restored \"%s\" from the trash bin (recreating its folders)"), name.c_str())
                                                                          : Fmt(T("Restored \"%s\" from the trash bin"), name.c_str());
            ShowError(action, r.Error);
            return;
        }
        s_SelectedId = r.Target ? r.Target->Id : 0;
        s_TrashSelId = 0;
        MarkDirty();
        SetInfo(r.Note.empty() ? Fmt(T("Restored \"%s\" to %s."), name.c_str(), (r.Target ? PathOf(*r.Target) : std::string(T("the library"))).c_str()) : r.Note);
    }

    void DrawTrashDetails(const TrashItem& aItem)
    {
        const Node& n = *aItem.Item;
        ImGui::TextDisabled("%s", n.IsFolder() ? T("Folder in the trash bin") : T("Macro in the trash bin"));
        ImGui::TextWrapped("%s", n.Name.c_str());
        ImGui::TextDisabled(T("Deleted %s from %s"), DeletedText(aItem).c_str(), Tree::FromPathText(aItem).c_str());
        const int idx = s_Tree.TrashIndexOf(n.Id);
        if (idx >= 0 && !s_Tree.OriginalFolderExists(static_cast<size_t>(idx)))
            ImGui::TextColored(kWarnColor, T("That folder no longer exists. Restoring asks where to put it."));
        UiFont::Row row;
        if (ImGui::SmallButton(T("Restore"))) s_Pending = {EAction::OpenRestore, n.Id};
        row.Next(UiFont::ButtonWidth(T("Delete permanently...")));
        if (ImGui::SmallButton(T("Delete permanently..."))) s_Pending = {EAction::OpenPurge, n.Id};
        ImGui::Separator();
        ImGui::TextWrapped(T("Items in the trash bin can't be sent, edited or used by a hotkey. Restore it first."));
        ImGui::Spacing();
        if (n.IsFolder())
        {
            size_t folders = 0, macros = 0;
            Tree::CountDescendants(n, &folders, &macros);
            ImGui::Text(T("Contains %zu folder(s) and %zu macro(s)"), folders, macros);
        }
        else
        {
            if (n.Lines.empty()) ImGui::TextDisabled(T("No lines."));
            for (const Line& l : n.Lines) DrawLinePreview(l);
            if (!n.Hotkey.Empty()) ImGui::TextDisabled(T("Hotkey (not active in the bin): %s"), Hotkey::Display(n.Hotkey).c_str());
        }
    }

    void DrawRestorePopup()
    {
        ImGui::SetNextWindowSize(ImVec2(Px(420.0f), 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(T(kRestorePopup), nullptr, ImGuiWindowFlags_NoResize)) return;
        const int idx = s_Tree.TrashIndexOf(s_RestoreId);
        if (idx < 0)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        const TrashItem& t = s_Tree.Trash()[static_cast<size_t>(idx)];
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextWrapped(T("\"%s\" came from \"%s\", which no longer exists."), t.Item->Name.c_str(), Tree::FromPathText(t).c_str());
        ImGui::TextWrapped(T("Where should it go?"));
        ImGui::Spacing();
        // Restoring happens after the pop-up closes, so an error can be shown in the error window.
        UiFont::Row row;
        if (ImGui::Button(T("Recreate the folders")))
        {
            s_Pending = {EAction::RestoreNow, s_RestoreId, 0, EDrop::Into, static_cast<int>(ERestoreTo::RecreateFolders)};
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Makes the missing folders again (by name) and puts it back inside."));
        row.Next(UiFont::ButtonWidth(T("Put it at the top level")));
        if (ImGui::Button(T("Put it at the top level")))
        {
            s_Pending = {EAction::RestoreNow, s_RestoreId, 0, EDrop::Into, static_cast<int>(ERestoreTo::TopLevel)};
            ImGui::CloseCurrentPopup();
        }
        row.Next(UiFont::ButtonWidth(T("Cancel")));
        if (ImGui::Button(T("Cancel"))) ImGui::CloseCurrentPopup();
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    void DrawPurgePopup()
    {
        ImGui::SetNextWindowSize(ImVec2(Px(400.0f), 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(T(kPurgePopup), nullptr, ImGuiWindowFlags_NoResize)) return;
        const int idx = s_PurgeAll ? -1 : s_Tree.TrashIndexOf(s_PurgeId);
        if ((s_PurgeAll && s_Tree.Trash().empty()) || (!s_PurgeAll && idx < 0))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::PushTextWrapPos(0.0f);
        if (s_PurgeAll)
            ImGui::TextWrapped(T("Empty the trash bin? Its %zu item(s) are deleted for good."), s_Tree.Trash().size());
        else
        {
            const TrashItem& t = s_Tree.Trash()[static_cast<size_t>(idx)];
            ImGui::TextWrapped(T("Delete \"%s\" permanently?"), t.Item->Name.c_str());
            if (t.Item->IsFolder() && !t.Item->Children.empty())
                ImGui::TextColored(kWarnColor, T("Everything inside it is deleted too: %zu item(s)."), Tree::CountDescendants(*t.Item));
        }
        ImGui::TextDisabled(T("This can't be undone."));
        if (ImGui::Button(s_PurgeAll ? T("Empty") : T("Delete"), Px(100.0f, 0.0f)))
        {
            if (s_PurgeAll) s_Tree.EmptyTrash();
            else s_Tree.DeleteFromTrash(static_cast<size_t>(idx));
            if (s_Tree.TrashIndexOf(s_TrashSelId) < 0) s_TrashSelId = 0;
            MarkDirty();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel"), Px(100.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    void DrawTree()
    {
        s_DragError.clear();
        for (auto& child : s_Tree.Root().Children) DrawNode(*child);
        if (!s_Search.Active) DrawTrashBin(); // the bin isn't searched: its items can't be used

        // The empty space below the tree: drop here to move to the end of the top level,
        // click here to clear the selection.
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.y < Px(24.0f)) avail.y = Px(24.0f);
        ImGui::InvisibleButton("##tree_end", ImVec2(avail.x > 1.0f ? avail.x : 1.0f, avail.y));
        if (ImGui::IsItemClicked()) { s_SelectedId = 0; s_TrashSelId = 0; }
        if (!s_Search.Active && ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* peek = ImGui::GetDragDropPayload();
            if (peek && peek->IsDataType(kDragPayload) && peek->DataSize == sizeof(uint64_t))
            {
                uint64_t draggedId;
                std::memcpy(&draggedId, peek->Data, sizeof(draggedId));
                Node* dragged = s_Tree.Find(draggedId);
                Result can = dragged ? s_Tree.CanMove(*dragged, s_Tree.Root(), EDrop::Into) : Result::Fail("");
                if (can.Ok)
                {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kDragPayload, ImGuiDragDropFlags_AcceptBeforeDelivery))
                        if (p->IsDelivery()) s_Pending = {EAction::Move, draggedId, 0, EDrop::Into};
                }
                else s_DragError = can.Error;
            }
            ImGui::EndDragDropTarget();
        }
    }

    // --- Pending actions and popups -----------------------------------------------------------

    void ApplyPending()
    {
        const Action a = s_Pending;
        s_Pending = {};
        switch (a.Type)
        {
        case EAction::None:
            break;
        case EAction::Move:
        {
            Node* node   = s_Tree.Find(a.Id);
            Node* target = NodeOrRoot(a.Target);
            if (!node || !target) break;
            Result r = s_Tree.Move(*node, *target, a.Drop);
            if (r.Ok) { s_SelectedId = a.Id; MarkDirty(); }
            else ShowError(Fmt(T("Moved \"%s\" by dragging it"), node->Name.c_str()), r.Error);
            break;
        }
        case EAction::Shift:
            if (Node* node = s_Tree.Find(a.Id))
                if (s_Tree.Shift(*node, a.Delta)) MarkDirty();
            break;
        case EAction::Sort:
            if (Node* node = s_Tree.Find(a.Id)) { s_Tree.SortChildren(*node, false); MarkDirty(); }
            break;
        case EAction::ExpandAll:
            if (Node* node = NodeOrRoot(a.Id))
            {
                if (a.Id != 0) node->Expanded = a.Flag;
                SetAllExpanded(*node, a.Flag);
                MarkDirty();
            }
            break;
        case EAction::OpenName:
            OpenNamePopup(a.Name, a.Target);
            break;
        case EAction::OpenDelete:
        case EAction::Trash:
            if (Node* node = s_Tree.Find(a.Id))
            {
                if (a.Type == EAction::Trash && !s_TrashOn) break;
                DeleteItem(*node);
            }
            break;
        case EAction::OpenRestore:
        {
            const int idx = s_Tree.TrashIndexOf(a.Id);
            if (idx < 0) break;
            if (s_Tree.OriginalFolderExists(static_cast<size_t>(idx))) RestoreItem(a.Id, ERestoreTo::Original);
            else
            {
                s_RestoreId = a.Id;
                ImGui::OpenPopup(kRestorePopup);
            }
            break;
        }
        case EAction::RestoreNow:
            RestoreItem(a.Id, static_cast<ERestoreTo>(a.Delta));
            break;
        case EAction::OpenPurge:
        case EAction::OpenEmpty:
            s_PurgeAll = a.Type == EAction::OpenEmpty;
            s_PurgeId  = a.Id;
            ImGui::OpenPopup(kPurgePopup);
            break;
        case EAction::Favourite:
            if (Node* node = s_Tree.Find(a.Id))
                SetFavourite(*node, !node->Favourite, T("Clicked Favourite (right-click menu)"));
            break;
        case EAction::Duplicate:
            if (Node* node = s_Tree.Find(a.Id))
            {
                Result r = s_Tree.Duplicate(*node);
                if (r.Ok && r.Target) { s_SelectedId = r.Target->Id; MarkDirty(); }
                else ShowError(Fmt(T("Clicked Duplicate on \"%s\""), node->Name.c_str()), r.Error);
            }
            break;
        }
    }

    void DrawNamePopup()
    {
        ImGui::SetNextWindowSize(ImVec2(Px(360.0f), 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(T(kNamePopup), nullptr, ImGuiWindowFlags_NoResize))
        {
            s_ErrorInPopup = false; // the pop-up is gone: any error left shows in the error window
            return;
        }
        ImGui::PushTextWrapPos(0.0f);

        const char* title = s_NameMode == ENameMode::NewFolder ? T("New folder name:")
                          : s_NameMode == ENameMode::NewMacro  ? T("New macro title:")
                                                               : T("New name:");
        ImGui::TextUnformatted(title);
        if (s_NameFocus) { ImGui::SetKeyboardFocusHere(); s_NameFocus = false; }
        ImGui::SetNextItemWidth(-1.0f);
        const bool enter = ImGui::InputText("##name", s_NameBuf, kMaxNameLength + 1,
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        // A folder: where it's active (game modes). At least one stays ticked.
        const Node* named = NodeOrRoot(s_NameTargetId);
        const bool forFolder = s_NameMode == ENameMode::NewFolder ||
                               (s_NameMode == ENameMode::Rename && named && named->IsFolder());
        if (forFolder)
        {
            UiFont::Row modesRow;
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(T("Active in:"));
            const uint8_t bits[] = {GM_PvE, GM_WvW, GM_PvP};
            for (uint8_t b : bits)
            {
                modesRow.Next(UiFont::CheckboxWidth(ModeName(b)));
                bool on = (s_NameModes & b) != 0;
                if (ImGui::Checkbox(ModeName(b), &on))
                {
                    const uint8_t next = on ? static_cast<uint8_t>(s_NameModes | b) : static_cast<uint8_t>(s_NameModes & ~b);
                    if (next != GM_None) s_NameModes = next; // the last one can't be unticked
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Everything in the folder is only active in the ticked game modes."));
            }
            ImGui::TextDisabled("%s", T("Maps: in the folder's details, under \"Where it's active\"."));
        }

        const bool ok = ImGui::Button(s_NameMode == ENameMode::Rename ? T("Rename") : T("Create"), Px(100.0f, 0.0f)) || enter;
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel"), Px(100.0f, 0.0f))) ImGui::CloseCurrentPopup();

        if (ok)
        {
            Node* target = NodeOrRoot(s_NameTargetId);
            Result r = !target ? Result::Fail(T("That item no longer exists."))
                     : s_NameMode == ENameMode::NewFolder ? s_Tree.AddFolder(target, s_NameBuf)
                     : s_NameMode == ENameMode::NewMacro  ? s_Tree.AddMacro(target, s_NameBuf)
                                                          : s_Tree.Rename(*target, s_NameBuf);
            if (r.Ok)
            {
                if (r.Target) s_SelectedId = r.Target->Id;
                if (forFolder)
                {
                    Node* folderNode = s_NameMode == ENameMode::NewFolder ? r.Target : target;
                    if (folderNode && folderNode->IsFolder()) folderNode->GameModes = s_NameModes;
                }
                MarkDirty();
                ImGui::CloseCurrentPopup();
            }
            else
            {
                const std::string where = target && target->Parent ? "\"" + target->Name + "\"" : std::string(T("the top level"));
                const std::string action = s_NameMode == ENameMode::NewFolder ? Fmt(T("Created a folder named \"%s\" in %s"), s_NameBuf, where.c_str())
                                         : s_NameMode == ENameMode::NewMacro  ? Fmt(T("Created a macro named \"%s\" in %s"), s_NameBuf, where.c_str())
                                         : Fmt(T("Renamed %s to \"%s\""), (target ? "\"" + target->Name + "\"" : std::string(T("an item"))).c_str(), s_NameBuf);
                ShowError(action, r.Error);
                s_ErrorInPopup = true; // shown inside this pop-up, which blocks other windows
            }
        }
        if (DrawErrorPopupInside()) s_NameFocus = true; // back to the name box to fix it
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    void DrawDeletePopup()
    {
        ImGui::SetNextWindowSize(ImVec2(Px(380.0f), 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(T(kDeletePopup), nullptr, ImGuiWindowFlags_NoResize)) return;

        Node* node = s_Tree.Find(s_DeleteId);
        if (!node)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::PushTextWrapPos(0.0f);
        if (node->IsFolder())
        {
            size_t folders = 0, macros = 0;
            Tree::CountDescendants(*node, &folders, &macros);
            ImGui::TextWrapped(T("Delete the folder \"%s\"?"), node->Name.c_str());
            if (folders + macros > 0)
                ImGui::TextColored(kWarnColor, T("Everything inside it is deleted too: %zu folder(s) and %zu macro(s)."), folders, macros);
        }
        else
        {
            ImGui::TextWrapped(T("Delete the macro \"%s\"?"), node->Name.c_str());
        }
        ImGui::TextDisabled(T("This can't be undone. (Turn on the trash bin in the addon's Nexus options to make deleting undoable.)"));

        if (ImGui::Button(T("Delete"), Px(100.0f, 0.0f)))
        {
            if (s_SelectedId == node->Id || (s_Tree.Find(s_SelectedId) && Tree::IsAncestorOf(*node, *s_Tree.Find(s_SelectedId))))
                s_SelectedId = 0;
            s_Tree.Delete(*node);
            MarkDirty();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel"), Px(100.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    // --- Example macros (the options page's button; docs/design.md, "Starter library") ---------
    // The button only opens the library and sets s_ExamplesRequest; the library window asks here,
    // at its own ID level: replace the Examples folder if there is one (never a merge, never a
    // copy beside it), add them again if they were added before, or just add them.

    void NoteExamplesAdded()
    {
        if (s_ExamplesAdded) return;
        s_ExamplesAdded = true;
        if (!Storage::SaveSetting(kExamplesSetting, true)) LogF(LOGL_WARNING, "The \"examples added\" setting couldn't be saved.");
    }

    void ShowNewExamples(Node& aFolder, const std::string& aInfo)
    {
        s_SelectedId = aFolder.Id;
        for (Node* p = aFolder.Parent; p; p = p->Parent) p->Expanded = true;
        MarkDirty();
        NoteExamplesAdded();
        SetInfo(aInfo);
    }

    void AddExamplesNow()
    {
        if (Node* ex = Examples::AddAgain(s_Tree)) ShowNewExamples(*ex, T("Added the example macros in a new Examples folder."));
        else ShowError(T("Clicked \"Add the example macros\""), T("The examples couldn't be added: there are too many folders named \"Examples\"."));
    }

    void HandleExamplesRequest()
    {
        if (!s_ExamplesRequest) return;
        s_ExamplesRequest = false;
        if (Examples::Find(s_Tree)) ImGui::OpenPopup(kExamplesReplacePopup);
        else if (s_ExamplesAdded) ImGui::OpenPopup(kExamplesAgainPopup);
        else AddExamplesNow();
    }

    void DrawExamplesReplacePopup()
    {
        ImGui::SetNextWindowSize(ImVec2(Px(420.0f), 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(T(kExamplesReplacePopup), nullptr, ImGuiWindowFlags_NoResize)) return;
        Node* old = Examples::Find(s_Tree);
        if (!old)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextWrapped(T("You already have the Examples folder (now called \"%s\"). Replace it with a fresh copy "
                           "of the example macros?"), old->Name.c_str());
        size_t folders = 0, macros = 0;
        Tree::CountDescendants(*old, &folders, &macros);
        if (s_TrashOn)
            ImGui::TextColored(kWarnColor, T("The old folder goes to the trash bin with everything in it (%zu folder(s) and "
                                           "%zu macro(s)), also any macros you added or changed there. You can restore it "
                                           "from the bin."), folders, macros);
        else
            ImGui::TextColored(kWarnColor, T("The old folder is deleted for good with everything in it (%zu folder(s) and "
                                           "%zu macro(s)), also any macros you added or changed there. This can't be "
                                           "undone."), folders, macros);
        ImGui::TextDisabled(T("The old and the new folder can't be merged."));

        if (ImGui::Button(T("Replace"), Px(100.0f, 0.0f)))
        {
            if (Node* sel = s_Tree.Find(s_SelectedId))
                if (sel == old || Tree::IsAncestorOf(*old, *sel)) s_SelectedId = 0;
            const bool toTrash = s_TrashOn;
            if (Node* fresh = Examples::Replace(s_Tree, *old, toTrash, MacroTree::UtcStamp(std::time(nullptr))))
                ShowNewExamples(*fresh, toTrash ? T("Replaced the Examples folder with a fresh copy. The old one is in the trash bin.")
                                                : T("Replaced the Examples folder with a fresh copy."));
            else
                ShowError(T("Clicked Replace for the example macros"), T("The Examples folder couldn't be replaced. Nothing was changed."));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel"), Px(100.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    void DrawExamplesAgainPopup()
    {
        ImGui::SetNextWindowSize(ImVec2(Px(420.0f), 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(T(kExamplesAgainPopup), nullptr, ImGuiWindowFlags_NoResize)) return;
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextWrapped(T("The example macros were added before (when the addon first started, or with this button), "
                           "and there's no Examples folder now. Add a new one?"));
        ImGui::TextDisabled(T("Your own macros aren't changed."));
        if (ImGui::Button(T("Add"), Px(100.0f, 0.0f)))
        {
            AddExamplesNow();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel"), Px(100.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    // --- Macro editor -------------------------------------------------------------------------

    constexpr size_t kMaxLinesPerMacro = kMaxLines; // MacroTree.h

    ImVec4 ChannelColorVec(EChannel aChannel)
    {
        const uint32_t c = ChatLine::ChannelColor(aChannel);
        return ImVec4(static_cast<float>((c >> 16) & 0xFF) / 255.0f, static_cast<float>((c >> 8) & 0xFF) / 255.0f,
                      static_cast<float>(c & 0xFF) / 255.0f, 1.0f);
    }

    // ImGui 1.80 has no std::string overload; grow the string as ImGui asks.
    int StringResizeCallback(ImGuiInputTextCallbackData* aData)
    {
        if (aData->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            auto* str = static_cast<std::string*>(aData->UserData);
            str->resize(static_cast<size_t>(aData->BufTextLen));
            aData->Buf = str->data();
        }
        return 0;
    }

    bool InputString(const char* aLabel, const char* aHint, std::string& aStr, ImGuiInputTextFlags aFlags = 0)
    {
        aFlags |= ImGuiInputTextFlags_CallbackResize;
        const bool changed = ImGui::InputTextWithHint(aLabel, aHint, aStr.data(), aStr.capacity() + 1, aFlags,
                                                      StringResizeCallback, &aStr);
        // A pasted newline would split the message (chat messages are one line), and ImGui lets
        // other control characters through, C1 ones like U+0085 too: all become spaces (SEC-026).
        if (changed) aStr = ChatLine::ControlsToSpaces(aStr);
        return changed;
    }

    bool InputStringMultiline(const char* aLabel, std::string& aStr, const ImVec2& aSize)
    {
        return ImGui::InputTextMultiline(aLabel, aStr.data(), aStr.capacity() + 1, aSize,
                                         ImGuiInputTextFlags_CallbackResize, StringResizeCallback, &aStr);
    }

    // aUnknown: a newer version's channel the line has (Line::UnknownChannel), shown as "? name".
    bool ChannelCombo(const char* aLabel, EChannel& aChannel, const std::string& aUnknown = {})
    {
        bool changed = false;
        ImGui::PushStyleColor(ImGuiCol_Text, aUnknown.empty() ? ChannelColorVec(aChannel) : kErrorColor);
        const std::string unknown = "? " + aUnknown;
        const bool open = ImGui::BeginCombo(aLabel, aUnknown.empty() ? Localization::Lookup(ChannelLabel(aChannel)) : unknown.c_str());
        ImGui::PopStyleColor();
        if (!open) return false;
        for (int i = 0; i < static_cast<int>(EChannel::Count); i++)
        {
            const EChannel c = static_cast<EChannel>(i);
            ImGui::PushStyleColor(ImGuiCol_Text, ChannelColorVec(c));
            if (ImGui::Selectable(Localization::Lookup(ChannelLabel(c)), c == aChannel)) { aChannel = c; changed = true; }
            ImGui::PopStyleColor();
            if (c == aChannel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
        return changed;
    }

    // Room to leave on the right of a message box for its LengthLabel.
    // The character counter: "23/199", grey, then yellow from 150, bright red from 180
    // (ChatLine::LengthLevel). Placeholders make it "~".
    void LengthLabel(const Line& aLine)
    {
        const std::string typed = ChatLine::TypedText(aLine);
        const size_t len = ChatLine::Utf16Length(typed);
        const bool placeholders = Placeholders::HasPlaceholders(typed);
        const ChatLine::ELengthLevel level = ChatLine::LengthLevel(len);
        // With placeholders the final length is only known when sending: show it as approximate.
        const char* approx = placeholders ? "~" : "";
        if (level == ChatLine::ELengthLevel::Normal)
            ImGui::TextDisabled("%s%zu/%zu", approx, len, ChatLine::kMaxChatLength);
        else
            ImGui::TextColored(level == ChatLine::ELengthLevel::Near ? kCountNearColor : kCountCloseColor,
                               "%s%zu/%zu", approx, len, ChatLine::kMaxChatLength);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Characters typed into chat, including the channel command.\n"
                              "Chat allows at most %zu. Longer lines won't be sent.%s"), ChatLine::kMaxChatLength,
                              placeholders ? (std::string("\n") + T("This line has placeholders: they're filled in when it's sent,\n"
                                                                    "and the real length is checked then.")).c_str() : "");
    }

    void LineWarnings(const Line& aLine)
    {
        if (!aLine.UnknownChannel.empty())
            ImGui::TextColored(kErrorColor, T("Its channel \"%s\" is from a newer version of the addon: this line won't be sent. "
                                              "Pick a channel to change it."), aLine.UnknownChannel.c_str());
        if (aLine.Channel == EChannel::Whisper && aLine.WhisperTo.empty())
            ImGui::TextColored(kWarnColor, T("Whisper needs a recipient."));
        if (aLine.Channel == EChannel::Whisper && ChatLine::Utf16Length(aLine.WhisperTo) > ChatLine::kMaxRecipientLength)
            ImGui::TextColored(kErrorColor, T("The recipient is longer than %zu characters: this line won't be sent."),
                               ChatLine::kMaxRecipientLength);
        if (aLine.Message.empty())
            ImGui::TextColored(kWarnColor, T("Empty message."));
        if (Placeholders::UsesRemoved(aLine.Message) || Placeholders::UsesRemoved(aLine.WhisperTo))
            ImGui::TextColored(kErrorColor, T("{txt} and {json} aren't available any more: remove them, or this macro won't be sent."));
    }

    enum class EEditMode { Lines, Raw };
    EEditMode   s_EditMode   = EEditMode::Lines;
    std::string s_RawText;
    uint64_t    s_RawForId   = 0; // which macro s_RawText was built from (0 = rebuild)

    // "Lines (n)" with "Placeholders (?)" on the right (it wraps under it when narrow).
    void DrawLinesHeader(const Node& aMacro)
    {
        ImGui::Spacing();
        const std::string title = Fmt(T("Lines (%zu)"), aMacro.Lines.size());
        ImGui::TextUnformatted(title.c_str());
        const float help = UiFont::TextWidth(T("Placeholders (?)"));
        const float right = ImGui::GetWindowContentRegionMax().x - help; // window coordinates
        const float after = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + ImGui::GetStyle().ItemSpacing.x;
        if (right > after) ImGui::SameLine(right);
        ImGui::TextDisabled(T("Placeholders (?)"));
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            UiFont::PushTooltipWrap();
            ImGui::TextUnformatted(T("Type these in a message; they're filled in when it's sent:"));
            ImGui::Separator();
            float syntaxWidth = 0.0f;
            for (const Placeholders::Info& p : Placeholders::List())
                syntaxWidth = std::max(syntaxWidth, UiFont::TextWidth(p.Syntax));
            for (const Placeholders::Info& p : Placeholders::List())
            {
                ImGui::TextUnformatted(p.Syntax);
                ImGui::SameLine(syntaxWidth + ImGui::GetStyle().ItemSpacing.x * 3.0f);
                ImGui::TextDisabled("%s", Localization::Lookup(p.Description));
            }
            ImGui::Separator();
            ImGui::TextDisabled(T("Anything else in { } is sent as typed. Chat links like [&BDAEAAA=] work as is."));
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::Separator();
    }

    // One row per line: number, channel (a Whisper's recipient or a Squad line's Broadcast box
    // next to it), message, character count, up/down and remove. The message box takes what's
    // left of the row; on a narrow pane (or with big text) it gets a row of its own.
    void DrawLinesEditor(Node& aMacro)
    {
        auto& lines = aMacro.Lines;
        int moveFrom = -1, moveTo = -1, removeAt = -1;
        const float sp     = ImGui::GetStyle().ItemSpacing.x;
        const float frame  = ImGui::GetFrameHeight();
        const float countW = UiFont::TextWidth("~000/000");
        const float tailW  = countW + sp + frame + 2.0f + frame + sp + frame; // count, up, down, remove
        const int count = static_cast<int>(lines.size());

        for (int i = 0; i < count; i++)
        {
            Line& l = lines[static_cast<size_t>(i)];
            ImGui::PushID(i);
            const float left  = ImGui::GetCursorScreenPos().x;
            const float right = left + ImGui::GetContentRegionAvail().x;

            UiFont::Row row;
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%d", i + 1);
            row.Next(Px(100.0f));
            ImGui::SetNextItemWidth(Px(100.0f));
            if (ChannelCombo("##channel", l.Channel, l.UnknownChannel)) { l.UnknownChannel.clear(); MarkDirty(); }
            if (l.Channel == EChannel::Whisper)
            {
                row.Next(Px(110.0f));
                ImGui::SetNextItemWidth(Px(110.0f));
                if (InputString("##to", T("Recipient"), l.WhisperTo)) MarkDirty();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Character or account name to whisper."));
            }
            else if (l.Channel == EChannel::Squad)
            {
                row.Next(UiFont::CheckboxWidth(T("Broadcast")));
                if (ImGui::Checkbox(T("Broadcast"), &l.SquadBroadcast)) MarkDirty();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(T("Send as a squad broadcast. Only works while you're the commander;\n"
                                      "otherwise it's sent as a normal squad message."));
            }

            float msgW = right - (ImGui::GetItemRectMax().x + sp) - sp - tailW;
            if (msgW >= Px(120.0f)) ImGui::SameLine();
            else msgW = std::max(Px(60.0f), right - left - sp - tailW);
            ImGui::SetNextItemWidth(msgW);
            if (InputString("##msg", T("Message"), l.Message)) MarkDirty();
            ImGui::SameLine();
            LengthLabel(l);
            // The arrows line up whatever the count's width.
            ImGui::SameLine(0.0f, sp + std::max(0.0f, countW - ImGui::GetItemRectSize().x));
            if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0) { moveFrom = i; moveTo = i - 1; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Move this line up"));
            ImGui::SameLine(0.0f, 2.0f);
            if (ImGui::ArrowButton("##down", ImGuiDir_Down) && i + 1 < count) { moveFrom = i; moveTo = i + 1; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Move this line down"));
            ImGui::SameLine();
            if (ImGui::Button("X##remove", ImVec2(frame, frame))) removeAt = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Remove this line"));

            const size_t len = ChatLine::Utf16Length(ChatLine::TypedText(l));
            if (ChatLine::LengthLevel(len) == ChatLine::ELengthLevel::Over)
                ImGui::TextColored(kErrorColor, T("Over %zu characters: this line won't be sent."), ChatLine::kMaxChatLength);
            LineWarnings(l);
            ImGui::PopID();
        }

        // Apply after the loop so the vector isn't changed while it's being drawn.
        if (removeAt >= 0) { lines.erase(lines.begin() + removeAt); MarkDirty(); }
        else if (moveFrom >= 0) { std::swap(lines[static_cast<size_t>(moveFrom)], lines[static_cast<size_t>(moveTo)]); MarkDirty(); }

        if (lines.size() < kMaxLinesPerMacro)
        {
            if (ImGui::Button(T("Add line"), ImVec2(-1.0f, 0.0f)))
            {
                Line l;
                if (!lines.empty()) { l.Channel = lines.back().Channel; l.WhisperTo = lines.back().WhisperTo; }
                lines.push_back(std::move(l));
                MarkDirty();
            }
        }
        else
        {
            ImGui::TextDisabled(T("A macro can have at most %zu lines."), kMaxLinesPerMacro);
        }
    }

    void DrawRawEditor(Node& aMacro)
    {
        // Raw text has no way to write a newer version's channel: editing it here would turn that
        // line into a current-channel line.
        if (MacroTree::HasUnknownChannel(aMacro))
        {
            ImGui::TextColored(kErrorColor, T("A line's channel is from a newer version of the addon, which raw text can't show. "
                                              "Edit this macro on the Lines tab."));
            return;
        }
        if (s_RawForId != aMacro.Id)
        {
            s_RawText  = ChatLine::SerializeRaw(aMacro.Lines);
            s_RawForId = aMacro.Id;
        }

        ImGui::TextDisabled(T("One message per row, typed as in game:"));
        ImGui::TextDisabled(T("/s hello   /w [Name] hi   /d ! broadcast   /g3 guild 3"));
        const float height = ImGui::GetTextLineHeightWithSpacing() * 8.0f;
        if (InputStringMultiline("##raw", s_RawText, ImVec2(-1.0f, height)))
        {
            // Lines follow the text as you type; the text itself is left exactly as typed.
            aMacro.Lines = ChatLine::ParseRaw(s_RawText);
            if (aMacro.Lines.size() > kMaxLinesPerMacro) aMacro.Lines.resize(kMaxLinesPerMacro);
            MarkDirty();
        }

        ImGui::Spacing();
        ImGui::TextDisabled(T("Will send:"));
        if (aMacro.Lines.empty()) ImGui::TextDisabled(T("(nothing)"));
        for (size_t i = 0; i < aMacro.Lines.size(); i++)
        {
            const Line& l = aMacro.Lines[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextColored(ChannelColorVec(l.Channel), "[%s]", Localization::Lookup(ChannelLabel(l.Channel)));
            ImGui::SameLine();
            if (l.Channel == EChannel::Whisper && !l.WhisperTo.empty())
            {
                ImGui::TextColored(ChannelColorVec(l.Channel), T("to %s:"), l.WhisperTo.c_str());
                ImGui::SameLine();
            }
            if (l.Channel == EChannel::Squad && l.SquadBroadcast)
            {
                ImGui::TextColored(ChannelColorVec(l.Channel), "%s", T("(broadcast)"));
                ImGui::SameLine();
            }
            LengthLabel(l);
            ImGui::SameLine();
            ImGui::TextWrapped("%s", l.Message.c_str());
            LineWarnings(l);
            ImGui::PopID();
        }
        if (aMacro.Lines.size() >= kMaxLinesPerMacro)
            ImGui::TextColored(kWarnColor, T("Only the first %zu lines are kept."), kMaxLinesPerMacro);
    }

    // --- Link files ---------------------------------------------------------------------------
    // A macro can be kept in step with a .txt file (src/LinkSync.*). The files are checked every
    // second on the render thread, a couple per frame, outside the tree drawing; only a changed
    // file is read.

    constexpr double kLinkSyncEverySec = 1.0;
    constexpr char   kLinkMissing[]    = TX("The file isn't there. Changes aren't saved to it until it's back or recreated.");

    std::unordered_map<uint64_t, LinkSync::State> s_LinkStates;
    std::unordered_map<uint64_t, std::string>     s_LinkProblems; // what's wrong with a link, for the editor
    double      s_NextLinkSync = 0.0;
    constexpr size_t kLinksPerFrame = 2; // linked macros checked per frame (SyncLinks)
    std::vector<Node*> s_LinkRound;        // this round's linked macros
    size_t             s_LinkRoundAt  = 0; // the next one to check
    uint64_t           s_LinkRoundGen = 0; // s_TreeGen when the round was listed
    std::string s_LinkPath;          // the path being typed in the editor
    uint64_t    s_LinkPathFor = 0;   // which macro s_LinkPath belongs to

    int64_t FileMtime(const std::filesystem::path& aFile, bool& aExists)
    {
        std::error_code ec;
        aExists = std::filesystem::is_regular_file(aFile, ec);
        if (!aExists) return 0;
        const auto t = std::filesystem::last_write_time(aFile, ec);
        return ec ? 0 : static_cast<int64_t>(t.time_since_epoch().count());
    }

    bool ReadLinkFile(const std::filesystem::path& aFile, std::string& aText, std::string& aError)
    {
        std::error_code ec;
        const auto size = std::filesystem::file_size(aFile, ec);
        if (ec) { aError = T("the file couldn't be read."); return false; }
        if (size > LinkSync::kMaxFileBytes) { aError = T("the file is too big (over 256 KB)."); return false; }
        std::ifstream in(aFile, std::ios::binary);
        if (!in) { aError = T("the file couldn't be opened (is another program locking it?)."); return false; }
        aText.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (aText.size() >= 3 && aText.compare(0, 3, "\xEF\xBB\xBF") == 0) aText.erase(0, 3); // UTF-8 BOM
        return true;
    }

    // Writes the macro's lines to its file and records the result as the agreed state.
    bool WriteLinkFile(const Node& aMacro, const std::filesystem::path& aFile, LinkSync::State& aState)
    {
        std::error_code ec;
        std::filesystem::create_directories(aFile.parent_path(), ec);
        if (!Storage::WriteText(aFile, LinkSync::FileText(aMacro.Lines))) return false;
        bool exists = false;
        aState.Known = true;
        aState.Mtime = FileMtime(aFile, exists);
        aState.Text  = ChatLine::SerializeRaw(aMacro.Lines);
        return true;
    }

    void CollectLinked(Node& aFolder, std::vector<Node*>& aOut)
    {
        for (auto& c : aFolder.Children)
        {
            if (c->IsFolder()) CollectLinked(*c, aOut);
            else if (!c->LinkFile.empty()) aOut.push_back(c.get());
        }
    }

    // Checks one linked macro against its file (and writes or loads it when one side changed).
    void SyncOne(Node* m, const std::filesystem::path& aDir)
    {
        if (m->LinkFile.empty()) return; // unlinked since the round began
        // Its file is raw text, which can't hold a newer version's channel: syncing would turn
        // that line into a current-channel line. Paused until the line gets another channel.
        const std::filesystem::path file = LinkSync::Resolve(aDir, m->LinkFile);
        LinkSync::State& st = s_LinkStates[m->Id];
        if (MacroTree::HasUnknownChannel(*m))
        {
            // The file as it is now counts as agreed: when the line gets a channel again, the
            // macro's edits go to the file instead of the file replacing them (a first sync
            // would load the file). A later change to the file still wins, as always.
            if (!st.Known)
            {
                bool exists = false;
                st.Mtime = FileMtime(file, exists);
                st.Text  = ChatLine::SerializeRaw(m->Lines);
                st.Known = exists;
            }
            s_LinkProblems[m->Id] = T("Not kept in step with its file: a line's channel is from a newer version of the addon.");
            return;
        }
        const LinkSync::State before = st;
        LinkSync::File view;
        view.Mtime = FileMtime(file, view.Exists);
        view.Read  = [&file](std::string& aText, std::string& aError) { return ReadLinkFile(file, aText, aError); };

        const LinkSync::Outcome o = LinkSync::Sync(st, m->Lines, view);
        std::string problem;
        if (o.Missing) problem = T(kLinkMissing);
        else if (!o.Error.empty()) problem = Fmt(T("The file can't be read: %s"), o.Error.c_str());
        else if (o.LoadMacro)
        {
            m->Lines = o.Lines;
            if (s_RawForId == m->Id) s_RawForId = 0;
            MarkDirty();
        }
        else if (o.WriteFile)
        {
            if (WriteLinkFile(*m, file, st)) {}
            else
            {
                st = before; // try again next time
                problem = T("The file couldn't be written. Check the Nexus log.");
            }
        }
        if (problem.empty() && !o.Warning.empty()) problem = o.Warning;
        if (problem.empty()) s_LinkProblems.erase(m->Id);
        else s_LinkProblems[m->Id] = problem;
    }

    // A round starts every second: it lists the linked macros, then checks a few of them each
    // frame (kLinksPerFrame), so many links never stall one frame on file checks. aNow checks
    // them all at once (unload, linking a macro, turning Advanced options on).
    void SyncLinks(bool aNow)
    {
        if (!s_AdvancedOn) return; // paused; the agreed states are kept for when it's back on
        const double now = ImGui::GetTime();
        // The listed pointers are only good while the library is unchanged: list them again if it
        // changed, and go on from the same place (typing in a macro changes it every few frames).
        if (s_LinkRoundGen != s_TreeGen)
        {
            s_LinkRound.clear();
            CollectLinked(s_Tree.Root(), s_LinkRound);
            s_LinkRoundGen = s_TreeGen;
            s_LinkRoundAt  = std::min(s_LinkRoundAt, s_LinkRound.size());
        }
        if (aNow || (s_LinkRoundAt >= s_LinkRound.size() && now >= s_NextLinkSync))
        {
            s_NextLinkSync = now + kLinkSyncEverySec;
            s_LinkRound.clear();
            s_LinkRoundAt  = 0;
            s_LinkRoundGen = s_TreeGen;
            CollectLinked(s_Tree.Root(), s_LinkRound);
            // Forget macros that were unlinked or deleted.
            std::unordered_set<uint64_t> ids;
            for (const Node* n : s_LinkRound) ids.insert(n->Id);
            for (auto it = s_LinkStates.begin(); it != s_LinkStates.end();)
                it = ids.count(it->first) ? std::next(it) : s_LinkStates.erase(it);
        }
        if (s_LinkRoundAt >= s_LinkRound.size()) return;
        const std::filesystem::path dir = Storage::AddonDir();
        if (dir.empty()) { s_LinkRoundAt = s_LinkRound.size(); return; }
        const size_t until = aNow ? s_LinkRound.size() : std::min(s_LinkRound.size(), s_LinkRoundAt + kLinksPerFrame);
        while (s_LinkRoundAt < until)
        {
            Node* m = s_LinkRound[s_LinkRoundAt++];
            SyncOne(m, dir);
            // A file loaded into its macro (MarkDirty) only changed that macro's lines: the list holds.
            s_LinkRoundGen = s_TreeGen;
        }
    }

    // Is this file already linked to another macro? (Two macros writing one file would fight.)
    const Node* LinkedElsewhere(const Node& aFolder, const Node& aMacro, const std::filesystem::path& aDir, const std::filesystem::path& aFile)
    {
        for (const auto& c : aFolder.Children)
        {
            if (c->IsFolder())
            {
                if (const Node* n = LinkedElsewhere(*c, aMacro, aDir, aFile)) return n;
            }
            else if (c.get() != &aMacro && !c->LinkFile.empty() && LinkSync::SamePath(LinkSync::Resolve(aDir, c->LinkFile), aFile))
                return c.get();
        }
        return nullptr;
    }

    void LinkMacro(Node& aMacro, const std::string& aPath)
    {
        const std::string action = Fmt(T("Linked \"%s\" to the file \"%s\""), aMacro.Name.c_str(), aPath.c_str());
        const std::string bad = LinkSync::CheckPath(aPath);
        if (!bad.empty()) { ShowError(action, bad); return; }
        // A text file can't hold a newer version's channel: it would become a current-channel line.
        if (MacroTree::HasUnknownChannel(aMacro))
        {
            ShowError(action, T("A line's channel is from a newer version of the addon, which a text file can't hold. Pick a channel for it first."));
            return;
        }
        const std::filesystem::path dir = Storage::AddonDir();
        if (dir.empty()) { ShowError(action, T("The addon folder isn't available.")); return; }
        const std::filesystem::path file = LinkSync::Resolve(dir, aPath);
        if (const Node* other = LinkedElsewhere(s_Tree.Root(), aMacro, dir, file))
        {
            ShowError(action, Fmt(T("\"%s\" is already linked to that file."), other->Name.c_str()));
            return;
        }

        LinkSync::State st;
        bool exists = false;
        FileMtime(file, exists);
        std::string text, error;
        if (exists && !ReadLinkFile(file, text, error))
        {
            ShowError(action, Fmt(T("That file can't be used: %s"), error.c_str())); // never overwrite a file we couldn't read
            return;
        }
        const bool hasLines = exists && !ChatLine::ParseRaw(text).empty();
        if (!hasLines)
        {
            // No file yet (or an empty one): it's made from the macro.
            if (!WriteLinkFile(aMacro, file, st))
            {
                ShowError(action, T("The file couldn't be created. Check the path and the Nexus log."));
                return;
            }
            SetInfo(T("Linked. The file was created from the macro's lines."));
        }
        else SetInfo(T("Linked. The macro now has the file's lines."));
        // An existing file with lines wins: the first sync (State not known yet) loads it.

        const size_t first = aPath.find_first_not_of(" \t");
        aMacro.LinkFile = aPath.substr(first, aPath.find_last_not_of(" \t") - first + 1);
        s_LinkStates[aMacro.Id] = st;
        s_LinkProblems.erase(aMacro.Id);
        s_LinkPath.clear();
        MarkDirty();
        SyncLinks(true);
    }

    void DrawLinkFile(Node& aMacro)
    {
        if (!s_AdvancedOn) return; // an advanced option: hidden while they're off
        if (s_LinkPathFor != aMacro.Id)
        {
            s_LinkPathFor = aMacro.Id;
            s_LinkPath.clear();
        }
        const auto problem = s_LinkProblems.find(aMacro.Id);
        const std::string header = aMacro.LinkFile.empty()
            ? std::string(T("Text file link: none"))
            : Fmt(T("Text file link: %s"), aMacro.LinkFile.c_str()) +
                  (problem != s_LinkProblems.end() ? std::string(" ") + T("(needs attention)") : std::string());
        if (problem != s_LinkProblems.end()) ImGui::TextColored(kWarnColor, "%s", header.c_str());
        else ImGui::TextUnformatted(header.c_str());
        ImGui::Indent();

        if (aMacro.LinkFile.empty())
        {
            ImGui::TextWrapped(T("Keep this macro in step with a .txt file, one chat line per row (like the Raw text "
                               "tab). Edit the file in Notepad and the macro follows; edit the macro here and the "
                               "file follows."));
            ImGui::TextDisabled(T("A name alone goes in <GW2>\\addons\\GW2_Chat_Messages\\links\\. A file that already "
                                "has lines replaces the macro's lines; otherwise it's created from them."));
            ImGui::SetNextItemWidth(-(UiFont::ButtonWidth(T("Link")) + ImGui::GetStyle().ItemSpacing.x));
            const bool enter = InputString("##linkpath", T("callouts.txt, or a full path"), s_LinkPath, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button(T("Link")) || enter)
            {
                std::string path = s_LinkPath;
                if (path.size() >= 2 && path.front() == '"' && path.back() == '"') path = path.substr(1, path.size() - 2);
                LinkMacro(aMacro, path);
            }
        }
        else
        {
            const std::filesystem::path dir  = Storage::AddonDir();
            const std::filesystem::path file = LinkSync::Resolve(dir, aMacro.LinkFile);
            ImGui::TextWrapped(T("File: %s"), PathText::Utf8(file).c_str());
            const bool missing = problem != s_LinkProblems.end() && problem->second == T(kLinkMissing);
            if (problem != s_LinkProblems.end()) ImGui::TextColored(kWarnColor, "%s", problem->second.c_str());
            else ImGui::TextDisabled(T("In step. Changes on either side show up within a second."));
            UiFont::Row row;
            if (ImGui::SmallButton(T("Open file"))) Storage::OpenFolder(file);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Opens it in your text editor (Notepad, unless you chose another)."));
            row.Next(UiFont::ButtonWidth(T("Open folder")));
            if (ImGui::SmallButton(T("Open folder"))) Storage::OpenFolder(file.parent_path());
            if (missing)
            {
                row.Next(UiFont::ButtonWidth(T("Recreate the file")));
                if (ImGui::SmallButton(T("Recreate the file")))
                {
                    if (WriteLinkFile(aMacro, file, s_LinkStates[aMacro.Id])) s_LinkProblems.erase(aMacro.Id);
                    else ShowError(Fmt(T("Clicked Recreate the file for \"%s\""), aMacro.Name.c_str()), T("The file couldn't be created. Check the Nexus log."));
                }
            }
            row.Next(UiFont::ButtonWidth(T("Unlink")));
            if (ImGui::SmallButton(T("Unlink")))
            {
                aMacro.LinkFile.clear();
                s_LinkStates.erase(aMacro.Id);
                s_LinkProblems.erase(aMacro.Id);
                MarkDirty();
                SetInfo(T("Unlinked. The file was left as it is."));
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Stops keeping the macro and the file in step. Neither is deleted."));
        }
        ImGui::Unindent();
    }

    // The note: what the macro is for, never sent. The box keeps what's typed (spaces at the end
    // too) while it's being edited; the macro gets the cleaned text (Tree::CleanNote).
    std::string s_NoteBuf;
    uint64_t    s_NoteForId   = 0;
    bool        s_NoteEditing = false;

    void DrawNote(Node& aMacro)
    {
        // Once it isn't being edited, the box shows what was kept (cut, merged lines, no end spaces).
        if (s_NoteForId != aMacro.Id || (!s_NoteEditing && s_NoteBuf != aMacro.Note))
        {
            s_NoteBuf     = aMacro.Note;
            s_NoteForId   = aMacro.Id;
            s_NoteEditing = false;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped(T("What it's for, or when to use it. Never typed into chat: it's shown in the quick "
                             "menu's preview and found by the search. At most %zu lines and %zu bytes."),
                           kMaxNoteLines, kMaxNoteLength);
        ImGui::PopStyleColor();
        const ImGuiStyle& style = ImGui::GetStyle();
        const float height = ImGui::GetTextLineHeight() * 3.0f + style.FramePadding.y * 2.0f;
        if (InputStringMultiline("##note", s_NoteBuf, ImVec2(-1.0f, height)))
        {
            const std::string clean = Tree::CleanNote(s_NoteBuf);
            if (clean != aMacro.Note)
            {
                aMacro.Note = clean;
                MarkDirty();
            }
        }
        s_NoteEditing = ImGui::IsItemActive();
        if (s_NoteBuf.size() > kMaxNoteLength)
            ImGui::TextColored(kWarnColor, T("A note can be at most %zu bytes long: the rest isn't kept."), kMaxNoteLength);
    }

    // "Hotkey and favourite: Ctrl+F1, favourite" and its warning (a hotkey that can't work, or
    // one shared with a macro that can be active at the same time).
    std::string HotkeySummary(const Node& aMacro)
    {
        const std::string keys = aMacro.Hotkey.Empty() ? std::string(T("no hotkey")) : Hotkey::Display(aMacro.Hotkey);
        return aMacro.Favourite ? Fmt(T("%s, favourite"), keys.c_str()) : keys;
    }

    const char* HotkeyWarning(const Node& aMacro)
    {
        if (aMacro.Hotkey.Empty()) return nullptr;
        if (Hotkey::NeedsModifier(aMacro.Hotkey)) return T("(the hotkey doesn't work)");
        for (const Node* n : s_Tree.MacrosWithHotkey(aMacro.Hotkey))
            if (n != &aMacro && FiltersOverlap(aMacro, *n)) return T("(the hotkey is shared)");
        // The same hints as DrawHotkeyRow: a GW2 default key, or a plain letter or number.
        if (GameKeys::DefaultUse(aMacro.Hotkey) ||
            (!aMacro.Hotkey.HasModifier() && aMacro.Hotkey.Key < 0x60 && aMacro.Hotkey.Key > Hotkey::kVkXButton2))
            return T("(it may be a game key)");
        return nullptr;
    }

    // Advanced options, all in one section at the bottom (only while they're on).
    void DrawAdvanced(Node& aMacro)
    {
        const bool attention = !aMacro.LinkFile.empty() && s_LinkProblems.count(aMacro.Id) > 0;
        if (!Section(T("Advanced"), attention ? T("(needs attention)") : nullptr, s_Sections.Advanced, "##sec_advanced"))
            return;
        ImGui::Indent();
        UiFont::Row modeRow;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Edit the lines as:"));
        int mode = static_cast<int>(s_EditMode);
        modeRow.Next(UiFont::CheckboxWidth(T("Lines")));
        if (ImGui::RadioButton(T("Lines"), &mode, static_cast<int>(EEditMode::Lines))) s_RawForId = 0;
        modeRow.Next(UiFont::CheckboxWidth(T("Raw text")));
        if (ImGui::RadioButton(T("Raw text"), &mode, static_cast<int>(EEditMode::Raw))) s_RawForId = 0;
        s_EditMode = static_cast<EEditMode>(mode);
        DrawCooldownRow(aMacro);
        ImGui::Spacing();
        DrawLinkFile(aMacro);
        ImGui::Unindent();
    }

    void DrawMacroEditor(Node& aMacro)
    {
        // The Raw text choice is an advanced option: while they're off, the editor is always Lines.
        if (!s_AdvancedOn && s_EditMode == EEditMode::Raw)
        {
            s_EditMode = EEditMode::Lines;
            s_RawForId = 0;
        }
        ImGui::BeginGroup();
        DrawLinesHeader(aMacro);
        if (s_EditMode == EEditMode::Lines) DrawLinesEditor(aMacro);
        else DrawRawEditor(aMacro);
        ImGui::EndGroup();
        TourMark(Welcome::ETarget::Lines);
        ImGui::Spacing();

        ImGui::BeginGroup();
        DrawFilters(aMacro);

        if (s_CaptureFor == aMacro.Id && Hotkeys::IsCapturing() && !s_Sections.Hotkey) // keep the prompt in view
        {
            s_Sections.Hotkey = true;
            if (!Storage::SaveSetting(EditorLayout::kSetting, EditorLayout::ToJson(s_Sections)))
                LogF(LOGL_WARNING, "The editor's open sections couldn't be saved.");
        }
        if (Section(Fmt(T("Hotkey and favourite: %s"), HotkeySummary(aMacro).c_str()), HotkeyWarning(aMacro),
                    s_Sections.Hotkey, "##sec_hotkey"))
        {
            ImGui::Indent();
            DrawHotkeyRow(aMacro);
            DrawFavouriteRow(aMacro);
            ImGui::Unindent();
        }

        const std::string note = aMacro.Note.empty() ? std::string(T("Note: none"))
                                                     : Fmt(T("Note: %s"), EditorLayout::ShortNote(aMacro.Note).c_str());
        if (Section(note, nullptr, s_Sections.Note, "##sec_note"))
        {
            ImGui::Indent();
            DrawNote(aMacro);
            ImGui::Unindent();
        }
        ImGui::EndGroup();
        TourMark(Welcome::ETarget::Sections);

        if (s_AdvancedOn) DrawAdvanced(aMacro);
    }

    // --- Details pane -------------------------------------------------------------------------

    // The header: the name, with Send now / Preview (macros) and "..." on its row (they wrap under
    // a long name or on a narrow pane), then the path and a summary in grey.
    void DrawDetailsHeader(Node& aNode)
    {
        const bool macro = aNode.IsMacro();
        ImGui::BeginGroup(); // the tour points at the name, its buttons and the grey line
        UiFont::Row head;
        ImGui::TextWrapped("%s", aNode.Name.c_str());
        if (macro)
        {
            const bool busy = Sender::IsBusy();
            const bool blocked = IsBlocked(aNode);
            head.Next(UiFont::ButtonWidth(T("Send now")));
            if (busy || blocked) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            if (ImGui::Button(T("Send now")) && !busy) SendMacro(aNode); // a blocked macro says why
            if (busy || blocked) ImGui::PopStyleVar();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(T("Types each line into its chat channel and sends it.\n"
                                  "Needs your Chat key set in Nexus > Options > Game Keybinds."));
            head.Next(UiFont::ButtonWidth(T("Preview")));
            if (ImGui::Button(T("Preview"))) MakePreview(aNode);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(T("Shows what Send now would type, with placeholders filled in,\n"
                                  "and runs the same checks. Nothing is sent."));
        }
        head.Next(UiFont::ButtonWidth("..."));
        if (ImGui::Button("...##more")) ImGui::OpenPopup("##more");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", macro ? T("Rename, duplicate or delete it") : T("Rename or delete it"));
        // A menu, not a modal: its items queue an action, and the pop-ups open at the window's level.
        if (ImGui::BeginPopup("##more"))
        {
            if (ImGui::MenuItem(T("Rename"))) s_Pending = {EAction::OpenName, 0, aNode.Id, EDrop::Into, 0, ENameMode::Rename};
            if (macro && ImGui::MenuItem(T("Duplicate"))) s_Pending = {EAction::Duplicate, aNode.Id};
            if (ImGui::MenuItem(s_TrashOn ? T("Delete") : T("Delete..."))) s_Pending = {EAction::OpenDelete, aNode.Id};
            if (s_TrashOn && ImGui::IsItemHovered()) ImGui::SetTooltip(T("Moves it to the trash bin, where it can be restored."));
            ImGui::EndPopup();
        }

        std::string summary = PathOf(aNode);
        if (macro && !aNode.Hotkey.Empty()) summary += "  \xC2\xB7  " + Hotkey::Display(aNode.Hotkey);
        summary += "  \xC2\xB7  " + FilterSummary(aNode);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", summary.c_str());
        ImGui::PopStyleColor();
        ImGui::EndGroup();
        if (macro) TourMark(Welcome::ETarget::Header);

        if (macro)
        {
            if (IsBlocked(aNode))
                ImGui::TextColored(kWarnColor, "%s", T("This macro is linked to a text file, which is an advanced option. Advanced "
                                                       "options are off, so it can't be sent (its hotkey does nothing) and it isn't "
                                                       "kept in step with its file. Turn on \"Advanced options\" on the addon's "
                                                       "options page to use it again."));
            if (!s_AdvancedOn) DrawCooldownRow(aNode); // only says so when one is kept but ignored
            DrawSendStatus();
            DrawPreview(aNode);
        }
        ImGui::Separator();
    }

    void DrawDetails()
    {
        Node* node = s_Tree.Find(s_SelectedId);
        if (!node)
        {
            const int ti = s_Tree.TrashIndexOf(s_TrashSelId);
            if (ti >= 0)
            {
                DrawTrashDetails(s_Tree.Trash()[static_cast<size_t>(ti)]);
                return;
            }
            ImGui::TextDisabled(T("Select a folder or macro."));
            ImGui::Spacing();
            ImGui::TextWrapped(T("Right-click an item for more options. Drag items to move them: drop on the middle "
                               "of a folder to put them inside it, or above or below an item to place them next to it."));
            return;
        }

        DrawDetailsHeader(*node);
        if (node->IsFolder())
        {
            size_t folders = 0, macros = 0;
            Tree::CountDescendants(*node, &folders, &macros);
            const int level = Tree::FolderLevel(*node);
            ImGui::TextDisabled(T("Folder level %d of %d"), level, kMaxFolderDepth);
            ImGui::TextDisabled(T("Contains %zu folder(s) and %zu macro(s)"), folders, macros);
            DrawFavouriteRow(*node);
            DrawFilters(*node);
            ImGui::Spacing();
            UiFont::Row newRow;
            if (level < kMaxFolderDepth)
            {
                if (ImGui::Button(T("New folder inside")))
                    s_Pending = {EAction::OpenName, 0, node->Id, EDrop::Into, 0, ENameMode::NewFolder};
                newRow.Next(UiFont::ButtonWidth(T("New macro inside")));
            }
            if (ImGui::Button(T("New macro inside")))
                s_Pending = {EAction::OpenName, 0, node->Id, EDrop::Into, 0, ENameMode::NewMacro};
        }
        else
        {
            DrawMacroEditor(*node);
        }
    }

    // --- Quick menu ---------------------------------------------------------------------------
    // The library as nested menus: in the Quick Access right-click menu and in a pop-up opened by
    // its own keybind. Clicking a macro sends it. Drawing it never changes the tree.

    constexpr const char* kQuickMenuPopup = "##chatmsg_quickmenu";
    constexpr size_t      kPreviewLines   = 8;
    bool s_QuickMenuOpenRequest  = false;
    bool s_QuickMenuCloseRequest = false;

    // Whether a folder has any macro active here, at any depth (folders without one aren't listed).
    void MacroPreviewTooltip(const Node& aMacro, bool aInactive = false)
    {
        ImGui::BeginTooltip();
        UiFont::PushTooltipWrap(); // a chat line can be 199 characters long
        if (aInactive) ImGui::TextDisabled("%s", T("Not set for this map or game mode. Clicking still sends it."));
        if (!aMacro.Note.empty())
        {
            ImGui::TextDisabled("%s", aMacro.Note.c_str()); // never sent: grey, above the lines
            ImGui::Separator();
        }
        if (aMacro.Lines.empty()) ImGui::TextDisabled(T("No lines yet."));
        for (size_t i = 0; i < aMacro.Lines.size() && i < kPreviewLines; i++)
        {
            const Line& l = aMacro.Lines[i];
            DrawLinePreview(l);
        }
        if (aMacro.Lines.size() > kPreviewLines)
            ImGui::TextDisabled(T("...and %zu more line(s)"), aMacro.Lines.size() - kPreviewLines);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    constexpr float kDimAlpha = 0.45f; // a favourite that isn't active here (like the library's dimmed macros)

    // One macro entry: coloured by its first line's channel, its hotkey on the right, the preview
    // on hover. Clicking sends it (sending doesn't change the tree). aDim fades a favourite that
    // isn't active here; it still sends.
    void DrawMenuMacro(const Node& n, const char* aLabel, bool aBusy, bool aDim = false)
    {
        const EChannel first = n.Lines.empty() ? EChannel::Current : n.Lines.front().Channel;
        const std::string keys = Hotkey::Fires(n.Hotkey) ? Hotkey::Display(n.Hotkey) : std::string(); // an off hotkey isn't shown
        const bool blocked = IsBlocked(n);
        ImVec4 color = ChannelColorVec(first);
        if (aDim || blocked) color.w *= kDimAlpha;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        const bool clicked = ImGui::MenuItem(aLabel, keys.empty() ? nullptr : keys.c_str(), false, !aBusy && !blocked);
        ImGui::PopStyleColor();
        if (blocked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", T("Linked to a text file, an advanced option that's off:\n"
                                    "turn on \"Advanced options\" on the addon's options page to send it."));
        else if (ImGui::IsItemHovered()) MacroPreviewTooltip(n, aDim);
        if (clicked) SendMacro(n, ESendFrom::Menu);
    }

    // A favourite folder as a sub-menu: everything in it, whatever the filters (what isn't active
    // here is faded). Folders without any macro are left out.
    bool BeginFolderMenu(const Node& aFolder, const char* aLabel, bool aDim)
    {
        ImVec4 color = kFolderColor;
        if (aDim) color.w *= kDimAlpha;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        const bool open = ImGui::BeginMenu(aLabel);
        ImGui::PopStyleColor();
        return open;
    }

    void DrawFavouriteFolderItems(const Node& aFolder, const Here& aHere)
    {
        const bool busy = Sender::IsBusy();
        const auto active = [&](const Node& m) { return IsActiveHere(m, aHere); };
        for (const auto& child : aFolder.Children)
        {
            const Node& n = *child;
            ImGui::PushID(static_cast<int>(n.Id ^ (n.Id >> 32)));
            if (n.IsFolder())
            {
                if (QuickLists::HasActive(n, nullptr)) // has a macro at all
                {
                    if (BeginFolderMenu(n, n.Name.c_str(), !QuickLists::HasActive(n, active)))
                    {
                        DrawFavouriteFolderItems(n, aHere);
                        ImGui::EndMenu();
                    }
                }
            }
            else
            {
                DrawMenuMacro(n, n.Name.c_str(), busy, !active(n));
            }
            ImGui::PopID();
        }
    }

    void DrawQuickMenuItems(const Node& aFolder, const Here& aHere)
    {
        const bool busy = Sender::IsBusy();
        for (const auto& child : aFolder.Children)
        {
            const Node& n = *child;
            // Folder and macro siblings may share a name: give every entry its own ID.
            ImGui::PushID(static_cast<int>(n.Id ^ (n.Id >> 32)));
            if (n.IsFolder())
            {
                if (HasActiveMacros(n, aHere))
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, kFolderColor);
                    const bool open = ImGui::BeginMenu(n.Name.c_str());
                    ImGui::PopStyleColor();
                    if (open)
                    {
                        DrawQuickMenuItems(n, aHere);
                        ImGui::EndMenu();
                    }
                }
            }
            else if (IsActiveHere(n, aHere))
            {
                DrawMenuMacro(n, n.Name.c_str(), busy);
            }
            ImGui::PopID();
        }
    }

    // The quick menu's search box. The pop-up and the Quick Access menu each have their own; the
    // text is cleared whenever the menu wasn't drawn the frame before (it was closed).
    struct MenuSearch
    {
        char Buf[128]  = {};
        int  LastFrame = -2;
        bool Focus     = false; // put the cursor in the box (the pop-up, when it opens)
    };
    MenuSearch     s_PopupSearch;
    MenuSearch     s_QaSearch;
    Search::Result s_MenuResult;
    SearchCache    s_MenuCache; // what s_MenuResult was worked out for (ActiveOnly is always true)
    constexpr size_t kMenuResultsMax = 30;

    // The folders a search result sits in, "A > B" ("" at the top level).
    std::string MenuPath(const Node& aNode)
    {
        std::vector<const Node*> chain;
        for (const Node* n = aNode.Parent; n && n->Parent; n = n->Parent) chain.push_back(n);
        std::string path;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) path += (path.empty() ? "" : " > ") + (*it)->Name;
        return path;
    }

    void CollectShownMacros(const Node& aFolder, const Search::Result& aResult, std::vector<const Node*>& aOut)
    {
        for (const auto& c : aFolder.Children)
        {
            if (!aResult.Shows(c->Id)) continue;
            if (c->IsMacro()) aOut.push_back(c.get());
            else CollectShownMacros(*c, aResult, aOut);
        }
    }

    // The search results: a flat list of the macros active here that match, with their folders.
    // Enter sends the first one.
    void DrawMenuResults(const Here& aHere, const std::string& aQuery, bool aEnter)
    {
        // Worked out again only when the text, where you are or the library changed (or twice a
        // second anyway), not every frame.
        const double now = ImGui::GetTime();
        SearchCache& c = s_MenuCache;
        const bool same = c.Valid && c.Query == aQuery && c.Mode == aHere.Mode && c.MapId == aHere.MapId &&
                          c.Gen == s_TreeGen && c.Lang == Localization::Active() && now >= c.At &&
                          now - c.At < kCacheRefreshSec;
        if (!same)
        {
            Search::Where where;
            where.ActiveOnly = true;
            where.Mode       = aHere.Mode;
            where.MapId      = aHere.MapId;
            Search::Run(s_Tree.Root(), aQuery, where, s_MenuResult);
            c.Valid = true;
            c.Query = aQuery;
            c.Mode  = aHere.Mode;
            c.MapId = aHere.MapId;
            c.Gen   = s_TreeGen;
            c.Lang  = Localization::Active();
            c.At    = now;
        }
        std::vector<const Node*> found;
        CollectShownMacros(s_Tree.Root(), s_MenuResult, found);
        const bool busy = Sender::IsBusy();
        if (found.empty())
        {
            ImGui::TextDisabled(T("No macro active here matches \"%s\"."), Search::Trim(aQuery).c_str());
            return;
        }
        if (busy) ImGui::TextColored(kWarnColor, T("Sending..."));
        for (size_t i = 0; i < found.size() && i < kMenuResultsMax; i++)
        {
            const Node& n = *found[i];
            const std::string path = MenuPath(n);
            const std::string label = path.empty() ? n.Name : n.Name + "  (" + path + ")";
            ImGui::PushID(static_cast<int>(n.Id ^ (n.Id >> 32)));
            DrawMenuMacro(n, label.c_str(), busy);
            ImGui::PopID();
        }
        if (found.size() > kMenuResultsMax)
            ImGui::TextDisabled(T("...and %zu more: type more to narrow it down."), found.size() - kMenuResultsMax);
        if (aEnter && !busy)
        {
            // The first one that can be sent; if none can (all blocked), the menu stays open, faded.
            const auto first = std::find_if(found.begin(), found.end(), [](const Node* n) { return !IsBlocked(*n); });
            if (first != found.end())
            {
                SendMacro(**first, ESendFrom::Menu);
                ImGui::CloseCurrentPopup();
            }
        }
    }

    // Above the folders: the favourites (on every map; faded where they're not active) and the
    // recently sent macros that are active here.
    void DrawQuickLists(const Here& aHere)
    {
        const QuickLists::Lists ls = QuickLists::Build(s_Tree, s_Recent, s_RecentCount,
                                                       [&](const Node& n) { return IsActiveHere(n, aHere); });
        if (ls.Favourites.empty() && ls.Recent.empty()) return;
        const bool busy = Sender::IsBusy();
        const auto label = [](const Node& n)
        {
            const std::string path = MenuPath(n);
            return path.empty() ? n.Name : n.Name + "  (" + path + ")";
        };
        if (!ls.Favourites.empty())
        {
            ImGui::TextDisabled("%s", T("Favourites"));
            ImGui::PushID(1); // the same macro can be in the tree below too
            for (const QuickLists::Favourite& f : ls.Favourites)
            {
                const Node& n = *f.Item;
                ImGui::PushID(static_cast<int>(n.Id ^ (n.Id >> 32)));
                if (n.IsMacro()) DrawMenuMacro(n, label(n).c_str(), busy, !f.Active);
                else if (BeginFolderMenu(n, label(n).c_str(), !f.Active))
                {
                    if (!QuickLists::HasActive(n, nullptr)) ImGui::TextDisabled("%s", T("No macros in this folder yet."));
                    DrawFavouriteFolderItems(n, aHere);
                    ImGui::EndMenu();
                }
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        if (!ls.Recent.empty())
        {
            ImGui::TextDisabled("%s", T("Recently sent"));
            ImGui::PushID(2);
            for (const Node* n : ls.Recent)
            {
                ImGui::PushID(static_cast<int>(n->Id ^ (n->Id >> 32)));
                DrawMenuMacro(*n, label(*n).c_str(), busy);
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::Separator();
    }

    void DrawQuickMenu(MenuSearch& aSearch)
    {
        const int frame = ImGui::GetFrameCount();
        if (aSearch.LastFrame != frame - 1 && aSearch.LastFrame != frame) aSearch.Buf[0] = '\0';
        aSearch.LastFrame = frame;

        const Here here = WhereAmI();
        // Favourites show on every map, so the menu has something to list whenever one is marked.
        if (!HasActiveMacros(s_Tree.Root(), here) && QuickLists::CountFavourites(s_Tree) == 0)
            ImGui::TextDisabled("%s", s_Tree.Root().Children.empty() ? T("No macros yet.") : T("No macros for this map."));
        else
        {
            if (aSearch.Focus)
            {
                aSearch.Focus = false;
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(UiFont::Px(220.0f));
            const bool enter = ImGui::InputTextWithHint("##menusearch", T("Search (Enter sends the first)"), aSearch.Buf,
                                                        sizeof(aSearch.Buf), ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(T("Finds the macros active here by name, line text, recipient or hotkey.\n"
                                  "Enter sends the first one."));
            const std::string query = aSearch.Buf;
            if (!Search::Trim(query).empty()) DrawMenuResults(here, query, enter);
            else
            {
                if (Sender::IsBusy()) ImGui::TextColored(kWarnColor, T("Sending..."));
                DrawQuickLists(here);
                DrawQuickMenuItems(s_Tree.Root(), here);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(s_ShowMainWindow ? T("Hide macro library") : T("Open macro library")))
            s_ShowMainWindow = !s_ShowMainWindow;
    }

    void DrawQuickMenuPopup()
    {
        if (s_QuickMenuOpenRequest)
        {
            s_QuickMenuOpenRequest = false;
            ImGui::OpenPopup(kQuickMenuPopup); // opens at the mouse pointer
            s_PopupSearch.Focus = true;        // opened by key: you can type straight away
        }
        if (!ImGui::BeginPopup(kQuickMenuPopup))
        {
            s_QuickMenuCloseRequest = false;
            return;
        }
        if (s_QuickMenuCloseRequest)
        {
            s_QuickMenuCloseRequest = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextDisabled("%s", kAddonName);
        ImGui::Separator();
        DrawQuickMenu(s_PopupSearch);
        ImGui::EndPopup();
    }

    // --- Import ----------------------------------------------------------------------------------
    // A source is read into a package, the preview shows what would happen, and only the Import
    // button changes the library. The library as it was before is kept so the import can be undone.

    std::unique_ptr<Exchange::Package> s_ImportPkg;
    Exchange::Options                  s_ImportOpt;
    std::string                        s_ImportPath;
    std::vector<std::filesystem::path> s_ImportList;
    bool                               s_ImportListStale = true;
    // "A whole library" (docs/design.md, "Import / export"): someone's macros.json, put into one
    // new top-level folder (the package's only top-level node), named s_LibraryName.
    bool                               s_ImportLibrary = false;
    std::string                        s_LibraryName;

    std::optional<nlohmann::json> s_UndoImport; // the library before the last import
    std::string                   s_UndoText;
    bool                          s_UndoIsRestore = false; // from a backup (Restore tab), not an import

    void LoadImport(Exchange::Package aPkg, bool aPasted = false)
    {
        s_ImportOpt.PerItem.clear(); // keyed by the old package's nodes
        s_ImportOpt.Unselected.clear();
        s_ImportOpt.KeepHotkeys = false; // every import starts with hotkeys off, even after one that kept them
        if (!aPkg.Ok())
        {
            if (aPkg.Source.empty()) ShowError(T("Opened it in the Import window"), std::string(T("It can't be imported.")) + " " + aPkg.Error);
            else ShowError(Fmt(T("Opened \"%s\" in the Import window"), aPkg.Source.c_str()),
                           Fmt(T("\"%s\" can't be imported."), aPkg.Source.c_str()) + " " + aPkg.Error);
            s_ImportPkg.reset();
            return;
        }
        if (s_ImportLibrary)
        {
            s_LibraryName = aPasted ? std::string(Exchange::LibraryFolderFallback()) : Exchange::LibraryFolderName(aPkg.Source);
            Exchange::WrapInFolder(aPkg, s_LibraryName);
        }
        s_ImportPkg = std::make_unique<Exchange::Package>(std::move(aPkg));
    }

    // Switches between importing macros and importing a whole library. Each starts with its own
    // defaults: hotkeys are off for both (see LoadImport), and in a library a taken folder name is
    // renamed, never merged.
    void SetImportLibrary(bool aOn)
    {
        if (aOn == s_ImportLibrary) return;
        s_ImportLibrary = aOn;
        s_ImportPkg.reset();
        s_ImportOpt = Exchange::Options{};
        if (aOn)
        {
            s_ImportOpt.FolderConflict = Exchange::EChoice::Rename;
        }
        s_ImportListStale = true;
    }

    // The new folder a library goes into (the package's only top-level node).
    Node* LibraryFolder()
    {
        if (!s_ImportLibrary || !s_ImportPkg) return nullptr;
        auto& top = s_ImportPkg->Items.Root().Children;
        return top.size() == 1 && top[0]->IsFolder() ? top[0].get() : nullptr;
    }

    void RefreshImportList()
    {
        s_ImportList.clear();
        s_ImportListStale = false;
        const std::filesystem::path dir = Storage::ImportsDir();
        if (dir.empty()) return;
        std::error_code ec;
        for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            std::error_code ec2;
            std::string ext = PathText::Utf8(it->path().extension());
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const bool dir = it->is_directory(ec2);
            const bool fits = s_ImportLibrary ? !dir && Exchange::IsLibraryFileName(it->path())
                                              : dir || ext == ".json" || ext == ".txt";
            if (fits) s_ImportList.push_back(it->path());
            if (s_ImportList.size() >= 200) break;
        }
        std::sort(s_ImportList.begin(), s_ImportList.end());
    }

    void FolderTargets(const Node& aFolder, std::vector<const Node*>& aOut)
    {
        for (const auto& c : aFolder.Children)
            if (c->IsFolder())
            {
                aOut.push_back(c.get());
                FolderTargets(*c, aOut);
            }
    }

    std::string FolderPath(const Node& aFolder)
    {
        return aFolder.Parent ? PathOf(aFolder) + " > " + aFolder.Name : std::string(T("Top level"));
    }

    bool ChoiceCombo(const char* aLabel, Exchange::EChoice& aChoice, bool aFolder)
    {
        using Exchange::EChoice;
        bool changed = false;
        if (ImGui::BeginCombo(aLabel, Localization::Lookup(Exchange::ChoiceLabel(aChoice))))
        {
            const EChoice folderChoices[] = {EChoice::Merge, EChoice::Rename, EChoice::Skip, EChoice::Replace};
            const EChoice macroChoices[]  = {EChoice::Rename, EChoice::Skip, EChoice::Replace};
            const EChoice* list = aFolder ? folderChoices : macroChoices;
            const size_t count  = aFolder ? 4 : 3;
            for (size_t i = 0; i < count; i++)
                if (ImGui::Selectable(Localization::Lookup(Exchange::ChoiceLabel(list[i])), list[i] == aChoice))
                {
                    aChoice = list[i];
                    changed = true;
                }
            ImGui::EndCombo();
        }
        return changed;
    }

    // One flag after a preview row, with its reason on hover.
    void PlanFlag(const char* aText, const char* aWhy)
    {
        ImGui::SameLine();
        ImGui::TextColored(kWarnColor, "%s", aText);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", aWhy);
    }

    // What to look at before importing a macro: placeholders that were removed, and hotkeys that
    // could fire by accident or clash with the user's own.
    void DrawPlanFlags(const Exchange::PlanItem& aItem)
    {
        using Exchange::EHotkeyRisk;
        if (aItem.UsesRemoved)
            PlanFlag(T("[REMOVED PLACEHOLDER]"), T("It uses {txt} or {json}, which this addon no longer has.\nIt won't be sent until you edit those out."));
        if (aItem.FromNewer)
            PlanFlag(T("[NEWER VERSION]"), T("It has settings from a newer version of the addon. They're kept, but this version can't use them.\n"
                                             "A line for a channel this version doesn't know isn't sent."));
        if (s_ImportOpt.KeepHotkeys && !aItem.HotkeyCleared)
        {
            if (aItem.HotkeyRisk == EHotkeyRisk::NoModifier)
                PlanFlag(T("[KEY WITHOUT CTRL/ALT/SHIFT]"), T("Its hotkey has no Ctrl, Alt or Shift, so it sends the macro whenever you press that key while playing."));
            else if (aItem.HotkeyRisk == EHotkeyRisk::Clash)
                PlanFlag(T("[YOUR HOTKEY]"), T("One of your macros already has this hotkey, and both can be active at once: there, the key would send nothing."));
        }
        if (aItem.HotkeyCleared)
            PlanFlag(T("[COMES WITHOUT ITS HOTKEY]"), T("It replaces one of yours, and its hotkey has no Ctrl, Alt or Shift or is already yours, so it isn't imported."));
        if (aItem.LosesOwnHotkey)
            PlanFlag(T("[YOUR HOTKEY GOES]"), T("The macro it replaces has a hotkey, which is removed. Set one again in the editor if you want it."));
    }

    void DrawPlanItems(const std::vector<Exchange::PlanItem>& aItems)
    {
        using Exchange::EOutcome;
        const ImVec4 newColor{0.55f, 0.90f, 0.55f, 1.0f};
        for (const Exchange::PlanItem& item : aItems)
        {
            const Node& src = *item.Src;
            ImGui::PushID(&src);
            std::string tag;
            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            switch (item.Outcome)
            {
            case EOutcome::New:       tag = T("new"); color = newColor; break;
            case EOutcome::Renamed:   tag = Fmt(T("new, as \"%s\""), item.Name.c_str()); color = newColor; break;
            case EOutcome::Merged:    tag = T("goes into the existing folder"); break;
            case EOutcome::Replaced:  tag = T("replaces the existing one"); color = kWarnColor; break;
            case EOutcome::Skipped:   tag = T("skipped"); color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled); break;
            case EOutcome::TooDeep:   tag = T("left out: folders would be nested more than 6 deep"); color = kErrorColor; break;
            case EOutcome::Flattened: tag = T("folder left out (too deep); its contents move up a level"); color = kWarnColor; break;
            case EOutcome::NotSelected: tag = T("not selected"); color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled); break;
            }
            // Tick to import. A folder brings everything ticked inside it.
            bool on = Exchange::IsSelected(s_ImportOpt, src);
            if (ImGui::Checkbox("##sel", &on)) Exchange::SetSelected(s_ImportOpt, src, on);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", src.IsFolder() ? T("Import this folder with everything ticked inside it.\nUntick to leave the whole folder out.")
                                                       : T("Import this macro."));
            ImGui::SameLine();
            const bool showChildren = src.IsFolder() && !item.Children.empty();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (!showChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | (src.IsMacro() ? ImGuiTreeNodeFlags_Bullet : 0);
            ImGui::PushStyleColor(ImGuiCol_Text, src.IsFolder() ? kFolderColor : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            // A hotkey that comes along is shown as in the library tree, so it can be checked first.
            const bool showKey = s_ImportOpt.KeepHotkeys && src.IsMacro() && !src.Hotkey.Empty();
            const bool open = showKey ? ImGui::TreeNodeEx("##item", flags, "%s  [%s]", src.Name.c_str(), Hotkey::Display(src.Hotkey).c_str())
                                      : ImGui::TreeNodeEx("##item", flags, "%s", src.Name.c_str());
            ImGui::PopStyleColor();
            if (Exchange::IsPartlySelected(s_ImportOpt, src))
            {
                ImGui::SameLine();
                ImGui::TextDisabled(T("[some]"));
            }
            if (item.Conflict && item.Outcome != EOutcome::NotSelected)
            {
                ImGui::SameLine();
                ImGui::TextColored(kCountNearColor, T("NAME TAKEN:"));
            }
            ImGui::SameLine();
            ImGui::TextColored(color, "(%s)", tag.c_str());
            DrawPlanFlags(item);
            if (item.Conflict && item.Outcome != EOutcome::NotSelected)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(Px(170.0f));
                Exchange::EChoice choice = item.Choice;
                if (ChoiceCombo("##choice", choice, src.IsFolder())) s_ImportOpt.PerItem[&src] = choice;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(src.IsFolder() ? T("There's already a folder called \"%s\" there.") : T("There's already a macro called \"%s\" there."),
                                      src.Name.c_str());
            }
            if (showChildren && open)
            {
                DrawPlanItems(item.Children);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    void DrawImportSource()
    {
        ImGui::TextUnformatted(T("1. Choose what to import"));
        // Importing a whole library is an advanced option: while they're off, only macros.
        if (!s_AdvancedOn) SetImportLibrary(false);
        else
        {
            if (ImGui::RadioButton(T("Macros"), !s_ImportLibrary)) SetImportLibrary(false);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("A .json export, a .txt file (one macro), or a folder of .txt files."));
            ImGui::SameLine();
            if (ImGui::RadioButton(T("A whole library"), s_ImportLibrary)) SetImportLibrary(true);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(T("Someone else's macros.json (or macros.json.bak), as they sent it to you.\n"
                                  "It goes into a new folder of its own, so it never mixes with your folders."));
        }
        if (s_ImportLibrary)
            ImGui::TextDisabled(T("Someone's macros.json or macros.json.bak. Rename it (e.g. Alex.json) so it's easy to tell apart. "
                                "It goes into a new folder at the top level. Their trash bin and text file links aren't taken."));
        else ImGui::TextDisabled(T("A .json export, a .txt file (one macro), or a folder of .txt files."));
        if (s_ImportListStale) RefreshImportList();

        ImGui::BeginChild("##imports", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 5.0f), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushTextWrapPos(0.0f);
        if (s_ImportList.empty())
            ImGui::TextDisabled(T("Put files in <GW2>\\addons\\GW2_Chat_Messages\\imports\\ to see them here."));
        for (const std::filesystem::path& p : s_ImportList)
        {
            std::error_code ec;
            const bool dir = std::filesystem::is_directory(p, ec);
            const std::string label = PathText::Utf8(p.filename()) + (dir ? "\\" : "");
            if (ImGui::Selectable(label.c_str())) LoadImport(s_ImportLibrary ? Exchange::FromLibraryFile(p) : Exchange::FromPath(p));
        }
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        UiFont::Row row;
        if (ImGui::SmallButton(T("Open imports folder"))) Storage::OpenFolder(Storage::ImportsDir());
        row.Next(UiFont::ButtonWidth(T("Refresh")));
        if (ImGui::SmallButton(T("Refresh"))) s_ImportListStale = true;
        row.Next(UiFont::ButtonWidth(T("Paste from clipboard")));
        if (ImGui::SmallButton(T("Paste from clipboard")))
        {
            const char* text = ImGui::GetClipboardText();
            if (!text || !*text) ShowError(T("Clicked Paste from clipboard in the Import window"), T("The clipboard has no text."));
            else
            {
                Exchange::Package pkg = s_ImportLibrary ? Exchange::FromJsonText(text) : Exchange::FromText(T("Pasted macro"), text);
                pkg.Source = T("Pasted text");
                LoadImport(std::move(pkg), true);
            }
        }

        ImGui::SetNextItemWidth(-(UiFont::ButtonWidth(T("Load")) + ImGui::GetStyle().ItemSpacing.x));
        const bool enter = InputString("##path", s_ImportLibrary ? T("Or a full path to the file") : T("Or a full path to a file or folder"),
                                       s_ImportPath, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button(T("Load")) || enter) && !s_ImportPath.empty())
        {
            std::string path = s_ImportPath;
            if (path.size() >= 2 && path.front() == '"' && path.back() == '"') path = path.substr(1, path.size() - 2); // "Copy as path"
            const std::filesystem::path p = PathText::FromUtf8(path);
            LoadImport(s_ImportLibrary ? Exchange::FromLibraryFile(p) : Exchange::FromPath(p));
        }
    }

    void CloseImport()
    {
        s_ShowImport = false;
        s_ImportPkg.reset();
        s_ImportOpt.PerItem.clear();
        s_ImportOpt.Unselected.clear();
        SetImportLibrary(false); // the window opens on "Macros" next time
        s_ImportListStale = true;
    }

    // --- The Export window -------------------------------------------------------------------------
    // Tick folders and macros, then save them in one .json file or copy them to the clipboard.
    // Ticking never changes the library (the ticks are ids, kept apart from the tree).

    void DrawExportItems(const Node& aFolder)
    {
        for (const auto& child : aFolder.Children)
        {
            const Node& n = *child;
            ImGui::PushID(static_cast<int>(n.Id ^ (n.Id >> 32)));
            ImGui::PushID(static_cast<int>(n.Id & 0xFFFFFFFF));
            bool on = Exchange::IsPicked(s_ExportPick, n);
            if (ImGui::Checkbox("##pick", &on)) Exchange::SetPicked(s_ExportPick, n, on);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", n.IsFolder() ? T("Export this folder with everything ticked inside it.")
                                                     : T("Export this macro."));
            ImGui::SameLine();
            const bool hasChildren = n.IsFolder() && !n.Children.empty();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
            if (hasChildren) flags |= ImGuiTreeNodeFlags_DefaultOpen;
            else flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | (n.IsMacro() ? ImGuiTreeNodeFlags_Bullet : 0);
            const EChannel first = n.Lines.empty() ? EChannel::Current : n.Lines.front().Channel;
            ImGui::PushStyleColor(ImGuiCol_Text, n.IsFolder() ? kFolderColor : ChannelColorVec(first));
            const bool open = ImGui::TreeNodeEx("##item", flags, "%s", n.Name.c_str());
            ImGui::PopStyleColor();
            if (Exchange::IsPartlyPicked(s_ExportPick, n))
            {
                ImGui::SameLine();
                ImGui::TextDisabled(T("[some]"));
            }
            if (hasChildren && open)
            {
                DrawExportItems(n);
                ImGui::TreePop();
            }
            ImGui::PopID();
            ImGui::PopID();
        }
    }

    // Saves the ticked items to <addon>\exports\ (never overwriting a file) or copies them.
    void ExportPicked(bool aClipboard)
    {
        const char* action = aClipboard ? T("Clicked Copy to the clipboard in the Export window")
                                        : T("Clicked Save as a .json file in the Export window");
        std::string text;
        try
        {
            text = Exchange::ExportPickedText(s_Tree, s_ExportPick);
        }
        catch (const std::exception& e) // an exception must never leave the render callback
        {
            LogF(LOGL_WARNING, "Could not make the export: %s", e.what());
            ShowError(action, T("The export couldn't be written. Check the Nexus log."));
            return;
        }
        if (aClipboard)
        {
            ImGui::SetClipboardText(text.c_str());
            s_ExportDone = WithTrashNote(T("Copied to the clipboard. Paste it into the Import window to import it."));
            SetInfo(s_ExportDone);
            return;
        }
        const std::filesystem::path dir = Storage::ExportsDir();
        if (dir.empty())
        {
            ShowError(action, T("The exports folder isn't available."));
            return;
        }
        const std::string name = Tree::CleanName(s_ExportName).empty() ? std::string("Chat Messages selection") : s_ExportName;
        const std::filesystem::path file = Storage::FreeFile(dir, Exchange::FileNameFor(name, ".json"));
        if (!Storage::WriteText(file, text))
        {
            ShowError(action, T("The export couldn't be written. Check the Nexus log."));
            return;
        }
        s_ExportDone = WithTrashNote(Fmt(T("Exported to %s."), ("<GW2>\\addons\\GW2_Chat_Messages\\exports\\" + PathText::Utf8(file.filename())).c_str()));
        SetInfo(s_ExportDone, true);
    }

    void DrawExportWindow()
    {
        if (!s_ShowExport) return;
        UiFont::BeforeBegin(s_ExportFit, ImVec2(520.0f, 560.0f), ImVec2(380.0f, 320.0f));
        const bool shown = ImGui::Begin(T("Export macros###chatmsg_export"), &s_ShowExport,
                                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar);
        UiFont::AfterBegin(s_ExportFit);
        if (!shown)
        {
            ImGui::End();
            return;
        }
        ImGui::PushTextWrapPos(0.0f); // long text wraps at the window's edge (popped before End)

        ImGui::TextDisabled(T("Tick what to export. A folder's tick brings everything ticked inside it."));
        UiFont::Row selRow;
        if (ImGui::SmallButton(T("Select all"))) Exchange::PickAll(s_ExportPick, s_Tree, true);
        selRow.Next(UiFont::ButtonWidth(T("Deselect all")));
        if (ImGui::SmallButton(T("Deselect all"))) Exchange::PickAll(s_ExportPick, s_Tree, false);
        size_t folders = 0, macros = 0;
        Exchange::CountPicked(s_ExportPick, s_Tree, folders, macros);
        ImGui::Text(T("%zu folder(s) and %zu macro(s) ticked."), folders, macros);
        const std::string note = TrashNotExportedText();
        if (!note.empty()) ImGui::TextColored(kWarnColor, "%s", note.c_str());

        const ImGuiStyle& st = ImGui::GetStyle();
        const float footer = ImGui::GetFrameHeightWithSpacing() * 2.0f + st.ItemSpacing.y +
                             (s_ExportDone.empty() ? 0.0f : ImGui::GetTextLineHeightWithSpacing() * 2.0f);
        ImGui::BeginChild("##exportpick", ImVec2(0.0f, -footer), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (s_Tree.Root().Children.empty()) ImGui::TextDisabled("%s", T("There's nothing to export yet."));
        DrawExportItems(s_Tree.Root());
        ImGui::EndChild();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("File name:"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        InputString("##exportname", "Chat Messages selection", s_ExportName);

        const bool any = folders + macros > 0;
        if (!any) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, st.Alpha * 0.5f);
        UiFont::Row row;
        if (ImGui::Button(T("Save as a .json file")) && any) ExportPicked(false);
        row.Next(UiFont::ButtonWidth(T("Copy to the clipboard")));
        if (ImGui::Button(T("Copy to the clipboard")) && any) ExportPicked(true);
        if (!any) ImGui::PopStyleVar();
        row.Next(UiFont::ButtonWidth(T("Exports folder")));
        if (ImGui::Button(T("Exports folder"))) Storage::OpenFolder(Storage::ExportsDir());
        row.Next(UiFont::ButtonWidth(T("Close")));
        if (ImGui::Button(T("Close"))) s_ShowExport = false;
        if (!s_ExportDone.empty()) ImGui::TextWrapped("%s", s_ExportDone.c_str());

        ImGui::PopTextWrapPos();
        ImGui::End();
    }

    void DrawImportWindow()
    {
        if (!s_ShowImport)
        {
            if (s_ImportPkg) CloseImport();
            return;
        }
        UiFont::BeforeBegin(s_ImportFit, ImVec2(640.0f, 600.0f), ImVec2(460.0f, 360.0f));
        const bool shown = ImGui::Begin(T("Import macros###chatmsg_import"), &s_ShowImport,
                                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar);
        UiFont::AfterBegin(s_ImportFit);
        if (!shown)
        {
            ImGui::End();
            return;
        }
        ImGui::PushTextWrapPos(0.0f); // long text wraps at the window's edge (popped before End)

        DrawImportSource();
        if (!s_ImportPkg)
        {
            ImGui::PopTextWrapPos();
            ImGui::End();
            return;
        }

        ImGui::Separator();
        Node* target = nullptr;
        if (Node* folder = LibraryFolder())
        {
            // A library always goes into its own new folder at the top level.
            ImGui::Text(T("2. Name the new folder it goes into (from %s)"), s_ImportPkg->Source.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (InputString("##libname", Exchange::LibraryFolderFallback(), s_LibraryName))
            {
                const std::string clean = Tree::CleanName(s_LibraryName);
                folder->Name = clean.empty() ? std::string(Exchange::LibraryFolderFallback()) : clean;
            }
            ImGui::TextDisabled(T("It's added at the top level of your library. If that name is taken, it gets \" (2)\"."));
            target = &s_Tree.Root();
        }
        else
        {
            ImGui::Text(T("2. Choose where it goes (from %s)"), s_ImportPkg->Source.c_str());
            target = NodeOrRoot(s_ImportTarget);
            if (!target || !target->IsFolder()) { s_ImportTarget = 0; target = &s_Tree.Root(); }
            ImGui::SetNextItemWidth(-(UiFont::TextWidth(T("Import into")) + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f));
            if (ImGui::BeginCombo(T("Import into"), FolderPath(*target).c_str()))
            {
                if (ImGui::Selectable(T("Top level"), target == &s_Tree.Root())) s_ImportTarget = 0;
                std::vector<const Node*> folders;
                FolderTargets(s_Tree.Root(), folders);
                for (const Node* f : folders)
                {
                    ImGui::PushID(f);
                    if (ImGui::Selectable(FolderPath(*f).c_str(), f == target)) s_ImportTarget = f->Id;
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            target = NodeOrRoot(s_ImportTarget);
            if (!target) target = &s_Tree.Root();

            ImGui::SetNextItemWidth(Px(170.0f));
            ChoiceCombo(T("When a macro's name is taken"), s_ImportOpt.MacroConflict, false);
            ImGui::SetNextItemWidth(Px(170.0f));
            ChoiceCombo(T("When a folder's name is taken"), s_ImportOpt.FolderConflict, true);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(T("Merge: the folder's contents go into the existing folder.\nReplace: the existing folder's contents are deleted first."));
        }
        ImGui::Checkbox(T("Move the contents of too-deep folders up a level"), &s_ImportOpt.Flatten);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Folders can be nested at most %d deep. Unticked, folders that would go deeper are left out."), kMaxFolderDepth);
        // Keep hotkeys is an advanced option: while they're off, imports never bring hotkeys.
        if (!s_AdvancedOn) s_ImportOpt.KeepHotkeys = false;
        else
        {
            ImGui::Checkbox(T("Keep hotkeys"), &s_ImportOpt.KeepHotkeys);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", s_ImportLibrary ? T("Off by default for someone else's library, so their keys don't clash with yours.")
                                                        : T("Off at first, so macros from someone else come without their hotkeys. Tick it for your own exports; the preview then shows each macro's hotkey."));
        }
        for (const std::string& w : s_ImportPkg->Warnings) ImGui::TextColored(kWarnColor, "%s", w.c_str());

        // Built every frame against the live library, so the preview is always what Import does.
        const Exchange::Plan plan = Exchange::BuildPlan(s_Tree, *target, s_ImportPkg->Items, s_ImportOpt);

        ImGui::Separator();
        ImGui::TextUnformatted(T("3. Check the preview"));
        std::string summary = Fmt(T("%zu new macro(s), %zu new folder(s)"), plan.NewMacros, plan.NewFolders);
        if (plan.Merged)   summary += ", " + Fmt(T("%zu merged"), plan.Merged);
        if (plan.Replaced) summary += ", " + Fmt(T("%zu replaced"), plan.Replaced);
        if (plan.Skipped)  summary += ", " + Fmt(T("%zu skipped"), plan.Skipped);
        if (plan.TooDeep)  summary += ", " + Fmt(T("%zu left out (too deep)"), plan.TooDeep);
        if (plan.NotSelected) summary += ", " + Fmt(T("%zu not selected"), plan.NotSelected);
        ImGui::TextWrapped("%s.", summary.c_str());
        if (plan.Conflicts)
            ImGui::TextColored(kCountNearColor, T("%zu name(s) are already taken (marked NAME TAKEN): choose what happens to each one below."), plan.Conflicts);
        if (plan.UsesRemoved)
            ImGui::TextColored(kWarnColor, T("%zu macro(s) use {txt} or {json}, which were removed (marked REMOVED PLACEHOLDER): they won't be sent until you edit those out."), plan.UsesRemoved);
        if (plan.FromNewer)
            ImGui::TextColored(kWarnColor, T("%zu item(s) have settings from a newer version of the addon (marked NEWER VERSION): they're kept, but not used here."),
                               plan.FromNewer);
        if (s_ImportOpt.KeepHotkeys && plan.HotkeysNoModifier)
            ImGui::TextColored(kWarnColor, T("%zu hotkey(s) have no Ctrl, Alt or Shift (marked KEY WITHOUT CTRL/ALT/SHIFT): they send their macro whenever you press that key while playing."),
                               plan.HotkeysNoModifier);
        if (s_ImportOpt.KeepHotkeys && plan.HotkeysClash)
            ImGui::TextColored(kWarnColor, T("%zu hotkey(s) are already yours (marked YOUR HOTKEY): where both macros can be active, that key would send nothing."),
                               plan.HotkeysClash);
        if (plan.HotkeysCleared)
            ImGui::TextColored(kWarnColor, T("%zu replacing macro(s) come without their hotkey (marked COMES WITHOUT ITS HOTKEY): it has no Ctrl, Alt or Shift, or is already yours."), plan.HotkeysCleared);
        if (plan.OwnHotkeysLost)
            ImGui::TextColored(kWarnColor, T("%zu of your macros lose their hotkey when replaced (marked YOUR HOTKEY GOES)."), plan.OwnHotkeysLost);
        ImGui::TextDisabled(T("Tick what to import. A folder's tick brings everything inside it."));
        UiFont::Row selRow;
        if (ImGui::SmallButton(T("Select all"))) Exchange::SelectAll(s_ImportOpt, s_ImportPkg->Items, true);
        selRow.Next(UiFont::ButtonWidth(T("Deselect all")));
        if (ImGui::SmallButton(T("Deselect all"))) Exchange::SelectAll(s_ImportOpt, s_ImportPkg->Items, false);

        const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        ImGui::BeginChild("##preview", ImVec2(0.0f, -footer), true, ImGuiWindowFlags_HorizontalScrollbar);
        DrawPlanItems(plan.Items); // one row per item; long rows scroll sideways
        ImGui::EndChild();

        const bool canImport = plan.Changes() > 0;
        if (!canImport) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        if (ImGui::Button(T("Import selected")) && canImport)
        {
            s_UndoImport = s_Tree.ToJson();
            s_UndoIsRestore = false;
            size_t n = 0;
            bool applied = true;
            try { n = Exchange::Apply(s_Tree, *target, plan, s_ImportOpt); }
            catch (const std::exception&) { applied = false; } // out of memory, say (SEC-025)
            if (!applied)
            {
                // All or nothing: the library goes back to how it was, as Undo import does.
                std::vector<std::string> ignored;
                s_Tree.FromJson(*s_UndoImport, &ignored);
                s_UndoImport.reset();
                s_SelectedId = 0;
                s_TrashSelId = 0;
                ShowError(T("Clicked Import selected in the Import window"),
                          T("There wasn't enough memory to import it. Nothing was imported."));
            }
            else
            {
                s_UndoText = Fmt(T("Imported %zu item(s) from %s into %s."), n, s_ImportPkg->Source.c_str(), FolderPath(*target).c_str());
                if (plan.HotkeysChanged)
                    s_UndoText += " " + Fmt(T("%zu replaced macro(s) have no hotkey now, or a different one: check them in the editor."), plan.HotkeysChanged);
            }
            s_RawForId = 0; // a replaced macro's raw text is stale
            CancelCapture();
            MarkDirty();
            s_ShowMainWindow = true;
            CloseImport();
        }
        if (!canImport) ImGui::PopStyleVar();
        if (!canImport && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", plan.NotSelected ? T("Nothing ticked would be added or replaced.") : T("Nothing would be added or replaced."));
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel"))) CloseImport();

        ImGui::PopTextWrapPos();
        ImGui::End();
    }

    // Shown in the library window after an import, until dismissed.
    void DrawUndoImport()
    {
        if (!s_UndoImport) return;
        ImGui::TextWrapped("%s", s_UndoText.c_str());
        UiFont::Row row;
        if (ImGui::SmallButton(s_UndoIsRestore ? T("Undo restore") : T("Undo import")))
        {
            std::vector<std::string> ignored;
            s_Tree.FromJson(*s_UndoImport, &ignored);
            s_UndoImport.reset();
            s_SelectedId = 0;
            s_TrashSelId = 0; // the bin goes back to how it was before the import too
            s_RawForId   = 0;
            CancelCapture();
            MarkDirty();
            SetInfo(s_UndoIsRestore ? T("The restore was undone.") : T("The import was undone."));
            return;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", s_UndoIsRestore ? T("Puts your library back as it was before the restore.\nChanges you made since then are undone too.")
                                                    : T("Puts your library back as it was before the import.\nChanges you made since then are undone too."));
        row.Next(UiFont::ButtonWidth(T("Dismiss##undo")));
        if (ImGui::SmallButton(T("Dismiss##undo"))) s_UndoImport.reset();
        ImGui::Separator();
    }

    void DrawToolbar()
    {
        Node& into = InsertFolder();
        const bool canNest = into.Parent == nullptr || Tree::FolderLevel(into) < kMaxFolderDepth;
        UiFont::Row row; // buttons that don't fit continue on the next line
        if (ImGui::Button(T("New folder")))
        {
            if (canNest) OpenNamePopup(ENameMode::NewFolder, into.Id);
            else ShowError(Fmt(T("Clicked New folder with \"%s\" selected"), into.Name.c_str()),
                           Fmt(T("Folders can be nested at most %d levels deep."), static_cast<int>(kMaxFolderDepth)));
        }
        row.Next(UiFont::ButtonWidth(T("New macro")));
        if (ImGui::Button(T("New macro"))) OpenNamePopup(ENameMode::NewMacro, into.Id);
        row.Next(UiFont::ButtonWidth(T("Expand all")));
        if (ImGui::Button(T("Expand all")))   s_Pending = {EAction::ExpandAll, 0, 0, EDrop::Into, 0, ENameMode::NewFolder, true};
        row.Next(UiFont::ButtonWidth(T("Collapse all")));
        if (ImGui::Button(T("Collapse all"))) s_Pending = {EAction::ExpandAll, 0, 0, EDrop::Into, 0, ENameMode::NewFolder, false};
        row.Next(UiFont::ButtonWidth(T("Sort A-Z")));
        if (ImGui::Button(T("Sort A-Z")))
        {
            s_Tree.SortChildren(s_Tree.Root(), true);
            MarkDirty();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Sorts every folder: folders first, then macros, each A-Z."));
        row.Next(UiFont::ButtonWidth(T("Import...")));
        if (ImGui::Button(T("Import...")))
        {
            s_ImportTarget = InsertFolder().Parent ? InsertFolder().Id : 0;
            s_ShowImport   = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Import exported macros or .txt files,\nor a whole library (macros.json) someone sent you."));
        row.Next(UiFont::ButtonWidth(T("Export all")));
        if (ImGui::Button(T("Export all")))
        {
            if (s_Tree.Root().Children.empty()) ShowError(T("Clicked Export all"), T("There's nothing to export yet."));
            else ExportToFile(TopLevelNodes(), "Chat Messages library", false);
        }
        if (ImGui::IsItemHovered())
        {
            const std::string note = TrashNotExportedText();
            ImGui::SetTooltip(T("Saves your whole library to the exports folder.\nRight-click a folder or macro to export just that.%s%s"),
                              note.empty() ? "" : "\n", note.c_str());
        }
        row.Next(UiFont::ButtonWidth(T("Export some...")));
        if (ImGui::Button(T("Export some...")))
            OpenExport(s_SelectedId ? s_Tree.Find(s_SelectedId) : nullptr); // the selected item starts ticked
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Tick the folders and macros you want, and save them in one file."));
        row.Next(UiFont::ButtonWidth(T("Exports folder")));
        if (ImGui::Button(T("Exports folder"))) Storage::OpenFolder(Storage::ExportsDir());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Opens <GW2>\\addons\\GW2_Chat_Messages\\exports\\, where exports are saved."));
        row.Next(UiFont::ComboWidth(T("Text size")));
        UiFont::DrawCombo(T("Text size"));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("The size of the text in this addon's windows.\nThe sizes are the fonts Nexus provides."));
        ImGui::TextDisabled(T("New items go into: %s"), into.Parent ? into.Name.c_str() : T("Top level"));
    }

    void DrawMessages()
    {
        if (s_SaveFailed) // stays until a save works (the error window said why)
            ImGui::TextColored(kErrorColor, T("Your macros couldn't be saved. Check the Nexus log. Retrying..."));
        if (s_Tree.FromNewerVersion()) // stays: the file is never saved over (Storage::SaveMacros)
            ImGui::TextColored(kWarnColor, T("Read-only: your macros were saved by a newer version of the addon. "
                                             "You can send them, but changes aren't saved. Update the addon to edit them."));
        if (!s_Status.empty() && ImGui::GetTime() < s_StatusUntil)
        {
            ImGui::TextWrapped("%s", s_Status.c_str());
            if (s_StatusExport && ImGui::SmallButton(T("Open exports folder"))) Storage::OpenFolder(Storage::ExportsDir());
        }
        DrawUndoImport();
    }

    // The search box at the top of the tree pane, aWidth wide (with its x button).
    void DrawSearchBox(float aWidth)
    {
        const bool hasText = s_SearchBuf[0] != '\0';
        const float clearW = hasText ? UiFont::ButtonWidth(T("x##search")) + ImGui::GetStyle().ItemSpacing.x : 0.0f;
        if (s_SearchFocus)
        {
            ImGui::SetKeyboardFocusHere();
            s_SearchFocus = false;
        }
        ImGui::SetNextItemWidth(std::max(aWidth - clearW, Px(60.0f)));
        ImGui::InputTextWithHint("##search", T("Search (Ctrl+F)"), s_SearchBuf, sizeof(s_SearchBuf));
        // Escape leaves the box; it also clears it.
        if (ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false)) s_SearchBuf[0] = '\0';
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Finds folders and macros by name, by the text of their lines, a whisper's\n"
                              "recipient, or their hotkey (e.g. ctrl+f1). Case and accents don't matter.\n"
                              "While searching, dragging and Move up/down are off."));
        if (hasText)
        {
            ImGui::SameLine();
            if (ImGui::Button(T("x##search"))) s_SearchBuf[0] = '\0';
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Clear the search (or press Escape in the box)."));
        }
    }

    // Works out the search when its text, the filter, where you are, the library or the language
    // changed (and twice a second anyway), not every frame: it folds every name and line. The
    // tree's row labels are built again with it. A selected item that's no longer shown is
    // unselected.
    void UpdateSearch()
    {
        const Here here = WhereAmI();
        const double now = ImGui::GetTime();
        SearchCache& c = s_SearchCache;
        const bool same = c.Valid && c.Query == s_SearchBuf && c.ActiveOnly == s_ActiveOnly && c.Mode == here.Mode &&
                          c.MapId == here.MapId && c.Gen == s_TreeGen && c.Lang == Localization::Active() &&
                          now >= c.At && now - c.At < kCacheRefreshSec;
        if (!same)
        {
            Search::Where where;
            where.ActiveOnly = s_ActiveOnly;
            where.Mode       = here.Mode;
            where.MapId      = here.MapId;
            Search::Run(s_Tree.Root(), s_SearchBuf, where, s_Search);
            c.Valid      = true;
            c.Query      = s_SearchBuf;
            c.ActiveOnly = s_ActiveOnly;
            c.Mode       = here.Mode;
            c.MapId      = here.MapId;
            c.Gen        = s_TreeGen;
            c.Lang       = Localization::Active();
            c.At         = now;
            s_RowLabels.clear();
        }
        if (s_Search.Active && s_SelectedId && !s_Search.Shows(s_SelectedId)) s_SelectedId = 0;
    }

    // The filter checkbox says where "here" is: "Only active in WvW on Eternal Battlegrounds (38)".
    std::string ActiveOnlyLabel()
    {
        const Here h = WhereAmI();
        if (h.Mode == GM_None) return T("Only active here (no game mode right now: only macros active everywhere)");
        return Fmt(T("Only active in %s on %s"), ModeName(h.Mode), (h.MapId ? MapNames::Label(h.MapId) : std::string(T("this map"))).c_str());
    }

    // At the top of the tree pane: the filter checkbox and what the search and filter found.
    void DrawSearchSummary()
    {
        if (ImGui::Checkbox(ActiveOnlyLabel().c_str(), &s_ActiveOnly))
        {
            if (!Storage::SaveSetting(kActiveOnlySetting, s_ActiveOnly)) LogF(LOGL_WARNING, "The \"only active here\" setting couldn't be saved.");
            UpdateSearch(); // this frame's tree already uses the new choice
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Shows only the macros your hotkeys and the quick menu would use where you are now\n"
                              "(see \"Where it's active\" in the editor). It follows you as you change maps.\n"
                              "While it's on, dragging and Move up/down are off."));
        if (s_Tree.Root().Children.empty() || !s_Search.Active) return;
        const std::string text = Search::Trim(s_SearchBuf);
        if (!text.empty())
        {
            if (s_Search.Visible.empty())
                ImGui::TextDisabled(s_ActiveOnly ? T("Nothing active here matches \"%s\".") : T("Nothing matches \"%s\"."), text.c_str());
            else if (s_ActiveOnly)
                ImGui::TextDisabled(T("Found: %zu folder(s), %zu macro(s), active here"), s_Search.FolderMatches, s_Search.MacroMatches);
            else
                ImGui::TextDisabled(T("Found: %zu folder(s), %zu macro(s)"), s_Search.FolderMatches, s_Search.MacroMatches);
        }
        else if (s_Search.Visible.empty()) ImGui::TextDisabled(T("No macros are active here."));
        else ImGui::TextDisabled(T("%zu macro(s) active here"), s_Search.ShownMacros);
    }

    void DrawMainWindow()
    {
        std::fill(std::begin(s_TourSeen), std::end(s_TourSeen), false); // marked again while drawing
        UiFont::BeforeBegin(s_MainFit, ImVec2(820.0f, 500.0f), ImVec2(560.0f, 300.0f));
        const bool shown = ImGui::Begin(Ui::kMainWindowName, &s_ShowMainWindow, ImGuiWindowFlags_NoCollapse);
        UiFont::AfterBegin(s_MainFit);
        if (!shown)
        {
            ImGui::End();
            return;
        }
        ImGui::PushTextWrapPos(0.0f); // long text wraps at the window's edge

        // Ctrl+F anywhere in the library window puts the cursor in the search box.
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl && !io.KeyAlt &&
            ImGui::IsKeyPressed('F', false))
            s_SearchFocus = true;

        DrawMessages();
        ImGui::BeginGroup();
        DrawToolbar();
        ImGui::EndGroup();
        TourMark(Welcome::ETarget::Toolbar);
        ImGui::Separator();

        // Both panes scroll sideways too, for long names and rows that can't wrap.
        // The search box sits above the tree; the group keeps the details pane level with it.
        const float treeWidth = ImGui::GetContentRegionAvail().x * 0.38f;
        UpdateSearch();
        ImGui::BeginGroup();
        ImGui::BeginGroup();
        DrawSearchBox(treeWidth);
        ImGui::EndGroup();
        TourMark(Welcome::ETarget::Search);
        ImGui::BeginChild("##tree", ImVec2(treeWidth, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushTextWrapPos(0.0f);
        DrawSearchSummary();
        if (s_Tree.Root().Children.empty())
        {
            ImGui::TextWrapped(T("Your library is empty. Create a folder or a macro to get started."));
        }
        DrawTree();
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        TourMark(Welcome::ETarget::Tree);
        ImGui::EndGroup();

        ImGui::SameLine();
        ImGui::BeginChild("##details", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushTextWrapPos(0.0f);
        DrawDetails();
        ImGui::PopTextWrapPos();
        ImGui::EndChild();

        ApplyPending();
        DrawNamePopup();
        DrawDeletePopup();
        DrawRestorePopup();
        DrawPurgePopup();
        HandleExamplesRequest();
        DrawExamplesReplacePopup();
        DrawExamplesAgainPopup();
        HandleWelcomeRequest();
        DrawWelcomePopup();

        ImGui::PopTextWrapPos();
        ImGui::End();
    }

    // --- First-run welcome (docs/design.md, "First-run welcome and tour") ---------------------

    NexusLinkData_t* s_NexusLink = nullptr;
    bool InGameplay() { return s_NexusLink && s_NexusLink->IsGameplay; } // not the loading screen or character select

    bool s_WelcomeShown   = false; // settings.json "welcome_shown"
    bool s_WelcomePending = false; // a first run: open the library and the welcome once in gameplay
    bool s_WelcomeRequest = false; // open the pop-up (at the library window's ID level)
    int  s_WelcomePage    = 0;     // 0: the examples question, 1: the tour

    void NoteWelcomeShown()
    {
        if (s_WelcomeShown) return;
        s_WelcomeShown = true;
        if (!Storage::SaveSetting(Welcome::kShownSetting, true)) LogF(LOGL_WARNING, "The \"welcome shown\" setting couldn't be saved.");
    }

    void HandleWelcomeRequest()
    {
        if (!s_WelcomeRequest) return;
        s_WelcomeRequest = false;
        s_WelcomePage    = 0;
        ImGui::OpenPopup(kWelcomePopup);
    }

    // Two pages: the example macros (Add / Start empty), then the tour (Take the tour / Not now).
    // "welcome_shown" is saved with the first answer; closing the game before it asks again.
    void DrawWelcomePopup()
    {
        ImGui::SetNextWindowSize(ImVec2(Px(460.0f), 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(T(kWelcomePopup), nullptr, ImGuiWindowFlags_NoResize)) return;
        ImGui::PushTextWrapPos(0.0f);
        if (s_WelcomePage == 0)
        {
            ImGui::TextWrapped(T("Chat Messages keeps your chat macros: one or more lines, each typed into its own chat "
                               "channel, sorted into folders."));
            ImGui::Spacing();
            ImGui::TextWrapped(T("Add some example folders and macros to start with? They show several lines, channels, "
                               "a whisper, placeholders and filters, and have no hotkeys. You can change or delete them, "
                               "and add them again later from the addon's options page."));
            ImGui::Spacing();
            UiFont::Row row;
            if (ImGui::Button(T("Add the examples")))
            {
                NoteWelcomeShown();
                if (Node* ex = Examples::Add(s_Tree))
                {
                    ShowNewExamples(*ex, T("Added the example macros in a new Examples folder."));
                    LogF(LOGL_INFO, "First run: added the example macros.");
                }
                else ShowError(T("Clicked \"Add the examples\" in the welcome"), T("The example macros couldn't be added."));
                s_WelcomePage = 1;
            }
            row.Next(UiFont::ButtonWidth(T("Start empty")));
            if (ImGui::Button(T("Start empty")))
            {
                NoteWelcomeShown();
                s_WelcomePage = 1;
            }
        }
        else
        {
            ImGui::TextWrapped(T("Take a short tour of the library window? It points at each part, one step at a time "
                               "(%zu steps). You can stop at any time, and take it again from the addon's options page."),
                               Welcome::Steps().size());
            ImGui::Spacing();
            UiFont::Row row;
            if (ImGui::Button(T("Take the tour")))
            {
                StartTour();
                ImGui::CloseCurrentPopup();
            }
            row.Next(UiFont::ButtonWidth(T("Not now")));
            if (ImGui::Button(T("Not now"))) ImGui::CloseCurrentPopup();
        }
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    // The first macro in a folder (depth first) that the search box lets the tree show, for the
    // tour's editor steps (the library unselects a macro the search hides).
    Node* FirstMacroIn(Node& aFolder)
    {
        for (const auto& c : aFolder.Children)
        {
            if (c->IsMacro())
            {
                if (!s_Search.Active || s_Search.Shows(c->Id)) return c.get();
            }
            else if (Node* m = FirstMacroIn(*c)) return m;
        }
        return nullptr;
    }

    // A step about the editor selects a macro when none is: the first example ("Hello"), else the
    // first macro in the library. With none at all, the step explains it in words.
    void EnterTourStep()
    {
        s_TourEntered = false;
        const Welcome::Step& st = Welcome::Steps()[s_TourStep];
        s_TourScrollTo = st.Target;
        if (!Welcome::NeedsMacro(st.Target)) return;
        const Node* sel = s_Tree.Find(s_SelectedId);
        if (sel && sel->IsMacro()) return;
        Node* m = nullptr;
        if (Node* ex = Examples::Find(s_Tree)) m = FirstMacroIn(*ex);
        if (!m) m = FirstMacroIn(s_Tree.Root());
        if (!m) return;
        s_SelectedId = m->Id;
        s_TrashSelId = 0;
        bool expanded = false;
        for (Node* p = m->Parent; p && p->Parent; p = p->Parent)
            if (!p->Expanded) { p->Expanded = true; expanded = true; }
        if (expanded) MarkDirty(); // folders' open state is saved
    }

    // The step's box, and an outline around the part it's about. Drawn after the library window
    // (so the marks are this frame's); hidden while a pop-up is open; closing the library ends it.
    void DrawTour()
    {
        if (!s_TourOn) return;
        const auto& steps = Welcome::Steps();
        if (!s_ShowMainWindow || s_TourStep >= steps.size())
        {
            s_TourOn = false;
            return;
        }
        if (s_TourEntered) EnterTourStep();
        const ImGuiWindow* main = ImGui::FindWindowByName(Ui::kMainWindowName);
        if (!main || !main->Active || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup)) return;

        const Welcome::Step& st = steps[s_TourStep];
        const size_t ti  = static_cast<size_t>(st.Target);
        const bool   has = ti > 0 && ti < kTourTargets && s_TourSeen[ti];
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        const ImVec2 wMin = main->Pos;
        const ImVec2 wMax(main->Pos.x + main->Size.x, main->Pos.y + main->Size.y);
        ImVec2 tMin, tMax;
        if (has)
        {
            // Only the part inside the library window (a pane can scroll it out of view).
            tMin = ImVec2(std::clamp(s_TourMin[ti].x, wMin.x, wMax.x), std::clamp(s_TourMin[ti].y, wMin.y, wMax.y));
            tMax = ImVec2(std::clamp(s_TourMax[ti].x, wMin.x, wMax.x), std::clamp(s_TourMax[ti].y, wMin.y, wMax.y));
            const float pad = Px(4.0f);
            ImGui::GetForegroundDrawList()->AddRect(ImVec2(tMin.x - pad, tMin.y - pad), ImVec2(tMax.x + pad, tMax.y + pad),
                                                    IM_COL32(255, 200, 60, 255), Px(4.0f), ImDrawCornerFlags_All, Px(3.0f));
        }

        // Below the part, else above, right or left of it; else in the middle of the library.
        const float  boxW = std::min(Px(340.0f), screen.x);
        const ImVec2 size(boxW, s_TourBoxSize.y > 0.0f ? s_TourBoxSize.y : Px(160.0f));
        const float  gap = Px(12.0f);
        const float  maxX = std::max(0.0f, screen.x - size.x), maxY = std::max(0.0f, screen.y - size.y);
        ImVec2 pos(std::clamp((wMin.x + wMax.x - size.x) * 0.5f, 0.0f, maxX),
                   std::clamp((wMin.y + wMax.y - size.y) * 0.5f, 0.0f, maxY));
        if (has)
        {
            const ImVec2 tries[] = {
                ImVec2(std::clamp(tMin.x, 0.0f, maxX), tMax.y + gap),
                ImVec2(std::clamp(tMin.x, 0.0f, maxX), tMin.y - gap - size.y),
                ImVec2(tMax.x + gap, std::clamp(tMin.y, 0.0f, maxY)),
                ImVec2(tMin.x - gap - size.x, std::clamp(tMin.y, 0.0f, maxY)),
            };
            for (const ImVec2& c : tries)
                if (c.x >= 0.0f && c.y >= 0.0f && c.x <= maxX && c.y <= maxY)
                {
                    pos = c;
                    break;
                }
        }

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(boxW, 0.0f), ImVec2(boxW, FLT_MAX));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin("###chatmsg_tour", nullptr, flags))
        {
            // In front of the library, as the error window is, without taking the keyboard.
            if (OurWindowAboveError(ImGui::GetCurrentWindow())) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(kWarnColor, "%s", Localization::Lookup(st.Title));
            ImGui::TextWrapped("%s", Localization::Lookup(st.Text));
            if (Welcome::NeedsMacro(st.Target) && !has)
                ImGui::TextDisabled(T("Make a macro, or select one in the library, to see this part."));
            ImGui::TextDisabled(T("Step %zu of %zu"), s_TourStep + 1, steps.size());
            const bool  last = s_TourStep + 1 == steps.size();
            const char* next = last ? T("Finish") : T("Next");
            UiFont::Row row;
            size_t go = s_TourStep;
            bool   end = false;
            if (s_TourStep > 0)
            {
                if (ImGui::Button(T("Back"))) go = s_TourStep - 1;
                row.Next(UiFont::ButtonWidth(next));
            }
            if (ImGui::Button(next))
            {
                if (last) end = true;
                else go = s_TourStep + 1;
            }
            if (!last)
            {
                row.Next(UiFont::ButtonWidth(T("Skip the tour")));
                if (ImGui::Button(T("Skip the tour"))) end = true;
            }
            ImGui::PopTextWrapPos();
            s_TourBoxSize = ImGui::GetWindowSize();
            if (end) s_TourOn = false;
            else if (go != s_TourStep)
            {
                s_TourStep    = go;
                s_TourEntered = true;
            }
        }
        ImGui::End();
    }

    // --- What's new after an update (docs/design.md, "Update notes") ---------------------------

    bool s_ShowNotes    = true;  // settings.json "show_update_notes"
    bool s_NotesPending = false; // shown once in gameplay
    bool s_NotesOpen    = false;
    std::vector<ReleaseNotes::Section>              s_Notes;
    std::vector<std::vector<ReleaseNotes::Block>>   s_NoteBlocks;

    // CHANGELOG.md, built into the DLL (resource type NOTES).
    std::string ReadChangelog()
    {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&ReadChangelog), &module);
        HRSRC res = module ? FindResourceW(module, MAKEINTRESOURCEW(IDR_CHANGELOG), L"NOTES") : nullptr;
        HGLOBAL data = res ? LoadResource(module, res) : nullptr;
        const void* bytes = data ? LockResource(data) : nullptr;
        if (!bytes) return std::string();
        return std::string(static_cast<const char*>(bytes), SizeofResource(module, res));
    }

    void RememberNotesSeen()
    {
        if (!Storage::SaveSetting(ReleaseNotes::kSeenSetting, std::string(kVersionTag)))
            LogF(LOGL_WARNING, "The \"notes seen\" setting couldn't be saved.");
    }

    void SaveShowNotes()
    {
        if (!Storage::SaveSetting(ReleaseNotes::kShowSetting, s_ShowNotes))
            LogF(LOGL_WARNING, "The \"show update notes\" setting couldn't be saved.");
    }

    void DrawNotesWindow()
    {
        if (!s_NotesOpen) return;
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.45f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(Px(540.0f), screen.x * 0.9f), std::min(Px(440.0f), screen.y * 0.8f)),
                                 ImGuiCond_Appearing);
        bool open = true;
        if (ImGui::Begin(T("What's new in Chat Messages###chatmsg_notes"), &open,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::PushTextWrapPos(0.0f);
            const bool  english = Languages::ActiveId() == "en";
            const ImGuiStyle& style = ImGui::GetStyle();
            const float footer = ImGui::GetFrameHeightWithSpacing() * 2.0f + style.ItemSpacing.y +
                                 (english ? 0.0f : ImGui::GetTextLineHeightWithSpacing());
            ImGui::BeginChild("##notes", ImVec2(0.0f, -footer), true);
            ImGui::PushTextWrapPos(0.0f);
            for (size_t i = 0; i < s_Notes.size() && i < s_NoteBlocks.size(); i++)
            {
                if (i > 0)
                {
                    ImGui::Spacing();
                    ImGui::Separator();
                }
                ImGui::TextColored(kWarnColor, T("Version %s"), s_Notes[i].Version.c_str());
                for (const ReleaseNotes::Block& b : s_NoteBlocks[i])
                {
                    if (b.Kind == ReleaseNotes::EKind::Heading)
                    {
                        ImGui::Spacing();
                        ImGui::TextColored(kFolderColor, "%s", b.Text.c_str());
                    }
                    else if (b.Kind == ReleaseNotes::EKind::Bullet)
                    {
                        const float in = Px(16.0f) * static_cast<float>(std::min(b.Indent, 4));
                        if (in > 0.0f) ImGui::Indent(in);
                        ImGui::Bullet();
                        ImGui::BeginGroup(); // wrapped lines line up after the bullet
                        ImGui::TextWrapped("%s", b.Text.c_str());
                        ImGui::EndGroup();
                        if (in > 0.0f) ImGui::Unindent(in);
                    }
                    else ImGui::TextWrapped("%s", b.Text.c_str());
                }
            }
            ImGui::PopTextWrapPos();
            ImGui::EndChild();
            if (!english) ImGui::TextDisabled(T("The notes are in English."));
            if (ImGui::Checkbox(T("Show what's new after each update"), &s_ShowNotes)) SaveShowNotes();
            if (ImGui::Button(T("Close"))) open = false;
            ImGui::PopTextWrapPos();
        }
        ImGui::End();
        if (!open)
        {
            s_NotesOpen = false;
            RememberNotesSeen();
            s_Notes.clear();
            s_NoteBlocks.clear();
        }
    }

    // --- Language (Nexus options page) -------------------------------------------------------
    // docs/design.md, "Languages". The list and the checks are in Languages.* / Localization.*.

    bool s_LangReloadClicked = false; // the next problems came from Reload, not from loading

    std::string LanguageLabel(const Languages::Entry& aEntry)
    {
        std::string label = aEntry.DisplayName;
        if (aEntry.Percent < 100) label += Fmt(" (%d%%)", aEntry.Percent);
        if (aEntry.FromFolder) label += std::string(" - ") + T("file from your localization folder");
        return label;
    }

    // A clickable web link (ImGui 1.80 has none): underlined text in the link colour, the hand
    // cursor on hover, the address in a tooltip. Only for the addon's built-in addresses.
    void DrawLink(const char* aLabel, const char* aUrl)
    {
        const ImVec4 colour = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
        ImGui::TextColored(colour, "%s", aLabel);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, max.y), max, ImGui::GetColorU32(colour));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("%s", aUrl);
        }
        if (ImGui::IsItemClicked()) Storage::OpenWebPage(aUrl);
    }

    // "Advanced options": one box for the features most players never need (Advanced.h). [?] opens
    // the wiki page that explains it.
    void DrawAdvancedOption()
    {
        if (ImGui::Checkbox(T("Advanced options"), &s_AdvancedOn))
        {
            if (!Storage::SaveSetting(Advanced::kSetting, s_AdvancedOn)) LogF(LOGL_WARNING, "The advanced options setting couldn't be saved.");
            if (s_AdvancedOn) SyncLinks(true);  // catch up with files changed while it was off
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("[?]##advanced_help")) Storage::OpenWebPage(kAdvancedHelpUrl);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip(T("Opens the wiki page about advanced options:\n%s"), kAdvancedHelpUrl);
        }
        ImGui::TextWrapped(T("Shows extra features most players never need: text file links, hotkey cooldowns, the Raw "
                           "text tab, importing a whole library, Keep hotkeys on import and Refresh map list. Off: "
                           "they're hidden and don't work, but nothing you set is deleted."));
        if (!s_AdvancedOn)
        {
            const Advanced::InUse in = Advanced::Count(s_Tree);
            if (in.Linked > 0)
                ImGui::TextColored(kWarnColor, T("%zu macro(s) linked to a text file: they can't be sent while this is off."), in.Linked);
            if (in.Cooldowns > 0)
                ImGui::TextColored(kWarnColor, T("%zu macro(s) with a cooldown: it's ignored while this is off."), in.Cooldowns);
        }
    }

    // The options page's map list: when the GW2 API's list was last fetched, and a button to do
    // it again now (it's also fetched on the first run, weekly, and when the language changes).
    void DrawMapListOptions()
    {
        const MapNames::State st = MapNames::GetState();
        UiFont::Row row;
        ImGui::AlignTextToFramePadding();
        if (st.Refreshing) ImGui::TextUnformatted(T("Map list: loading from the GW2 API..."));
        else if (st.Listed == 0) ImGui::TextUnformatted(T("Map list: not loaded yet."));
        else
        {
            char when[32] = "?";
            const std::time_t t = static_cast<std::time_t>(st.ListedAt);
            std::tm local{};
            if (localtime_s(&local, &t) == 0) std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
            ImGui::Text(T("Map list: %zu maps, from %s."), st.Listed, when);
        }
        if (!st.Refreshing && s_AdvancedOn) // an advanced option: the list refreshes by itself anyway
        {
            row.Next(UiFont::ButtonWidth(T("Refresh map list")));
            if (ImGui::SmallButton(T("Refresh map list"))) MapNames::RefreshNow();
        }
        if (!st.LastError.empty()) ImGui::TextColored(kWarnColor, T("The last refresh failed: %s"), st.LastError.c_str());
        ImGui::TextWrapped(T("Map names for the filters' map search and {map}, in your language where the GW2 API has "
                           "it. It's fetched on the first run, once a week, and when you change the language."));
    }

    void DrawLanguageOptions()
    {
        const std::vector<Languages::Entry>& list = Languages::List();
        const std::string& choice = Languages::Choice();
        const std::string nexusId = Languages::NexusLanguage();
        std::string nexusName = nexusId;
        for (const Languages::Entry& e : list)
            if (e.Id == nexusId) nexusName = e.DisplayName;
        const std::string follow = Fmt(T("Follow Nexus (%s)"), nexusName.c_str());
        std::string current = follow;
        if (choice != Languages::kFollowNexus)
        {
            current = choice; // a chosen language whose file is gone shows its id
            if (!Languages::UsesFolder()) current = Fmt(T("%s (a language file: turn on language files below)"), choice.c_str());
            for (const Languages::Entry& e : list)
                if (e.Id == choice) current = LanguageLabel(e);
        }
        ImGui::SetNextItemWidth(Px(320.0f));
        if (ImGui::BeginCombo(T("Language"), current.c_str()))
        {
            if (ImGui::Selectable(follow.c_str(), choice == Languages::kFollowNexus)) Languages::Choose(Languages::kFollowNexus);
            for (const Languages::Entry& e : list)
            {
                ImGui::PushID(e.Id.c_str());
                if (ImGui::Selectable(LanguageLabel(e).c_str(), choice == e.Id)) Languages::Choose(e.Id);
                if (ImGui::IsItemHovered() && e.FromFolder)
                    ImGui::SetTooltip(T("From %s in your localization folder, made for version %s.\n"
                                        "%zu text(s) were left out by the checks (English is shown for them)."),
                                      e.File.c_str(), e.AddonVersion.empty() ? "?" : e.AddonVersion.c_str(), e.Skipped);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("%s", T("The language of this addon's text. \"Follow Nexus\" uses the language set in Nexus' own options."));
        if (Languages::ActiveId() == "en" && nexusId != "en" && choice == Languages::kFollowNexus)
            ImGui::TextDisabled("%s", T("This addon has no translation for that language yet, so it's shown in English."));
        // Your own language files: their own small opt-in (not Advanced options), since they're an
        // untrusted input and the folder isn't read at all while it's off (docs/design.md, "Languages").
        bool own = Languages::UsesFolder();
        if (ImGui::Checkbox(T("Use language files from the localization folder"), &own)) Languages::UseFolder(own);
        if (!own)
        {
            ImGui::TextDisabled("%s", T("For a language this addon doesn't include yet, from a file someone made for it."));
            return;
        }
        for (const Languages::Entry& e : list)
            if (e.Id == Languages::ActiveId() && e.FromFolder && e.FromNewer)
                ImGui::TextColored(kWarnColor, T("%s was made for a newer version of this addon. Everything this version understands is "
                                                 "used; the rest is shown in English."), e.File.c_str());
        UiFont::Row row;
        if (ImGui::SmallButton(T("Open localization folder"))) Storage::OpenFolder(Languages::Folder());
        row.Next(UiFont::ButtonWidth(T("Reload language files")));
        if (ImGui::SmallButton(T("Reload language files")))
        {
            Languages::RequestReload();
            s_LangReloadClicked = true;
        }
        ImGui::TextWrapped("%s", T("To add a language, put its .json file in <GW2>\\addons\\GW2_Chat_Messages\\localization\\ and click "
                                   "Reload. Every file is checked before it's used, but only use files from this addon's GitHub "
                                   "page (the localization folder there)."));
    }

    void ShowLanguageProblems()
    {
        const std::vector<std::string> problems = Languages::TakeProblems();
        if (problems.empty()) return;
        const char* action = s_LangReloadClicked ? T("Clicked Reload language files in the addon's options")
                                                 : T("Started the game (the addon read its language files)");
        for (const std::string& p : problems) ShowError(action, p);
        s_LangReloadClicked = false;
    }
}

// --- Backups (docs/design.md, "Backups") ---------------------------------------------------------

namespace
{
    constexpr const char* kBackupsSetting  = "backups";
    constexpr double      kBackupCheckSec  = 60.0;   // how often the schedule is looked at
    constexpr double      kBackupRetrySec  = 3600.0; // after a backup couldn't be written
    constexpr char kRestoreBackupPopup[] = TX("Restore this backup?###chatmsg_restore_backup");
    constexpr const char* kUnitLabels[] = {TX("hour(s)"), TX("day(s)"), TX("week(s)"), TX("month(s)")};

    Backups::Settings        s_Backup;
    std::vector<std::string> s_BackupNames;             // files in the backups folder
    double                   s_BackupCheckAt = 0.0;     // ImGui time of the next look at the schedule
    uint64_t                 s_BackupSkipGen = ~0ull;   // the library was unchanged (or empty) at this edit
    Backups::EKind           s_BackupSkipKind = Backups::EKind::Backup;
    std::string              s_BackupInfo;              // "Backed up as ..." under the button

    // What the Restore tab shows for one backup (read once per file name).
    struct BackupInfo
    {
        bool        Ok = false;
        size_t      Folders = 0, Macros = 0;
        std::string Error;
    };
    std::map<std::string, BackupInfo> s_BackupInfos;
    std::string s_RestoreName;          // the backup the Restore? popup is about
    bool        s_RestoreOpenRequest = false;

    enum class EBackup { Made, Unchanged, Failed };

    // At the start and on Refresh (both a click or the game starting): the folder's file names.
    void RefreshBackupNames() { s_BackupNames = Storage::BackupNames(); }

    // Backups run on the writer thread (Storage::Post): the library becomes JSON here, on the
    // render thread that owns it; reading the last backup, writing, listing and deleting happen
    // there. One at a time; PollBackups picks up the result.
    struct BackupDone
    {
        EBackup                  Result = EBackup::Failed;
        bool                     Manual = false; // Back up now
        uint64_t                 Gen    = 0;     // s_TreeGen when it was started
        Backups::EKind           Kind   = Backups::EKind::Backup;
        std::string              Name;           // the file made (Made)
        std::vector<std::string> Names;          // the backups folder afterwards
    };
    std::mutex                              s_BackupDoneMutex; // guards the three results below
    std::optional<BackupDone>               s_BackupDone;      // a finished backup
    std::optional<std::vector<std::string>> s_BackupListed;    // the once-a-minute look at the folder
    std::map<std::string, BackupInfo>       s_BackupInfoDone;  // the Restore tab's reads
    bool                  s_BackupRunning = false; // a backup job is posted, its result not in yet
    bool                  s_BackupManualWanted = false; // Back up now was clicked while one was running
    std::atomic<bool>     s_StopBackupReads{false};     // Ui::Shutdown: skip the Restore tab's reads still waiting
    constexpr size_t      kBackupReadsInFlight = 2;     // Restore tab reads posted at a time
    bool                  s_BackupListing = false; // the same for the folder listing
    std::set<std::string> s_BackupInfoAsked;       // Restore tab: reads posted, results not in yet

    // On the writer thread: writes a backup of aJson (null: a newer version's library, whose
    // file is copied as it is, since this build would write it in its own way), then deletes the
    // oldest of that kind beyond aKeep. Without aForce, nothing is written when the newest one
    // of that kind is the same. Only Storage and Backups functions: nothing of the UI.
    EBackup WriteBackupFile(const std::shared_ptr<const nlohmann::json>& aJson, Backups::EKind aKind, int aKeep,
                            bool aForce, std::string& aName)
    {
        if (Storage::BackupsDir().empty()) return EBackup::Failed;
        std::string text;
        if (aJson) text = Storage::JsonText(*aJson);
        else if (!Storage::ReadLibraryText(text)) return EBackup::Failed;
        if (!aForce)
        {
            const std::time_t newest = Backups::Newest(Storage::BackupNames(), aKind);
            std::string old;
            if (newest != 0 && Storage::ReadBackupText(Backups::FileName(aKind, newest), old) && old == text)
                return EBackup::Unchanged;
        }
        const std::string name = Backups::FileName(aKind, std::time(nullptr));
        if (!Storage::WriteBackup(name, text))
        {
            LogF(LOGL_WARNING, "The backup %s couldn't be written.", name.c_str());
            return EBackup::Failed;
        }
        LogF(LOGL_INFO, "Backed up the macros as %s.", name.c_str());
        aName = name;
        for (const std::string& old : Backups::ToDelete(Storage::BackupNames(), aKind, aKeep))
        {
            if (old == name) continue; // never the one just made (a clock set back)
            Storage::DeleteBackup(old);
        }
        return EBackup::Made;
    }

    // Starts a backup of the library as it is now. False while one is still running.
    bool StartBackup(Backups::EKind aKind, int aKeep, bool aForce, bool aManual)
    {
        if (s_BackupRunning) return false;
        std::shared_ptr<const nlohmann::json> json;
        if (!s_Tree.FromNewerVersion()) json = std::make_shared<const nlohmann::json>(s_Tree.ToJson());
        s_BackupRunning = true;
        const uint64_t gen = s_TreeGen;
        Storage::Post([json, aKind, aKeep, aForce, aManual, gen] {
            BackupDone d;
            d.Manual = aManual;
            d.Gen    = gen;
            d.Kind   = aKind;
            try
            {
                d.Result = WriteBackupFile(json, aKind, aKeep, aForce, d.Name);
                d.Names  = Storage::BackupNames();
            }
            catch (const std::exception& e)
            {
                LogF(LOGL_WARNING, "The backup failed: %s", e.what());
                d.Result = EBackup::Failed;
            }
            // Always handed back, or s_BackupRunning would never clear.
            std::lock_guard<std::mutex> lock(s_BackupDoneMutex);
            s_BackupDone = std::move(d);
        });
        return true;
    }

    // Restore tab entries whose file is gone.
    void ForgetMissingBackupInfos()
    {
        for (auto it = s_BackupInfos.begin(); it != s_BackupInfos.end();)
        {
            const bool there = std::find(s_BackupNames.begin(), s_BackupNames.end(), it->first) != s_BackupNames.end();
            it = there ? std::next(it) : s_BackupInfos.erase(it);
        }
    }

    constexpr double kMapCheckSec = 60.0;
    double           s_MapCheckAt = 0.0;

    // Once a minute: the game's build, so a game update fetches the map list again (MapNames).
    void CheckMapList()
    {
        if (ImGui::GetTime() < s_MapCheckAt) return;
        s_MapCheckAt = ImGui::GetTime() + kMapCheckSec;
        MapNames::Check(s_Mumble ? s_Mumble->Context.BuildID : 0);
    }

    // With the folder's names fresh from the writer thread: is the schedule's backup due?
    void StartBackupIfDue()
    {
        if (s_BackupRunning) return;
        const Backups::EKind kind = Backups::Kind(s_Backup);
        if (!Backups::IsDue(Backups::Newest(s_BackupNames, kind), std::time(nullptr), Backups::UnitInEffect(s_Backup),
                            Backups::EveryInEffect(s_Backup)))
            return;
        if (s_Tree.Root().Children.empty() && s_Tree.Trash().empty())
        {
            s_BackupSkipGen = s_TreeGen; // an empty library isn't worth a copy
            s_BackupSkipKind = kind;
            return;
        }
        StartBackup(kind, Backups::KeepInEffect(s_Backup), false, false);
    }

    // Once a minute: the user's schedule, or the weekly safety copy while backups are off. The
    // folder is listed on the writer thread; StartBackupIfDue runs when its names are in.
    void CheckBackups()
    {
        if (ImGui::GetTime() < s_BackupCheckAt) return;
        s_BackupCheckAt = ImGui::GetTime() + kBackupCheckSec;
        if (Storage::AddonDir().empty()) return;
        const Backups::EKind kind = Backups::Kind(s_Backup);
        if (s_TreeGen == s_BackupSkipGen && kind == s_BackupSkipKind) return; // nothing new to back up
        if (s_BackupRunning || s_BackupListing) return;
        s_BackupListing = true;
        Storage::Post([] {
            std::vector<std::string> names;
            try { names = Storage::BackupNames(); }
            catch (const std::exception& e) { LogF(LOGL_WARNING, "The backups folder couldn't be listed: %s", e.what()); }
            std::lock_guard<std::mutex> lock(s_BackupDoneMutex);
            s_BackupListed = std::move(names);
        }, "backup-list");
    }

    void PollBackupResults();

    // Once a frame: results from the writer thread, then a Back up now that had to wait.
    void PollBackups()
    {
        PollBackupResults();
        if (s_BackupManualWanted && !s_BackupRunning)
        {
            s_BackupManualWanted = false;
            StartBackup(Backups::EKind::Backup, Backups::Clamped(s_Backup).Keep, true, true);
        }
    }

    void PollBackupResults()
    {
        std::optional<BackupDone> done;
        std::optional<std::vector<std::string>> listed;
        {
            std::lock_guard<std::mutex> lock(s_BackupDoneMutex);
            done.swap(s_BackupDone);
            listed.swap(s_BackupListed);
        }
        if (listed)
        {
            s_BackupListing = false;
            s_BackupNames = std::move(*listed);
            ForgetMissingBackupInfos();
            StartBackupIfDue();
        }
        if (!done) return;
        s_BackupRunning = false;
        if (done->Result != EBackup::Failed || !done->Names.empty()) s_BackupNames = std::move(done->Names);
        ForgetMissingBackupInfos();
        if (done->Manual)
        {
            if (done->Result == EBackup::Made) s_BackupInfo = Fmt(T("Backed up as %s."), done->Name.c_str());
            else
            {
                s_BackupInfo.clear();
                ShowError(T("Clicked Back up now in the addon's options"),
                          T("The backup couldn't be written (one may have been made this second already). Check the Nexus log."));
            }
            return;
        }
        switch (done->Result)
        {
        case EBackup::Made: break;
        case EBackup::Unchanged:
            s_BackupSkipGen  = done->Gen; // look again after the next change
            s_BackupSkipKind = done->Kind;
            break;
        case EBackup::Failed: s_BackupCheckAt = ImGui::GetTime() + kBackupRetrySec; break;
        }
    }

    void SaveBackupSettings()
    {
        s_Backup = Backups::Clamped(s_Backup);
        if (!Storage::SaveSetting(kBackupsSetting, Backups::ToJson(s_Backup)))
            LogF(LOGL_WARNING, "The backup settings couldn't be saved.");
        s_BackupCheckAt = 0.0; // look at the new schedule now
        s_BackupSkipGen = ~0ull;
    }

    std::string ExampleBackupName() { return Backups::FileName(Backups::EKind::Backup, std::time(nullptr)); }

    void DrawBackupOptions()
    {
        if (ImGui::Checkbox(T("Make backups of my macros"), &s_Backup.Enabled)) SaveBackupSettings();

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Back up every"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(Px(90.0f));
        int every = s_Backup.Every;
        if (ImGui::InputInt("##backup_every", &every, 1, 1) && every != s_Backup.Every)
        {
            s_Backup.Every = every;
            SaveBackupSettings();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(Px(140.0f));
        const int unit = static_cast<int>(s_Backup.Unit);
        if (ImGui::BeginCombo("##backup_unit", Localization::Lookup(kUnitLabels[unit])))
        {
            for (int i = 0; i < static_cast<int>(Backups::EUnit::Count); ++i)
                if (ImGui::Selectable(Localization::Lookup(kUnitLabels[i]), i == unit) && i != unit)
                {
                    s_Backup.Unit = static_cast<Backups::EUnit>(i);
                    SaveBackupSettings(); // brings "every" into the new unit's range
                }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Up to 48 hours, 30 days, 8 weeks or 12 months."));

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Copies to keep"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(Px(90.0f));
        int keep = s_Backup.Keep;
        if (ImGui::InputInt("##backup_keep", &keep, 1, 5) && keep != s_Backup.Keep)
        {
            s_Backup.Keep = keep;
            SaveBackupSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("1 to 50."));
        ImGui::Spacing();

        ImGui::TextWrapped(T("Backups go into <GW2>\\addons\\GW2_Chat_Messages\\backups\\. Each file is named with the "
                             "date and time of the backup in UTC, for example %s."), ExampleBackupName().c_str());
        ImGui::TextWrapped(T("A backup is only made when your macros changed since the last one. When there are more "
                             "copies than you keep, the oldest are deleted."));
        if (!s_Backup.Enabled)
            ImGui::TextColored(kWarnColor, "%s", T("Backups are off."));
        if (!s_Backup.Enabled)
            ImGui::TextWrapped(T("While backups are off, the addon still quietly makes a safety copy of your macros once "
                                 "a week, in case macros.json is ever damaged. It keeps the last 4 and deletes the oldest "
                                 "when it makes a new one. They're on the Restore tab too."));

        const Backups::EKind kind = Backups::Kind(s_Backup);
        const std::time_t newest = Backups::Newest(s_BackupNames, kind);
        if (newest == 0) ImGui::TextDisabled("%s", kind == Backups::EKind::Backup ? T("No backup yet.") : T("No safety copy yet."));
        else
            ImGui::TextDisabled(kind == Backups::EKind::Backup ? T("Last backup: %s at %s UTC.") : T("Last safety copy: %s at %s UTC."),
                                Backups::DateText(newest).c_str(), Backups::TimeText(newest).c_str());

        UiFont::Row row;
        // Written on the writer thread: PollBackups shows "Backed up as ..." or the error. A click
        // while one is still being written does nothing (that one's result shows soon).
        if (ImGui::Button(T("Back up now")) && !StartBackup(Backups::EKind::Backup, Backups::Clamped(s_Backup).Keep, true, true))
            s_BackupManualWanted = true; // made as soon as the running one is done (PollBackups)
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Makes a backup now, even if nothing changed. It counts as one of the copies you keep."));
        row.Next(UiFont::ButtonWidth(T("Open the backups folder")));
        if (ImGui::Button(T("Open the backups folder"))) Storage::OpenFolder(Storage::BackupsDir());
        if (!s_BackupInfo.empty()) ImGui::TextWrapped("%s", s_BackupInfo.c_str());
    }

    void RestoreBackup(const std::string& aName)
    {
        const char* action = T("Clicked Restore on the Restore tab of the addon's options");
        // Read-only: a restore would be saved over the newer version's macros.json, and Undo
        // restore couldn't write it back (a read-only library is never saved).
        if (s_Tree.FromNewerVersion())
        {
            ShowError(action, T("Your macros were saved by a newer version of the addon, so they're read-only and a backup "
                                "can't be restored over them. Update the addon first."));
            return;
        }
        Backups::EKind kind;
        std::time_t when = 0;
        MacroTree::Tree tree;
        std::string error;
        if (!Backups::ParseFileName(aName, kind, when) || !Storage::ReadBackup(aName, tree, error))
        {
            ShowError(action, Fmt(T("The backup %s couldn't be restored: %s"), aName.c_str(), error.c_str()));
            return;
        }
        CloseImport();
        s_UndoImport    = s_Tree.ToJson();
        s_UndoIsRestore = true;
        s_UndoText      = Fmt(T("Restored the backup from %s at %s UTC."), Backups::DateText(when).c_str(), Backups::TimeText(when).c_str());
        s_Tree       = std::move(tree);
        // Linked macros sync again as at a start: a link file that exists wins, so a restore never
        // writes old lines into the user's .txt files.
        s_LinkStates.clear();
        s_LinkProblems.clear();
        s_SelectedId = 0;
        s_TrashSelId = 0;
        s_RawForId   = 0;
        CancelCapture();
        MarkDirty();
        s_ShowMainWindow = true;
    }

    void DrawRestoreBackupPopup()
    {
        if (s_RestoreOpenRequest)
        {
            ImGui::OpenPopup(Localization::Lookup(kRestoreBackupPopup));
            s_RestoreOpenRequest = false;
        }
        if (!ImGui::BeginPopupModal(Localization::Lookup(kRestoreBackupPopup), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
        ImGui::PushTextWrapPos(Px(420.0f));
        Backups::EKind kind;
        std::time_t when = 0;
        const auto info = s_BackupInfos.find(s_RestoreName);
        if (Backups::ParseFileName(s_RestoreName, kind, when) && info != s_BackupInfos.end() && info->second.Ok)
        {
            ImGui::TextWrapped(T("Replace your whole library with the backup from %s at %s UTC (%zu folder(s), %zu macro(s))?"),
                               Backups::DateText(when).c_str(), Backups::TimeText(when).c_str(), info->second.Folders, info->second.Macros);
            ImGui::TextWrapped(T("Every folder and macro you have now is replaced, and so is the trash bin. Until the game "
                                 "closes, Undo restore in the library window puts your library back as it is now."));
            if (ImGui::Button(T("Restore"))) { RestoreBackup(s_RestoreName); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
        }
        if (ImGui::Button(T("Cancel"))) ImGui::CloseCurrentPopup();
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    void DrawRestoreOptions()
    {
        ImGui::TextWrapped(T("Your backups and safety copies, newest first. Restore replaces your whole library with the "
                             "one in the backup."));
        UiFont::Row row;
        if (ImGui::Button(T("Refresh")))
        {
            s_BackupInfos.clear();
            s_BackupInfoAsked.clear(); // read again (a read still waiting is replaced, not doubled)
            RefreshBackupNames();
        }
        row.Next(UiFont::ButtonWidth(T("Open the backups folder")));
        if (ImGui::Button(T("Open the backups folder"))) Storage::OpenFolder(Storage::BackupsDir());

        struct Row { std::string Name; Backups::EKind Kind; std::time_t When; };
        std::vector<Row> rows;
        for (const std::string& n : s_BackupNames)
        {
            Row r{n, Backups::EKind::Backup, 0};
            if (Backups::ParseFileName(n, r.Kind, r.When)) rows.push_back(std::move(r));
        }
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.When != b.When ? a.When > b.When : a.Name > b.Name; });
        if (rows.empty())
        {
            ImGui::TextDisabled("%s", T("There are no backups yet."));
            return;
        }

        // Each backup is read and counted on the writer thread: parsing a big library takes a
        // while, and the game keeps drawing meanwhile ("..." until it's in).
        {
            std::lock_guard<std::mutex> lock(s_BackupDoneMutex);
            for (auto& kv : s_BackupInfoDone)
            {
                s_BackupInfoAsked.erase(kv.first);
                s_BackupInfos[kv.first] = std::move(kv.second);
            }
            s_BackupInfoDone.clear();
        }
        // A few at a time, so a save posted meanwhile doesn't wait behind every backup's read.
        for (const Row& r : rows)
        {
            if (s_BackupInfoAsked.size() >= kBackupReadsInFlight) break;
            if (s_BackupInfos.count(r.Name) || s_BackupInfoAsked.count(r.Name)) continue;
            s_BackupInfoAsked.insert(r.Name);
            const std::string name = r.Name;
            Storage::Post([name] {
                if (s_StopBackupReads) return; // unloading
                BackupInfo bi;
                try
                {
                    MacroTree::Tree tree;
                    bi.Ok = Storage::ReadBackup(name, tree, bi.Error);
                    if (bi.Ok) Tree::CountDescendants(tree.Root(), &bi.Folders, &bi.Macros);
                }
                catch (const std::exception&)
                {
                    bi.Ok    = false;
                    bi.Error = TR("The file isn't a macro library.");
                }
                std::lock_guard<std::mutex> lock(s_BackupDoneMutex);
                s_BackupInfoDone[name] = std::move(bi);
            }, "backup-info:" + name);
        }

        const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("##backups", 6, flags))
        {
            ImGui::TableSetupColumn(T("Date (UTC)"));
            ImGui::TableSetupColumn(T("Time (UTC)"));
            ImGui::TableSetupColumn(T("Folders"));
            ImGui::TableSetupColumn(T("Macros"));
            ImGui::TableSetupColumn(T("Type"));
            ImGui::TableSetupColumn("##restore");
            ImGui::TableHeadersRow();
            for (const Row& r : rows)
            {
                ImGui::PushID(r.Name.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(Backups::DateText(r.When).c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.Name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(Backups::TimeText(r.When).c_str());
                const auto it = s_BackupInfos.find(r.Name);
                ImGui::TableNextColumn();
                if (it == s_BackupInfos.end()) ImGui::TextDisabled("...");
                else if (it->second.Ok) ImGui::Text("%zu", it->second.Folders);
                else
                {
                    ImGui::TextColored(kWarnColor, "%s", T("Can't be read"));
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", it->second.Error.c_str());
                }
                ImGui::TableNextColumn();
                if (it != s_BackupInfos.end() && it->second.Ok) ImGui::Text("%zu", it->second.Macros);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(r.Kind == Backups::EKind::Backup ? T("Backup") : T("Safety copy"));
                ImGui::TableNextColumn();
                if (it != s_BackupInfos.end() && it->second.Ok && ImGui::SmallButton(T("Restore...")))
                {
                    s_RestoreName = r.Name;
                    s_RestoreOpenRequest = true; // opened below, outside the table's ID level
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
}

void Ui::Init()
{
    UiFont::Init();
    std::string notesSeen; // settings.json "notes_seen"
    {
        const nlohmann::json settings = Storage::LoadSettings();
        const auto it = settings.find(kTrashSetting);
        s_TrashOn = it == settings.end() || !it->is_boolean() || it->get<bool>(); // on by default
        const auto active = settings.find(kActiveOnlySetting);
        s_ActiveOnly = active != settings.end() && active->is_boolean() && active->get<bool>(); // off by default
        const auto examples = settings.find(kExamplesSetting);
        s_ExamplesAdded = examples != settings.end() && examples->is_boolean() && examples->get<bool>();
        const auto welcome = settings.find(Welcome::kShownSetting);
        s_WelcomeShown = welcome != settings.end() && welcome->is_boolean() && welcome->get<bool>();
        s_ShowNotes = ReleaseNotes::ShowFromSettings(settings);
        const auto seen = settings.find(ReleaseNotes::kSeenSetting);
        notesSeen = seen != settings.end() && seen->is_string() ? seen->get<std::string>() : std::string();
        const auto recent = settings.find(kRecentSetting);
        if (recent != settings.end()) s_Recent.FromJson(*recent);
        else s_Recent.Clear();
        const auto recentCount = settings.find(kRecentCountSetting);
        s_RecentCount = recentCount != settings.end() ? QuickLists::CountFromJson(*recentCount) : QuickLists::kDefaultRecent;
        const auto backups = settings.find(kBackupsSetting);
        s_Backup = backups != settings.end() ? Backups::FromJson(*backups) : Backups::Settings{};
        s_AdvancedOn = Advanced::FromSettings(settings); // off unless the user turned it on
        const auto sections = settings.find(EditorLayout::kSetting);
        s_Sections = sections != settings.end() ? EditorLayout::FromJson(*sections) : EditorLayout::Sections{};
    }
    s_TrashSelId = 0;
    // The imports and exports folders exist from the start, so there's never one to make by hand.
    if (Storage::ImportsDir().empty() || Storage::ExportsDir().empty() || Storage::BackupsDir().empty())
        LogF(LOGL_WARNING, "The imports, exports and backups folders couldn't be created.");
    RefreshBackupNames();
    s_BackupCheckAt = 0.0;
    s_BackupSkipGen = ~0ull;
    s_BackupInfos.clear();
    s_BackupInfo.clear();
    s_Mumble = static_cast<Mumble::Data*>(g_Api->DataLink_Get(DL_MUMBLE_LINK));
    s_NexusLink = static_cast<NexusLinkData_t*>(g_Api->DataLink_Get(DL_NEXUS_LINK));
    s_LoadMessages.clear();
    const bool noLibrary = Storage::LoadMacros(s_Tree, s_LoadMessages);
    // A first run (no macros.json yet) gets the welcome: it asks about the example macros, then
    // offers the tour. A library that already exists counts as seen, so removing macros.json
    // later starts empty, as before (docs/design.md, "First-run welcome and tour").
    s_ExamplesRequest = false;
    s_WelcomePending  = false;
    s_WelcomeRequest  = false;
    s_TourOn          = false;
    s_TourStartRequest = false;
    Welcome::Start start;
    if (!Storage::AddonDir().empty())
    {
        start = Welcome::Decide(s_ExamplesAdded, s_WelcomeShown, noLibrary, s_Tree.Root().Children.empty());
        s_WelcomePending = start.ShowWelcome;
        if (start.ShowWelcome) LogF(LOGL_INFO, "First run: the welcome will ask about the example macros.");
        if (start.MarkExamplesAdded)
        {
            s_ExamplesAdded = true;
            if (!Storage::SaveSetting(kExamplesSetting, true)) LogF(LOGL_WARNING, "The \"examples added\" setting couldn't be saved.");
        }
        if (start.MarkShown) NoteWelcomeShown();

        // What's new: once after an update, from the changelog built into the DLL.
        const std::vector<ReleaseNotes::Section> notes =
            ReleaseNotes::Since(ReleaseNotes::Parse(ReadChangelog()), notesSeen, kVersionTag, kVersionChannel);
        const ReleaseNotes::Start show =
            ReleaseNotes::Decide(s_ShowNotes, start.ShowWelcome, notesSeen, kVersionTag, !notes.empty());
        s_Notes.clear();
        s_NoteBlocks.clear();
        s_NotesOpen    = false;
        s_NotesPending = show.Show;
        if (show.Remember) RememberNotesSeen();
        if (show.Show)
        {
            s_Notes = notes;
            for (const ReleaseNotes::Section& s : s_Notes) s_NoteBlocks.push_back(ReleaseNotes::ToBlocks(s.Body));
        }
    }
    for (const std::string& m : s_LoadMessages)
    {
        LogF(LOGL_WARNING, "%s", m.c_str());
        ShowError(T("Started the game (the addon loaded macros.json)"), m);
    }
    s_LoadMessages.clear();
    ShowLanguageProblems();
    s_Dirty = false;
}

void Ui::Shutdown()
{
    SyncLinks(true); // last edits to linked macros reach their files
    s_StopBackupReads = true; // Restore tab reads still waiting are skipped: the unload doesn't wait for them
    Storage::Flush();         // every job of ours (saves, backups) has finished
    // A save handed to the writer that failed (or whose result wasn't looked at yet) is tried
    // once more here, as a failed save always was.
    if (s_SaveSeq != 0)
    {
        const Storage::SaveResult r = Storage::LastMacrosSave();
        if (r.Seq != s_SaveSeq || !r.Ok) s_Dirty = true;
    }
    if (s_Dirty && !s_Tree.FromNewerVersion() && !Storage::SaveMacros(s_Tree))
        LogF(LOGL_WARNING, "Unsaved macro changes could not be written on unload.");
    s_StopBackupReads = false;
    s_Dirty   = false;
    s_SaveSeq = 0;
    s_SaveFailed = false;
    // The caches hold ids (and the link round pointers) of the tree that's about to go.
    s_ActiveHere.Valid  = false;
    s_SearchCache.Valid = false;
    s_MenuCache.Valid   = false;
    s_RowLabels.clear();
    s_LinkRound.clear();
    s_LinkRoundAt = 0;
    s_BackupManualWanted = false;
    {
        std::lock_guard<std::mutex> lock(s_BackupDoneMutex);
        s_BackupDone.reset();
        s_BackupListed.reset();
        s_BackupInfoDone.clear();
    }
    s_BackupRunning = false;
    s_BackupListing = false;
    s_BackupInfoAsked.clear();
    s_Tree  = Tree();
    s_SelectedId = 0;
    CloseImport();
    s_UndoImport.reset();
    s_LinkStates.clear();
    s_LinkProblems.clear();
    s_Preview = {};
    s_Cooldowns.Clear();
    s_Recent.Clear();
    s_ExamplesRequest = false;
    s_WelcomePending  = false;
    s_WelcomeRequest  = false;
    s_TourOn          = false;
    s_TourStartRequest = false;
    s_NotesPending    = false;
    s_NotesOpen       = false;
    s_Notes.clear();
    s_NoteBlocks.clear();
    s_NexusLink       = nullptr;
    s_BackupNames.clear();
    s_BackupInfos.clear();
    s_RestoreName.clear();
    s_RestoreOpenRequest = false;
    s_UndoIsRestore = false;
}

void Ui::Render()
{
    Sender::FrameRendered(); // first: the sender's whisper pauses count frames
    ShowLanguageProblems(); // after a reload (Languages::Update ran before this)
    PollSender();
    WatchMap();
    SyncLinks(false);
    HandleHotkeys();
    UiFont::NewFrame();
    UiFont::Push(); // the chosen text size, for our windows only
    if (s_WelcomePending && InGameplay()) // a first run: the library opens with the welcome in it
    {
        s_WelcomePending = false;
        s_ShowMainWindow = true;
        s_WelcomeRequest = true;
    }
    if (s_NotesPending && InGameplay())
    {
        s_NotesPending = false;
        s_NotesOpen    = true;
    }
    if (s_TourStartRequest)
    {
        s_TourStartRequest = false;
        s_ShowMainWindow   = true;
        StartTour();
    }
    if (s_ShowMainWindow) DrawMainWindow();
    DrawTour();
    DrawNotesWindow();
    DrawImportWindow();
    DrawExportWindow();
    DrawQuickMenuPopup();
    DrawErrorWindow();
    UiFont::Pop();
    if (s_Dirty && ImGui::GetTime() >= s_SaveAt) SaveNow();
    PollSave();
    PollBackups();
    CheckBackups();
    CheckMapList();
}

void Ui::RenderOptions()
{
    ImGui::Text(T("Chat Messages %s"), kVersionText);
    ImGui::TextDisabled(T("Build %s"), BuildStamp::Text());
    DrawLink(T("Chat Messages on GitHub"), kRepositoryUrl);
    DrawLink(T("ArenaNet's macro policy"), kMacroPolicyUrl);
    ImGui::TextDisabled(T("This addon only sends chat messages you start yourself. Don't use it to automate gameplay."));
    if (!ImGui::BeginTabBar("##chatmsg_options")) return;
    if (ImGui::BeginTabItem(T("General###chatmsg_tab_general")))
    {
        ImGui::TextDisabled(T("Your macros are saved in <GW2>\\addons\\GW2_Chat_Messages\\macros.json."));
        if (ImGui::Button(T("Open macro library"))) s_ShowMainWindow = true;
        ImGui::SameLine();
        if (ImGui::Button(T("Add the example macros")))
        {
            s_ShowMainWindow  = true; // the library asks first (replace, or add again) and shows them
            s_ExamplesRequest = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Adds an \"Examples\" folder to the library, with macros that show several lines,\n"
                              "channels, a whisper, placeholders and filters. If you already have it, the library\n"
                              "asks whether to replace it with a fresh copy. Your own macros aren't changed.\n"
                              "The examples have no hotkeys."));
        ImGui::SameLine();
        if (ImGui::Button(T("Take the tour"))) s_TourStartRequest = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Opens the library and shows its parts, one step at a time."));
        if (ImGui::Checkbox(T("Show what's new after each update"), &s_ShowNotes)) SaveShowNotes();
        ImGui::TextDisabled(T("Once after each update, a window lists what changed (in English)."));
        ImGui::Spacing();
        UiFont::DrawCombo(T("Text size"));
        ImGui::TextDisabled(T("The size of the text in this addon's windows (not this page)."));
        ImGui::Spacing();
        DrawLanguageOptions();
        ImGui::Spacing();
        if (ImGui::Checkbox(T("Use a trash bin for deleted folders and macros"), &s_TrashOn))
        {
            if (!Storage::SaveSetting(kTrashSetting, s_TrashOn)) LogF(LOGL_WARNING, "The trash bin setting couldn't be saved.");
        }
        ImGui::TextWrapped(T("On: Delete moves a folder or macro to the Trash bin at the bottom of the library, "
                           "with no question asked. From there you can restore it (back where it was, or at the top "
                           "level if its folder is gone) or delete it for good. Items in the bin can't be sent or "
                           "used by a hotkey. They stay until you empty the bin. "
                           "Off: Delete asks first and removes the item for good. Items already in the bin stay "
                           "there, and the bin stays visible, until you empty it."));
        if (!s_Tree.Trash().empty()) ImGui::TextDisabled(T("The trash bin holds %zu item(s)."), s_Tree.Trash().size());
        ImGui::Spacing();
        DrawMapListOptions();
        ImGui::Spacing();
        ImGui::TextWrapped(T("Quick menu: right-click the chat bubble icon in the Nexus bar, or set a key for "
                           "\"Open Chat Messages quick menu\" in Nexus > Options > Keybinds."));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Recently sent in the quick menu:"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(Px(90.0f));
        int count = s_RecentCount;
        if (ImGui::InputInt("##recent_count", &count, 1, 1))
        {
            count = std::clamp(count, 0, QuickLists::kMaxRecent);
            if (count != s_RecentCount)
            {
                s_RecentCount = count;
                if (!Storage::SaveSetting(kRecentCountSetting, s_RecentCount))
                    LogF(LOGL_WARNING, "The recently sent setting couldn't be saved.");
            }
        }
        if (!s_Recent.Ids().empty())
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(T("Clear the list")))
            {
                s_Recent.Clear();
                SaveRecent();
            }
        }
        ImGui::TextWrapped(T("How many of the macros you sent last are listed at the top of the quick menu, under your "
                           "favourites (0 to 10; 0 = none). Mark a favourite in the macro's editor, or right-click it "
                           "in the library."));
        ImGui::Spacing();
        DrawAdvancedOption();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(T("Backups###chatmsg_tab_backups")))
    {
        DrawBackupOptions();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(T("Restore###chatmsg_tab_restore")))
    {
        DrawRestoreOptions();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    DrawRestoreBackupPopup();
}

void Ui::ToggleMainWindow() { s_ShowMainWindow = !s_ShowMainWindow; }

void Ui::RenderQuickAccessMenu()
{
    UiFont::Push();
    DrawQuickMenu(s_QaSearch); // no keyboard focus here: click the box to type
    UiFont::Pop();
}

void Ui::ToggleQuickMenu()
{
    // ImGui::IsPopupOpen only sees popups at the current ID level, which is ours here (root).
    if (ImGui::IsPopupOpen(kQuickMenuPopup)) s_QuickMenuCloseRequest = true;
    else s_QuickMenuOpenRequest = true;
}
bool* Ui::MainWindowOpenFlag() { return &s_ShowMainWindow; }
