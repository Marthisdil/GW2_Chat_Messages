#include "Ui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "imgui.h"

#include "Languages.h"
#include "Localization.h"

#include "BuildStamp.h"
#include "ChatLine.h"
#include "ErrorReport.h"
#include "Exchange.h"
#include "GameKeys.h"
#include "Globals.h"
#include "Hotkeys.h"
#include "LinkSync.h"
#include "MapNames.h"
#include "MacroTree.h"
#include "Mumble.h"
#include "Placeholders.h"
#include "Search.h"
#include "SendPlan.h"
#include "Sender.h"
#include "PathText.h"
#include "Storage.h"
#include "UiFont.h"

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
    constexpr const char* kTrashSetting    = "trash_bin";
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
    Search::Result s_Search;
    const ImU32    kSearchHighlight = IM_COL32(255, 215, 0, 80);

    // Name popup (new folder, new macro, rename).
    enum class ENameMode { NewFolder, NewMacro, Rename };
    ENameMode   s_NameMode     = ENameMode::NewFolder;
    uint64_t    s_NameTargetId = 0; // parent folder for New*, the node for Rename (0 = root)
    char        s_NameBuf[256] = {};
    bool        s_NameFocus    = false;

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
                         Trash, OpenRestore, RestoreNow, OpenPurge, OpenEmpty };
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
    }

    // Opens the error window: aAction is what the user did, in their words ("Clicked Export all").
    // aFocus brings it to the front; false for errors from hotkeys and the quick menu, which happen
    // while the user is playing. Defined with the error window below.
    void ShowError(const std::string& aAction, const std::string& aMessage, bool aFocus = true);

    void SaveNow()
    {
        if (!s_Dirty) return;
        const bool failedBefore = s_SaveFailed;
        s_SaveFailed = !Storage::SaveMacros(s_Tree);
        if (!s_SaveFailed) s_Dirty = false;
        else s_SaveAt = ImGui::GetTime() + 5.0; // retry later, without hammering the disk
        if (s_SaveFailed && !failedBefore)
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

    bool IsActiveHere(const Node& aMacro, const Here& aHere) { return IsActive(aMacro, aHere.Mode, aHere.MapId); }

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
            DrawErrorBody();
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
        const std::string name = "\"" + aMacro.Name + "\"";
        const std::string action = aFrom == ESendFrom::Hotkey ? Fmt(T("Pressed the hotkey %s to send %s"), Hotkey::Display(aMacro.Hotkey).c_str(), name.c_str())
                                 : aFrom == ESendFrom::Menu   ? Fmt(T("Sent %s from the quick menu"), name.c_str())
                                                              : Fmt(T("Sent %s from the library (Send now)"), name.c_str());
        std::string error;
        if (Sender::Start(aMacro.Id, aMacro.Lines, error))
        {
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
        const Sender::Status st = Sender::GetStatus();
        if (st.Seq == s_SendSeenSeq) return;
        s_SendSeenSeq = st.Seq;
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
            std::vector<Node*> macros;
            for (Node* m : s_Tree.MacrosWithHotkey(combo))
                if (IsActiveHere(*m, here)) macros.push_back(m);
            if (macros.empty()) continue;
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
            // A shared hotkey is only a conflict where both macros can be active at once.
            std::string names;
            for (const Node* n : s_Tree.MacrosWithHotkey(aMacro.Hotkey))
                if (n != &aMacro && FiltersOverlap(aMacro, *n)) names += (names.empty() ? "" : ", ") + n->Name;
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
            else if (st.Listed == 0)
                ImGui::TextDisabled(T("The map list isn't loaded yet (it comes from the GW2 API). Try Refresh map list "
                                    "on the addon's options page, or add the map by its id."));
            else ImGui::TextDisabled(T("No map matches."));
            return;
        }
        ImGui::BeginChild("##mapresults", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() *
                          (static_cast<float>(std::min<size_t>(s_MapResults.size(), 6)) + 0.5f)), true);
        for (const MapNames::Entry& e : s_MapResults) pick(e);
        ImGui::EndChild();
        if (s_MapResults.size() >= kMapResults) ImGui::TextDisabled(T("Only the first %zu matches are shown."), kMapResults);
    }

    void DrawFilters(Node& aMacro)
    {
        const Here here = WhereAmI();
        const bool active = IsActiveHere(aMacro, here);
        // "###filters" keeps the header's open state while its label changes.
        const std::string header = Fmt(T("Where it's active: %s"), FilterSummary(aMacro).c_str()) +
                                   (active ? std::string() : std::string("  ") + T("(not here)")) + "###filters";
        if (!active) ImGui::PushStyleColor(ImGuiCol_Text, kWarnColor);
        const bool open = ImGui::CollapsingHeader(header.c_str());
        if (!active) ImGui::PopStyleColor();
        if (!open) return;
        ImGui::Indent();
        ImGui::TextWrapped(T("Hotkeys and the quick menu only use a macro where it's active. "
                           "Send now here in the library always works."));

        // Game modes. At least one stays ticked: "none" would mean the macro can never be used.
        if (aMacro.GameModes == GM_None) aMacro.GameModes = GM_All;
        UiFont::Row modesRow;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("Game modes:"));
        const uint8_t bits[] = {GM_PvE, GM_WvW, GM_PvP};
        for (uint8_t b : bits)
        {
            modesRow.Next(UiFont::CheckboxWidth(ModeName(b)));
            bool on = (aMacro.GameModes & b) != 0;
            if (ImGui::Checkbox(ModeName(b), &on))
            {
                const uint8_t next = on ? static_cast<uint8_t>(aMacro.GameModes | b)
                                        : static_cast<uint8_t>(aMacro.GameModes & ~b);
                if (next == GM_None)
                    ShowError(Fmt(T("Unticked %s under \"Where it's active\" for \"%s\""), ModeName(b), aMacro.Name.c_str()),
                              T("A macro needs at least one game mode."));
                else
                {
                    aMacro.GameModes = next;
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
        if (aMacro.MapIds.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled(T("Any map"));
        }
        int removeAt = -1;
        for (size_t i = 0; i < aMacro.MapIds.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            const uint32_t mapId = static_cast<uint32_t>(aMacro.MapIds[i]);
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
            aMacro.MapIds.erase(aMacro.MapIds.begin() + removeAt);
            MarkDirty();
        }

        auto hasMap = [&](int aId) {
            for (int m : aMacro.MapIds)
                if (m == aId) return true;
            return false;
        };
        auto addMap = [&](int aId) {
            const std::string action = Fmt(T("Added map id %d to \"%s\""), aId, aMacro.Name.c_str());
            if (aId <= 0) ShowError(action, T("Map ids are positive numbers."));
            else if (hasMap(aId)) ShowError(action, T("That map is already in the list."));
            else if (aMacro.MapIds.size() >= kMaxMapIds)
                ShowError(action, Fmt(T("A macro can list at most %zu maps."), kMaxMapIds));
            else
            {
                aMacro.MapIds.push_back(aId);
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
                a[i].SquadBroadcast != b[i].SquadBroadcast) return false;
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
        else ImGui::NewLine(); // callers put this after SameLine: end the row
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
    bool MakeExportText(const std::vector<const Node*>& aNodes, bool aAsText, std::string& aText)
    {
        try
        {
            aText = aAsText ? Exchange::ExportText(*aNodes.front()) : Exchange::ExportJsonText(aNodes);
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
        std::string text;
        const bool made = MakeExportText(aNodes, aAsText, text);
        const std::filesystem::path file = Storage::FreeFile(dir, Exchange::FileNameFor(aName, aAsText ? ".txt" : ".json"));
        if (made && Storage::WriteText(file, text))
            SetInfo(WithTrashNote(Fmt(T("Exported to %s."), ("<GW2>\\addons\\GW2_Chat_Messages\\exports\\" + PathText::Utf8(file.filename())).c_str())), true);
        else ShowError(Fmt(T("Exported \"%s\" as a %s file"), aName.c_str(), aAsText ? ".txt" : ".json"),
                       T("The export couldn't be written. Check the Nexus log."));
    }

    void ExportToClipboard(const std::vector<const Node*>& aNodes, bool aAsText)
    {
        if (aNodes.empty() || !aNodes.front()) return;
        std::string text;
        if (!MakeExportText(aNodes, aAsText, text))
        {
            ShowError(Fmt(T("Exported \"%s\""), aNodes.front()->Name.c_str()), T("The export couldn't be written. Check the Nexus log."));
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

    // Window sizes that follow the text size (UiFont).
    UiFont::WindowFit s_MainFit;
    UiFont::WindowFit s_ImportFit;

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
            ImGui::Separator();
        }
        else
        {
            if (ImGui::MenuItem(T("Send now"), nullptr, false, !Sender::IsBusy())) SendMacro(aNode); // doesn't change the tree
            ImGui::Separator();
        }
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
        return std::string("  ") + T("(found in its lines)");
    }

    void HighlightSearch(const Node& aNode)
    {
        if (!s_Search.Active || !s_Search.Matches.count(aNode.Id)) return;
        size_t at = 0, len = 0;
        if (!Search::FindRange(aNode.Name, s_SearchBuf, at, len)) return;
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const char* name = aNode.Name.c_str();
        const float x0 = min.x + ImGui::GetTreeNodeToLabelSpacing() + ImGui::CalcTextSize(name, name + at).x;
        const float x1 = x0 + ImGui::CalcTextSize(name + at, name + at + len).x;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x0, min.y), ImVec2(x1, max.y), kSearchHighlight, 2.0f);
    }

    void DrawNode(Node& aNode)
    {
        if (!s_Search.Shows(aNode.Id)) return;
        ImGui::PushID(static_cast<int>(aNode.Id ^ (aNode.Id >> 32)));
        ImGui::PushID(static_cast<int>(aNode.Id & 0xFFFFFFFF));

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (aNode.Id == s_SelectedId) flags |= ImGuiTreeNodeFlags_Selected;
        const std::string suffix = SearchSuffix(aNode);

        bool open = false;
        if (aNode.IsFolder())
        {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
            if (aNode.Children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
            // A folder with a match inside is held open while searching; its own open state isn't
            // changed, so clearing the search brings the tree back as it was.
            const bool forced = s_Search.ForcedOpen(aNode.Id);
            ImGui::SetNextItemOpen(forced || aNode.Expanded, ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_Text, kFolderColor);
            open = ImGui::TreeNodeEx("##node", flags, "%s%s", aNode.Name.c_str(), suffix.c_str());
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
            if (!IsActiveHere(aNode, WhereAmI())) color.w *= 0.45f; // filtered out here: dimmed
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (aNode.Hotkey.Empty()) ImGui::TreeNodeEx("##node", flags, "%s%s", aNode.Name.c_str(), suffix.c_str());
            else if (Hotkey::NeedsModifier(aNode.Hotkey))
                ImGui::TreeNodeEx("##node", flags, T("%s  [%s: off, needs Ctrl/Alt/Shift]%s"), aNode.Name.c_str(),
                                  Hotkey::Display(aNode.Hotkey).c_str(), suffix.c_str());
            else ImGui::TreeNodeEx("##node", flags, "%s  [%s]%s", aNode.Name.c_str(), Hotkey::Display(aNode.Hotkey).c_str(), suffix.c_str());
            ImGui::PopStyleColor();
        }
        HighlightSearch(aNode);

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
                ImGui::SetTooltip(T("Deleted %s from %s"), t.DeletedAt.c_str(), Tree::FromPathText(t).c_str());
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
        if (s_Tree.MoveToTrash(aNode, NowText().substr(0, 16)).Ok)
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
        ImGui::TextDisabled(T("Deleted %s from %s"), aItem.DeletedAt.c_str(), Tree::FromPathText(aItem).c_str());
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
            for (const Line& l : n.Lines) ImGui::TextColored(ChannelColorVec(l.Channel), "%s", ChatLine::TypedText(l).c_str());
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

    bool ChannelCombo(const char* aLabel, EChannel& aChannel)
    {
        bool changed = false;
        ImGui::PushStyleColor(ImGuiCol_Text, ChannelColorVec(aChannel));
        const bool open = ImGui::BeginCombo(aLabel, Localization::Lookup(ChannelLabel(aChannel)));
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
    // The character counter: "23/199" (with " characters" and a warning when aLong), grey, then
    // yellow from 150, bright red from 180 (ChatLine::LengthLevel). Placeholders make it "~".
    void LengthLabel(const Line& aLine, bool aLong = false)
    {
        const std::string typed = ChatLine::TypedText(aLine);
        const size_t len = ChatLine::Utf16Length(typed);
        const bool placeholders = Placeholders::HasPlaceholders(typed);
        const ChatLine::ELengthLevel level = ChatLine::LengthLevel(len);
        const std::string tail = !aLong ? std::string()
                         : std::string(" ") + (level == ChatLine::ELengthLevel::Over ? T("characters: too long, this line won't be sent")
                                             : level == ChatLine::ELengthLevel::Close ? T("characters: nearly at the limit")
                                                                                      : T("characters"));
        // With placeholders the final length is only known when sending: show it as approximate.
        const char* approx = placeholders ? "~" : "";
        if (level == ChatLine::ELengthLevel::Normal)
            ImGui::TextDisabled("%s%zu/%zu%s", approx, len, ChatLine::kMaxChatLength, tail.c_str());
        else
            ImGui::TextColored(level == ChatLine::ELengthLevel::Near ? kCountNearColor : kCountCloseColor,
                               "%s%zu/%zu%s", approx, len, ChatLine::kMaxChatLength, tail.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Characters typed into chat, including the channel command.\n"
                              "Chat allows at most %zu. Longer lines won't be sent.%s"), ChatLine::kMaxChatLength,
                              placeholders ? (std::string("\n") + T("This line has placeholders: they're filled in when it's sent,\n"
                                                                    "and the real length is checked then.")).c_str() : "");
    }

    void LineWarnings(const Line& aLine)
    {
        if (aLine.Channel == EChannel::Whisper && aLine.WhisperTo.empty())
            ImGui::TextColored(kWarnColor, T("Whisper needs a recipient."));
        if (aLine.Message.empty())
            ImGui::TextColored(kWarnColor, T("Empty message."));
        if (Placeholders::UsesRemoved(aLine.Message) || Placeholders::UsesRemoved(aLine.WhisperTo))
            ImGui::TextColored(kErrorColor, T("{txt} and {json} aren't available any more: remove them, or this macro won't be sent."));
    }

    enum class EEditMode { Lines, Raw };
    EEditMode   s_EditMode   = EEditMode::Lines;
    std::string s_RawText;
    uint64_t    s_RawForId   = 0; // which macro s_RawText was built from (0 = rebuild)

    void DrawLinesEditor(Node& aMacro)
    {
        auto& lines = aMacro.Lines;
        int moveFrom = -1, moveTo = -1, removeAt = -1;

        for (int i = 0; i < static_cast<int>(lines.size()); i++)
        {
            Line& l = lines[static_cast<size_t>(i)];
            ImGui::PushID(i);

            UiFont::Row row;
            if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0) { moveFrom = i; moveTo = i - 1; }
            ImGui::SameLine(0.0f, 2.0f);
            if (ImGui::ArrowButton("##down", ImGuiDir_Down) && i + 1 < static_cast<int>(lines.size())) { moveFrom = i; moveTo = i + 1; }
            row.Next(Px(110.0f));
            ImGui::SetNextItemWidth(Px(110.0f));
            if (ChannelCombo("##channel", l.Channel)) MarkDirty();
            if (l.Channel == EChannel::Whisper)
            {
                row.Next(Px(150.0f));
                ImGui::SetNextItemWidth(Px(150.0f));
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
            row.Next(UiFont::ButtonWidth(T("Remove")));
            if (ImGui::SmallButton(T("Remove"))) removeAt = i;

            ImGui::SetNextItemWidth(-1.0f);
            if (InputString("##msg", T("Message"), l.Message)) MarkDirty();
            LengthLabel(l, true); // under the message box
            LineWarnings(l);

            ImGui::PopID();
            ImGui::Separator();
        }

        // Apply after the loop so the vector isn't changed while it's being drawn.
        if (removeAt >= 0) { lines.erase(lines.begin() + removeAt); MarkDirty(); }
        else if (moveFrom >= 0) { std::swap(lines[static_cast<size_t>(moveFrom)], lines[static_cast<size_t>(moveTo)]); MarkDirty(); }

        if (lines.size() < kMaxLinesPerMacro)
        {
            if (ImGui::Button(T("Add line")))
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
    // A macro can be kept in step with a .txt file (src/LinkSync.*). The files are checked once a
    // second on the render thread, outside the tree drawing; only a changed file is read.

    constexpr double kLinkSyncEverySec = 1.0;
    constexpr char   kLinkMissing[]    = TX("The file isn't there. Changes aren't saved to it until it's back or recreated.");

    std::unordered_map<uint64_t, LinkSync::State> s_LinkStates;
    std::unordered_map<uint64_t, std::string>     s_LinkProblems; // what's wrong with a link, for the editor
    double      s_NextLinkSync = 0.0;
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

    void SyncLinks(bool aNow)
    {
        if (!aNow && ImGui::GetTime() < s_NextLinkSync) return;
        s_NextLinkSync = ImGui::GetTime() + kLinkSyncEverySec;

        std::vector<Node*> linked;
        CollectLinked(s_Tree.Root(), linked);
        // Forget macros that were unlinked or deleted.
        for (auto it = s_LinkStates.begin(); it != s_LinkStates.end();)
        {
            bool still = false;
            for (const Node* n : linked) still = still || n->Id == it->first;
            it = still ? std::next(it) : s_LinkStates.erase(it);
        }
        if (linked.empty()) return;
        const std::filesystem::path dir = Storage::AddonDir();
        if (dir.empty()) return;

        for (Node* m : linked)
        {
            const std::filesystem::path file = LinkSync::Resolve(dir, m->LinkFile);
            LinkSync::State& st = s_LinkStates[m->Id];
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
        if (s_LinkPathFor != aMacro.Id)
        {
            s_LinkPathFor = aMacro.Id;
            s_LinkPath.clear();
        }
        const auto problem = s_LinkProblems.find(aMacro.Id);
        const std::string header = aMacro.LinkFile.empty()
            ? std::string(T("Text file link: none")) + "###linkfile"
            : Fmt(T("Text file link: %s"), aMacro.LinkFile.c_str()) +
                  (problem != s_LinkProblems.end() ? std::string(" ") + T("(needs attention)") : std::string()) + "###linkfile";
        if (!ImGui::CollapsingHeader(header.c_str())) return;
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

    void DrawMacroEditor(Node& aMacro)
    {
        const bool busy = Sender::IsBusy();
        if (busy) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        if (ImGui::Button(T("Send now")) && !busy) SendMacro(aMacro);
        if (busy) ImGui::PopStyleVar();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Types each line into its chat channel and sends it.\n"
                              "Needs your Chat key set in Nexus > Options > Game Keybinds."));
        ImGui::SameLine();
        if (ImGui::Button(T("Preview"))) MakePreview(aMacro);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Shows what Send now would type, with placeholders filled in,\n"
                              "and runs the same checks. Nothing is sent."));
        ImGui::SameLine();
        DrawSendStatus();
        DrawPreview(aMacro);
        DrawHotkeyRow(aMacro);
        DrawFilters(aMacro);
        DrawLinkFile(aMacro);
        ImGui::Separator();

        UiFont::Row modeRow;
        int mode = static_cast<int>(s_EditMode);
        if (ImGui::RadioButton(T("Lines"), &mode, static_cast<int>(EEditMode::Lines))) s_RawForId = 0;
        modeRow.Next(UiFont::CheckboxWidth(T("Raw text")));
        if (ImGui::RadioButton(T("Raw text"), &mode, static_cast<int>(EEditMode::Raw))) s_RawForId = 0;
        s_EditMode = static_cast<EEditMode>(mode);
        const std::string count = Fmt(T("%zu line(s)"), aMacro.Lines.size());
        modeRow.Next(UiFont::TextWidth(count.c_str()));
        ImGui::TextDisabled("%s", count.c_str());
        modeRow.Next(UiFont::TextWidth(T("Placeholders (?)")));
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
        ImGui::Spacing();

        if (s_EditMode == EEditMode::Lines) DrawLinesEditor(aMacro);
        else DrawRawEditor(aMacro);
    }

    // --- Details pane -------------------------------------------------------------------------

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

        ImGui::TextDisabled("%s", node->IsFolder() ? T("Folder") : T("Macro"));
        ImGui::TextWrapped("%s", node->Name.c_str());
        ImGui::TextDisabled(T("In: %s"), PathOf(*node).c_str());
        // Popups must be opened at the main window's ID level, so these buttons queue an action.
        UiFont::Row nameRow;
        if (ImGui::SmallButton(T("Rename"))) s_Pending = {EAction::OpenName, 0, node->Id, EDrop::Into, 0, ENameMode::Rename};
        const char* deleteLabel = s_TrashOn ? T("Delete") : T("Delete...");
        nameRow.Next(UiFont::ButtonWidth(deleteLabel));
        if (ImGui::SmallButton(deleteLabel)) s_Pending = {EAction::OpenDelete, node->Id};
        if (s_TrashOn && ImGui::IsItemHovered()) ImGui::SetTooltip(T("Moves it to the trash bin, where it can be restored."));
        ImGui::Separator();

        if (node->IsFolder())
        {
            size_t folders = 0, macros = 0;
            Tree::CountDescendants(*node, &folders, &macros);
            const int level = Tree::FolderLevel(*node);
            ImGui::Text(T("Folder level %d of %d"), level, kMaxFolderDepth);
            ImGui::Text(T("Contains %zu folder(s) and %zu macro(s)"), folders, macros);
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
    bool HasActiveMacros(const Node& aFolder, const Here& aHere)
    {
        for (const auto& c : aFolder.Children)
            if (c->IsMacro() ? IsActiveHere(*c, aHere) : HasActiveMacros(*c, aHere)) return true;
        return false;
    }

    void MacroPreviewTooltip(const Node& aMacro)
    {
        ImGui::BeginTooltip();
        UiFont::PushTooltipWrap(); // a chat line can be 199 characters long
        if (aMacro.Lines.empty()) ImGui::TextDisabled(T("No lines yet."));
        for (size_t i = 0; i < aMacro.Lines.size() && i < kPreviewLines; i++)
        {
            const Line& l = aMacro.Lines[i];
            ImGui::TextColored(ChannelColorVec(l.Channel), "%s", ChatLine::TypedText(l).c_str());
        }
        if (aMacro.Lines.size() > kPreviewLines)
            ImGui::TextDisabled(T("...and %zu more line(s)"), aMacro.Lines.size() - kPreviewLines);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    // One macro entry: coloured by its first line's channel, its hotkey on the right, the preview
    // on hover. Clicking sends it (sending doesn't change the tree).
    void DrawMenuMacro(const Node& n, const char* aLabel, bool aBusy)
    {
        const EChannel first = n.Lines.empty() ? EChannel::Current : n.Lines.front().Channel;
        const std::string keys = Hotkey::Fires(n.Hotkey) ? Hotkey::Display(n.Hotkey) : std::string(); // an off hotkey isn't shown
        ImGui::PushStyleColor(ImGuiCol_Text, ChannelColorVec(first));
        const bool clicked = ImGui::MenuItem(aLabel, keys.empty() ? nullptr : keys.c_str(), false, !aBusy);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) MacroPreviewTooltip(n);
        if (clicked) SendMacro(n, ESendFrom::Menu);
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
        Search::Where where;
        where.ActiveOnly = true;
        where.Mode       = aHere.Mode;
        where.MapId      = aHere.MapId;
        Search::Run(s_Tree.Root(), aQuery, where, s_MenuResult);
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
            SendMacro(*found.front(), ESendFrom::Menu);
            ImGui::CloseCurrentPopup();
        }
    }

    void DrawQuickMenu(MenuSearch& aSearch)
    {
        const int frame = ImGui::GetFrameCount();
        if (aSearch.LastFrame != frame - 1 && aSearch.LastFrame != frame) aSearch.Buf[0] = '\0';
        aSearch.LastFrame = frame;

        const Here here = WhereAmI();
        if (!HasActiveMacros(s_Tree.Root(), here))
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
        if (ImGui::RadioButton(T("Macros"), !s_ImportLibrary)) SetImportLibrary(false);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("A .json export, a .txt file (one macro), or a folder of .txt files."));
        ImGui::SameLine();
        if (ImGui::RadioButton(T("A whole library"), s_ImportLibrary)) SetImportLibrary(true);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Someone else's macros.json (or macros.json.bak), as they sent it to you.\n"
                              "It goes into a new folder of its own, so it never mixes with your folders."));
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
        ImGui::Checkbox(T("Keep hotkeys"), &s_ImportOpt.KeepHotkeys);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", s_ImportLibrary ? T("Off by default for someone else's library, so their keys don't clash with yours.")
                                                    : T("Off at first, so macros from someone else come without their hotkeys. Tick it for your own exports; the preview then shows each macro's hotkey."));
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
        if (ImGui::SmallButton(T("Undo import")))
        {
            std::vector<std::string> ignored;
            s_Tree.FromJson(*s_UndoImport, &ignored);
            s_UndoImport.reset();
            s_SelectedId = 0;
            s_TrashSelId = 0; // the bin goes back to how it was before the import too
            s_RawForId   = 0;
            CancelCapture();
            MarkDirty();
            SetInfo(T("The import was undone."));
            return;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(T("Puts your library back as it was before the import.\nChanges you made since then are undone too."));
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

    // Works out the search for this frame. A selected item that's no longer shown is unselected.
    void UpdateSearch()
    {
        const Here here = WhereAmI();
        Search::Where where;
        where.ActiveOnly = s_ActiveOnly;
        where.Mode       = here.Mode;
        where.MapId      = here.MapId;
        Search::Run(s_Tree.Root(), s_SearchBuf, where, s_Search);
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
        DrawToolbar();
        ImGui::Separator();

        // Both panes scroll sideways too, for long names and rows that can't wrap.
        // The search box sits above the tree; the group keeps the details pane level with it.
        const float treeWidth = ImGui::GetContentRegionAvail().x * 0.38f;
        UpdateSearch();
        ImGui::BeginGroup();
        DrawSearchBox(treeWidth);
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

        ImGui::PopTextWrapPos();
        ImGui::End();
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
        if (!st.Refreshing)
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

void Ui::Init()
{
    UiFont::Init();
    {
        const nlohmann::json settings = Storage::LoadSettings();
        const auto it = settings.find(kTrashSetting);
        s_TrashOn = it == settings.end() || !it->is_boolean() || it->get<bool>(); // on by default
        const auto active = settings.find(kActiveOnlySetting);
        s_ActiveOnly = active != settings.end() && active->is_boolean() && active->get<bool>(); // off by default
    }
    s_TrashSelId = 0;
    // The imports and exports folders exist from the start, so there's never one to make by hand.
    if (Storage::ImportsDir().empty() || Storage::ExportsDir().empty())
        LogF(LOGL_WARNING, "The imports and exports folders couldn't be created.");
    s_Mumble = static_cast<Mumble::Data*>(g_Api->DataLink_Get(DL_MUMBLE_LINK));
    s_LoadMessages.clear();
    Storage::LoadMacros(s_Tree, s_LoadMessages);
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
    if (s_Dirty && !Storage::SaveMacros(s_Tree))
        LogF(LOGL_WARNING, "Unsaved macro changes could not be written on unload.");
    s_Dirty = false;
    s_Tree  = Tree();
    s_SelectedId = 0;
    CloseImport();
    s_UndoImport.reset();
    s_LinkStates.clear();
    s_LinkProblems.clear();
    s_Preview = {};
}

void Ui::Render()
{
    ShowLanguageProblems(); // after a reload (Languages::Update ran before this)
    PollSender();
    WatchMap();
    SyncLinks(false);
    HandleHotkeys();
    UiFont::NewFrame();
    UiFont::Push(); // the chosen text size, for our windows only
    if (s_ShowMainWindow) DrawMainWindow();
    DrawImportWindow();
    DrawQuickMenuPopup();
    DrawErrorWindow();
    UiFont::Pop();
    if (s_Dirty && ImGui::GetTime() >= s_SaveAt) SaveNow();
}

void Ui::RenderOptions()
{
    ImGui::Text(T("Chat Messages %s"), kVersionText);
    ImGui::TextDisabled(T("Build %s"), BuildStamp::Text());
    DrawLink(T("Chat Messages on GitHub"), kRepositoryUrl);
    DrawLink(T("ArenaNet's macro policy"), kMacroPolicyUrl);
    ImGui::TextDisabled(T("This addon only sends chat messages you start yourself. Don't use it to automate gameplay."));
    ImGui::TextDisabled(T("Your macros are saved in <GW2>\\addons\\GW2_Chat_Messages\\macros.json."));
    if (ImGui::Button(T("Open macro library"))) s_ShowMainWindow = true;
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
    ImGui::TextWrapped(T("Quick menu: right-click the scroll icon in the Nexus bar, or set a key for "
                       "\"Open Chat Messages quick menu\" in Nexus > Options > Keybinds."));
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
