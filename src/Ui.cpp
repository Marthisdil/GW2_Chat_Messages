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

#include "ChatLine.h"
#include "ErrorReport.h"
#include "Exchange.h"
#include "Globals.h"
#include "Hotkeys.h"
#include "LinkSync.h"
#include "MapNames.h"
#include "MacroTree.h"
#include "Mumble.h"
#include "Placeholders.h"
#include "Sender.h"
#include "Storage.h"
#include "UiFont.h"

using namespace MacroTree;
using UiFont::Px;

namespace
{
    constexpr const char* kDragPayload     = "CHATMSG_NODE";
    constexpr const char* kNamePopup       = "Name##chatmsg";
    constexpr const char* kDeletePopup     = "Delete?##chatmsg";
    constexpr double      kSaveDelaySec    = 0.75; // batch quick edits (e.g. several expands) into one write
    constexpr double      kStatusShowSec   = 6.0;

    const ImVec4 kWarnColor  {1.00f, 0.80f, 0.35f, 1.0f};
    const ImVec4 kErrorColor {1.00f, 0.45f, 0.40f, 1.0f};
    const ImVec4 kFolderColor{0.88f, 0.86f, 0.70f, 1.0f};

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

    uint64_t s_SelectedId = 0;

    // Name popup (new folder, new macro, rename).
    enum class ENameMode { NewFolder, NewMacro, Rename };
    ENameMode   s_NameMode     = ENameMode::NewFolder;
    uint64_t    s_NameTargetId = 0; // parent folder for New*, the node for Rename (0 = root)
    char        s_NameBuf[256] = {};
    bool        s_NameFocus    = false;

    uint64_t s_DeleteId = 0;

    // Requests raised while drawing the tree, handled after it (never change the tree mid-draw).
    enum class EAction { None, Move, Shift, Sort, OpenName, OpenDelete, ExpandAll };
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
            ShowError("Changed the library (it's saved to macros.json automatically)",
                      "Your macros couldn't be saved. Check the Nexus log. The addon keeps trying every 5 seconds.", false);
    }

    // Good news ("Exported to ..."), shown for a few seconds under the toolbar.
    void SetInfo(const std::string& aText)
    {
        s_Status      = aText;
        s_StatusUntil = ImGui::GetTime() + kStatusShowSec;
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
        case GM_PvE: return "PvE";
        case GM_WvW: return "WvW";
        case GM_PvP: return "PvP";
        default:     return "no game mode";
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
        if (aMacro.MapIds.empty()) return modes.empty() ? "Everywhere" : modes;
        const std::string maps = aMacro.MapIds.size() == 1 ? "1 map" : std::to_string(aMacro.MapIds.size()) + " maps";
        return modes.empty() ? "On " + maps : modes + " on " + maps;
    }

    // --- Errors -----------------------------------------------------------------------------------
    // Every error opens the error window: what went wrong, what the user did, OK, and Copy to
    // clipboard (src/ErrorReport.*). It's a normal window, not an ImGui pop-up: while a pop-up is
    // open ImGui takes every mouse click, which would block the game after a hotkey error. Only an
    // error inside our Name pop-up is shown as a pop-up within it (a pop-up blocks other windows).

    constexpr const char* kErrorWindow = "Chat Messages: error##chatmsg_error";
    constexpr const char* kErrorPopup  = "Chat Messages: error##chatmsg_errorpopup";

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
        return std::string(ModeName(h.Mode)) + ", " + (h.MapId ? MapNames::Label(h.MapId) : std::string("no map"));
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
        if (s_Errors.size() > 1) ImGui::TextDisabled("Error 1 of %zu", s_Errors.size());
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kErrorColor, "%s", e.Message.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("What you did: %s", e.Action.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        const bool ok = ImGui::Button("OK", Px(100.0f, 0.0f));
        ImGui::SameLine();
        if (ImGui::Button("Copy to clipboard"))
        {
            ImGui::SetClipboardText(ErrorReport::ToText(e, kVersionText).c_str());
            s_ErrorCopiedUntil = ImGui::GetTime() + 3.0;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Copies the error, what you did, the time and where you were in game,\nready to paste into a bug report.");
        if (ImGui::GetTime() < s_ErrorCopiedUntil)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("Copied.");
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
        if (ImGui::Begin(kErrorWindow, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
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
        if (ImGui::BeginPopupModal(kErrorPopup, nullptr, ImGuiWindowFlags_NoResize))
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
        const std::string action = aFrom == ESendFrom::Hotkey ? "Pressed the hotkey " + Hotkey::ToString(aMacro.Hotkey) + " to send " + name
                                 : aFrom == ESendFrom::Menu   ? "Sent " + name + " from the quick menu"
                                                              : "Sent " + name + " from the library (Send now)";
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
        MapNames::Request(id);
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
                if (!why.empty()) ShowError("Set a hotkey for \"" + m->Name + "\" and pressed " + Hotkey::ToString(combo), why);
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
            // Only macros active here count: the same hotkey can be used for, say, a WvW macro and a PvE one.
            const Here here = WhereAmI();
            std::vector<Node*> macros;
            for (Node* m : s_Tree.MacrosWithHotkey(combo))
                if (IsActiveHere(*m, here)) macros.push_back(m);
            if (macros.empty()) continue;
            if (macros.size() > 1)
            {
                const std::string why = Hotkey::ToString(combo) + " is set on " + std::to_string(macros.size()) +
                                        " macros, so nothing was sent. Give each macro its own hotkey.";
                ShowError("Pressed the hotkey " + Hotkey::ToString(combo), why, false);
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
        ImGui::TextUnformatted("Hotkey:");
        if (s_CaptureFor == aMacro.Id && Hotkeys::IsCapturing())
        {
            ImGui::SameLine();
            ImGui::TextColored(kWarnColor, "Press a key or mouse button (Escape cancels)...");
            row.Next(UiFont::ButtonWidth("Cancel"));
            if (ImGui::SmallButton("Cancel")) CancelCapture();
            return;
        }

        const std::string keys = aMacro.Hotkey.Empty() ? std::string("None") : Hotkey::ToString(aMacro.Hotkey);
        row.Next(UiFont::TextWidth(keys.c_str()));
        if (aMacro.Hotkey.Empty()) ImGui::TextDisabled("None");
        else ImGui::TextUnformatted(keys.c_str());
        const char* setLabel = aMacro.Hotkey.Empty() ? "Set..." : "Change...";
        row.Next(UiFont::ButtonWidth(setLabel));
        if (ImGui::SmallButton(setLabel))
        {
            s_CaptureFor = aMacro.Id;
            Hotkeys::StartCapture();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Then press the key you want, with Ctrl, Alt or Shift if you like.\n"
                              "Mouse 3 (middle), Mouse 4 and Mouse 5 work too.\n"
                              "The key still does its normal job in game, so pick one you don't use there.\n"
                              "Hotkeys don't fire while you're typing in chat or in a text box.");
        if (!aMacro.Hotkey.Empty())
        {
            row.Next(UiFont::ButtonWidth("Clear##hotkey"));
            if (ImGui::SmallButton("Clear##hotkey"))
            {
                aMacro.Hotkey = {};
                MarkDirty();
            }
            // A shared hotkey is only a conflict where both macros can be active at once.
            std::string names;
            for (const Node* n : s_Tree.MacrosWithHotkey(aMacro.Hotkey))
                if (n != &aMacro && FiltersOverlap(aMacro, *n)) names += (names.empty() ? "" : ", ") + n->Name;
            if (!names.empty())
            {
                ImGui::TextColored(kWarnColor, "Also set on: %s. Where both are active, this hotkey sends nothing.",
                                   names.c_str());
            }
            else if (!aMacro.Hotkey.HasModifier() && aMacro.Hotkey.Key < 0x60 && aMacro.Hotkey.Key > Hotkey::kVkXButton2)
            {
                ImGui::TextColored(kWarnColor, "Plain letters, numbers and keys like Space are usually game keys. "
                                               "Adding Ctrl, Alt or Shift is safer.");
            }
        }
    }

    int s_MapIdInput = 0;

    void DrawFilters(Node& aMacro)
    {
        const Here here = WhereAmI();
        const bool active = IsActiveHere(aMacro, here);
        // "###filters" keeps the header's open state while its label changes.
        const std::string header = "Where it's active: " + FilterSummary(aMacro) + (active ? "" : "  (not here)") + "###filters";
        if (!active) ImGui::PushStyleColor(ImGuiCol_Text, kWarnColor);
        const bool open = ImGui::CollapsingHeader(header.c_str());
        if (!active) ImGui::PopStyleColor();
        if (!open) return;
        ImGui::Indent();
        ImGui::TextWrapped("Hotkeys and the quick menu only use a macro where it's active. "
                           "Send now here in the library always works.");

        // Game modes. At least one stays ticked: "none" would mean the macro can never be used.
        if (aMacro.GameModes == GM_None) aMacro.GameModes = GM_All;
        UiFont::Row modesRow;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Game modes:");
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
                    ShowError("Unticked " + std::string(ModeName(b)) + " under \"Where it's active\" for \"" + aMacro.Name + "\"",
                              "A macro needs at least one game mode.");
                else
                {
                    aMacro.GameModes = next;
                    MarkDirty();
                }
            }
            if (ImGui::IsItemHovered())
            {
                if (b == GM_WvW) ImGui::SetTooltip("Includes Armistice Bastion and Edge of the Mists.");
                else if (b == GM_PvE) ImGui::SetTooltip("Open world, story, fractals, raids and strikes.");
                else ImGui::SetTooltip("PvP maps, including the Heart of the Mists lobby.");
            }
        }

        // Maps: one button per map, flowing onto more rows when there are many.
        UiFont::Row mapsRow;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Maps:");
        if (aMacro.MapIds.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("Any map");
        }
        int removeAt = -1;
        for (size_t i = 0; i < aMacro.MapIds.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            const std::string name = MapNames::Label(static_cast<uint32_t>(aMacro.MapIds[i]));
            const std::string label = name + " x";
            mapsRow.Next(UiFont::ButtonWidth(label.c_str()));
            if (ImGui::SmallButton(label.c_str())) removeAt = static_cast<int>(i);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove %s", name.c_str());
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
            const std::string action = "Added map id " + std::to_string(aId) + " to \"" + aMacro.Name + "\"";
            if (aId <= 0) ShowError(action, "Map ids are positive numbers.");
            else if (hasMap(aId)) ShowError(action, "That map is already in the list.");
            else if (aMacro.MapIds.size() >= kMaxMapIds)
                ShowError(action, "A macro can list at most " + std::to_string(kMaxMapIds) + " maps.");
            else
            {
                aMacro.MapIds.push_back(aId);
                MarkDirty();
            }
        };

        const int current = static_cast<int>(here.MapId);
        const bool canAddCurrent = current > 0 && !hasMap(current);
        if (!canAddCurrent) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        const std::string addCurrent = current > 0 ? "Add this map: " + MapNames::Label(here.MapId) : "Add this map";
        UiFont::Row addRow;
        if (ImGui::SmallButton(addCurrent.c_str()) && canAddCurrent) addMap(current);
        if (!canAddCurrent) ImGui::PopStyleVar();
        addRow.Next(Px(90.0f));
        ImGui::SetNextItemWidth(Px(90.0f));
        ImGui::InputInt("##mapid", &s_MapIdInput, 0, 0);
        addRow.Next(UiFont::ButtonWidth("Add id"));
        if (ImGui::SmallButton("Add id")) addMap(s_MapIdInput);
        ImGui::TextDisabled("Map names come from the GW2 API. Ids are also listed on the GW2 wiki.");

        const std::string where = here.MapId ? MapNames::Label(here.MapId) : std::string("no map");
        if (active) ImGui::TextDisabled("Active here (%s, %s).", ModeName(here.Mode), where.c_str());
        else ImGui::TextColored(kWarnColor, "Not active here (%s, %s).", ModeName(here.Mode), where.c_str());
        ImGui::Unindent();
    }

    void DrawSendStatus()
    {
        const Sender::Status st = Sender::GetStatus();
        if (st.State == Sender::EState::Sending)
        {
            if (st.Line == 0) ImGui::TextColored(kWarnColor, "Preparing..."); // filling in placeholders
            else ImGui::TextColored(kWarnColor, "Sending line %zu of %zu...", st.Line, st.Lines);
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
        std::string path = "Top level";
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) path += " > " + (*it)->Name;
        return path;
    }

    // --- Export -----------------------------------------------------------------------------------
    // Writes to <addon>\exports\ (never overwriting a file) or copies to the clipboard. Doesn't
    // change the tree, so it's safe from the tree's context menu.

    void ExportToFile(const std::vector<const Node*>& aNodes, const std::string& aName, bool aAsText)
    {
        const std::filesystem::path dir = Storage::ExportsDir();
        if (dir.empty() || aNodes.empty() || !aNodes.front())
        {
            ShowError("Exported \"" + aName + "\"", "The exports folder isn't available.");
            return;
        }
        const std::string text = aAsText ? Exchange::ExportText(*aNodes.front()) : Exchange::ExportJsonText(aNodes);
        const std::filesystem::path file = Storage::FreeFile(dir, Exchange::FileNameFor(aName, aAsText ? ".txt" : ".json"));
        if (Storage::WriteText(file, text))
            SetInfo("Exported to <GW2>\\addons\\GW2_Chat_Messages\\exports\\" + file.filename().u8string());
        else ShowError("Exported \"" + aName + "\" as a " + (aAsText ? ".txt" : ".json") + " file",
                       "The export couldn't be written. Check the Nexus log.");
    }

    void ExportToClipboard(const std::vector<const Node*>& aNodes, bool aAsText)
    {
        if (aNodes.empty() || !aNodes.front()) return;
        const std::string text = aAsText ? Exchange::ExportText(*aNodes.front()) : Exchange::ExportJsonText(aNodes);
        ImGui::SetClipboardText(text.c_str());
        SetInfo(aAsText ? "Copied the macro's lines to the clipboard." : "Copied to the clipboard. Paste it into the Import window to import it.");
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
                            : aMode == ENameMode::NewFolder        ? "New folder"
                                                                   : "New macro";
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
            if (ImGui::MenuItem("New folder inside", nullptr, false, canNest))
                s_Pending = {EAction::OpenName, 0, aNode.Id, EDrop::Into, 0, ENameMode::NewFolder};
            if (ImGui::MenuItem("New macro inside"))
                s_Pending = {EAction::OpenName, 0, aNode.Id, EDrop::Into, 0, ENameMode::NewMacro};
            ImGui::Separator();
        }
        else
        {
            if (ImGui::MenuItem("Send now", nullptr, false, !Sender::IsBusy())) SendMacro(aNode); // doesn't change the tree
            ImGui::Separator();
        }
        if (ImGui::MenuItem("Rename"))
            s_Pending = {EAction::OpenName, 0, aNode.Id, EDrop::Into, 0, ENameMode::Rename};
        if (ImGui::MenuItem("Move up"))   s_Pending = {EAction::Shift, aNode.Id, 0, EDrop::Into, -1};
        if (ImGui::MenuItem("Move down")) s_Pending = {EAction::Shift, aNode.Id, 0, EDrop::Into, +1};
        if (ImGui::BeginMenu("Export"))
        {
            const std::vector<const Node*> one{&aNode};
            if (ImGui::MenuItem("Save as a .json file")) ExportToFile(one, aNode.Name, false);
            if (aNode.IsMacro() && ImGui::MenuItem("Save as a .txt file")) ExportToFile(one, aNode.Name, true);
            if (ImGui::MenuItem("Copy to the clipboard")) ExportToClipboard(one, false);
            if (aNode.IsMacro() && ImGui::MenuItem("Copy the lines as text")) ExportToClipboard(one, true);
            ImGui::EndMenu();
        }
        if (aNode.IsFolder() && ImGui::MenuItem("Import into this folder..."))
        {
            s_ImportTarget = aNode.Id;
            s_ShowImport   = true;
        }
        if (aNode.IsFolder())
        {
            if (ImGui::MenuItem("Sort contents A-Z")) s_Pending = {EAction::Sort, aNode.Id};
            if (ImGui::MenuItem("Expand all inside")) s_Pending = {EAction::ExpandAll, aNode.Id, 0, EDrop::Into, 0, ENameMode::NewFolder, true};
            if (ImGui::MenuItem("Collapse all inside")) s_Pending = {EAction::ExpandAll, aNode.Id, 0, EDrop::Into, 0, ENameMode::NewFolder, false};
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete...")) s_Pending = {EAction::OpenDelete, aNode.Id};
        ImGui::EndPopup();
    }

    void DrawNode(Node& aNode)
    {
        ImGui::PushID(static_cast<int>(aNode.Id ^ (aNode.Id >> 32)));
        ImGui::PushID(static_cast<int>(aNode.Id & 0xFFFFFFFF));

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (aNode.Id == s_SelectedId) flags |= ImGuiTreeNodeFlags_Selected;

        bool open = false;
        if (aNode.IsFolder())
        {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
            if (aNode.Children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
            ImGui::SetNextItemOpen(aNode.Expanded, ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_Text, kFolderColor);
            open = ImGui::TreeNodeEx("##node", flags, "%s", aNode.Name.c_str());
            ImGui::PopStyleColor();
            if (open != aNode.Expanded && !aNode.Children.empty())
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
            if (aNode.Hotkey.Empty()) ImGui::TreeNodeEx("##node", flags, "%s", aNode.Name.c_str());
            else ImGui::TreeNodeEx("##node", flags, "%s  [%s]", aNode.Name.c_str(), Hotkey::ToString(aNode.Hotkey).c_str());
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) s_SelectedId = aNode.Id;

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kDragPayload, &aNode.Id, sizeof(aNode.Id));
            UiFont::PushTooltipWrap();
            ImGui::Text("%s %s", aNode.IsFolder() ? "Folder:" : "Macro:", aNode.Name.c_str());
            if (!s_DragError.empty()) ImGui::TextColored(kErrorColor, "%s", s_DragError.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndDragDropSource();
        }
        HandleDropTarget(aNode);
        DrawContextMenu(aNode);

        if (aNode.IsFolder() && open)
        {
            for (auto& child : aNode.Children) DrawNode(*child);
            ImGui::TreePop();
        }
        ImGui::PopID();
        ImGui::PopID();
    }

    void DrawTree()
    {
        s_DragError.clear();
        for (auto& child : s_Tree.Root().Children) DrawNode(*child);

        // The empty space below the tree: drop here to move to the end of the top level,
        // click here to clear the selection.
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.y < Px(24.0f)) avail.y = Px(24.0f);
        ImGui::InvisibleButton("##tree_end", ImVec2(avail.x > 1.0f ? avail.x : 1.0f, avail.y));
        if (ImGui::IsItemClicked()) s_SelectedId = 0;
        if (ImGui::BeginDragDropTarget())
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
            else ShowError("Moved \"" + node->Name + "\" by dragging it", r.Error);
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
            s_DeleteId = a.Id;
            ImGui::OpenPopup(kDeletePopup);
            break;
        }
    }

    void DrawNamePopup()
    {
        ImGui::SetNextWindowSize(ImVec2(Px(360.0f), 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(kNamePopup, nullptr, ImGuiWindowFlags_NoResize))
        {
            s_ErrorInPopup = false; // the pop-up is gone: any error left shows in the error window
            return;
        }
        ImGui::PushTextWrapPos(0.0f);

        const char* title = s_NameMode == ENameMode::NewFolder ? "New folder name:"
                          : s_NameMode == ENameMode::NewMacro  ? "New macro title:"
                                                               : "New name:";
        ImGui::TextUnformatted(title);
        if (s_NameFocus) { ImGui::SetKeyboardFocusHere(); s_NameFocus = false; }
        ImGui::SetNextItemWidth(-1.0f);
        const bool enter = ImGui::InputText("##name", s_NameBuf, kMaxNameLength + 1,
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        const bool ok = ImGui::Button(s_NameMode == ENameMode::Rename ? "Rename" : "Create", Px(100.0f, 0.0f)) || enter;
        ImGui::SameLine();
        if (ImGui::Button("Cancel", Px(100.0f, 0.0f))) ImGui::CloseCurrentPopup();

        if (ok)
        {
            Node* target = NodeOrRoot(s_NameTargetId);
            Result r = !target ? Result::Fail("That item no longer exists.")
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
                const std::string where = target && target->Parent ? "\"" + target->Name + "\"" : std::string("the top level");
                const std::string action = s_NameMode == ENameMode::NewFolder ? "Created a folder named \"" + std::string(s_NameBuf) + "\" in " + where
                                         : s_NameMode == ENameMode::NewMacro  ? "Created a macro named \"" + std::string(s_NameBuf) + "\" in " + where
                                         : "Renamed " + (target ? "\"" + target->Name + "\"" : std::string("an item")) + " to \"" + std::string(s_NameBuf) + "\"";
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
        if (!ImGui::BeginPopupModal(kDeletePopup, nullptr, ImGuiWindowFlags_NoResize)) return;

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
            ImGui::TextWrapped("Delete the folder \"%s\"?", node->Name.c_str());
            if (folders + macros > 0)
                ImGui::TextColored(kWarnColor, "Everything inside it is deleted too: %zu folder(s) and %zu macro(s).", folders, macros);
        }
        else
        {
            ImGui::TextWrapped("Delete the macro \"%s\"?", node->Name.c_str());
        }
        ImGui::TextDisabled("This can't be undone.");

        if (ImGui::Button("Delete", Px(100.0f, 0.0f)))
        {
            if (s_SelectedId == node->Id || (s_Tree.Find(s_SelectedId) && Tree::IsAncestorOf(*node, *s_Tree.Find(s_SelectedId))))
                s_SelectedId = 0;
            s_Tree.Delete(*node);
            MarkDirty();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", Px(100.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    // --- Macro editor -------------------------------------------------------------------------

    constexpr size_t kMaxLinesPerMacro = kMaxLines; // MacroTree.h

    ImVec4 ChannelColorVec(EChannel aChannel)
    {
        const uint32_t c = ChatLine::ChannelColor(aChannel);
        return ImVec4(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, 1.0f);
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
        if (changed) // a pasted newline would split the message; chat messages are one line
            for (char& c : aStr)
                if (c == '\n' || c == '\r' || c == '\t') c = ' ';
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
        const bool open = ImGui::BeginCombo(aLabel, ChannelLabel(aChannel));
        ImGui::PopStyleColor();
        if (!open) return false;
        for (int i = 0; i < static_cast<int>(EChannel::Count); i++)
        {
            const EChannel c = static_cast<EChannel>(i);
            ImGui::PushStyleColor(ImGuiCol_Text, ChannelColorVec(c));
            if (ImGui::Selectable(ChannelLabel(c), c == aChannel)) { aChannel = c; changed = true; }
            ImGui::PopStyleColor();
            if (c == aChannel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
        return changed;
    }

    // Room to leave on the right of a message box for its LengthLabel.
    float LengthLabelWidth()
    {
        return UiFont::TextWidth("~199/199") + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    }

    // "12/199" after a message, red when too long. Returns the length.
    void LengthLabel(const Line& aLine)
    {
        const std::string typed = ChatLine::TypedText(aLine);
        const size_t len = ChatLine::Utf16Length(typed);
        const bool placeholders = Placeholders::HasPlaceholders(typed);
        // With placeholders the final length is only known when sending: show it as approximate.
        if (len > ChatLine::kMaxChatLength && !placeholders)
            ImGui::TextColored(kErrorColor, "%zu/%zu", len, ChatLine::kMaxChatLength);
        else
            ImGui::TextDisabled("%s%zu/%zu", placeholders ? "~" : "", len, ChatLine::kMaxChatLength);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Characters typed into chat, including the channel command.\n"
                              "Chat allows at most %zu. Longer lines won't be sent.%s", ChatLine::kMaxChatLength,
                              placeholders ? "\nThis line has placeholders: they're filled in when it's sent,\n"
                                             "and the real length is checked then." : "");
    }

    void LineWarnings(const Line& aLine)
    {
        if (aLine.Channel == EChannel::Whisper && aLine.WhisperTo.empty())
            ImGui::TextColored(kWarnColor, "Whisper needs a recipient.");
        if (aLine.Message.empty())
            ImGui::TextColored(kWarnColor, "Empty message.");
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
                if (InputString("##to", "Recipient", l.WhisperTo)) MarkDirty();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Character or account name to whisper.");
            }
            else if (l.Channel == EChannel::Squad)
            {
                row.Next(UiFont::CheckboxWidth("Broadcast"));
                if (ImGui::Checkbox("Broadcast", &l.SquadBroadcast)) MarkDirty();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Send as a squad broadcast. Only works while you're the commander;\n"
                                      "otherwise it's sent as a normal squad message.");
            }
            row.Next(UiFont::ButtonWidth("Remove"));
            if (ImGui::SmallButton("Remove")) removeAt = i;

            ImGui::SetNextItemWidth(-LengthLabelWidth());
            if (InputString("##msg", "Message", l.Message)) MarkDirty();
            ImGui::SameLine();
            LengthLabel(l);
            LineWarnings(l);

            ImGui::PopID();
            ImGui::Separator();
        }

        // Apply after the loop so the vector isn't changed while it's being drawn.
        if (removeAt >= 0) { lines.erase(lines.begin() + removeAt); MarkDirty(); }
        else if (moveFrom >= 0) { std::swap(lines[static_cast<size_t>(moveFrom)], lines[static_cast<size_t>(moveTo)]); MarkDirty(); }

        if (lines.size() < kMaxLinesPerMacro)
        {
            if (ImGui::Button("Add line"))
            {
                Line l;
                if (!lines.empty()) { l.Channel = lines.back().Channel; l.WhisperTo = lines.back().WhisperTo; }
                lines.push_back(std::move(l));
                MarkDirty();
            }
        }
        else
        {
            ImGui::TextDisabled("A macro can have at most %zu lines.", kMaxLinesPerMacro);
        }
    }

    void DrawRawEditor(Node& aMacro)
    {
        if (s_RawForId != aMacro.Id)
        {
            s_RawText  = ChatLine::SerializeRaw(aMacro.Lines);
            s_RawForId = aMacro.Id;
        }

        ImGui::TextDisabled("One message per row, typed as in game:");
        ImGui::TextDisabled("/s hello   /w [Name] hi   /d ! broadcast   /g3 guild 3");
        const float height = ImGui::GetTextLineHeightWithSpacing() * 8.0f;
        if (InputStringMultiline("##raw", s_RawText, ImVec2(-1.0f, height)))
        {
            // Lines follow the text as you type; the text itself is left exactly as typed.
            aMacro.Lines = ChatLine::ParseRaw(s_RawText);
            if (aMacro.Lines.size() > kMaxLinesPerMacro) aMacro.Lines.resize(kMaxLinesPerMacro);
            MarkDirty();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Will send:");
        if (aMacro.Lines.empty()) ImGui::TextDisabled("(nothing)");
        for (size_t i = 0; i < aMacro.Lines.size(); i++)
        {
            const Line& l = aMacro.Lines[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextColored(ChannelColorVec(l.Channel), "[%s]", ChannelLabel(l.Channel));
            ImGui::SameLine();
            if (l.Channel == EChannel::Whisper && !l.WhisperTo.empty())
            {
                ImGui::TextColored(ChannelColorVec(l.Channel), "to %s:", l.WhisperTo.c_str());
                ImGui::SameLine();
            }
            if (l.Channel == EChannel::Squad && l.SquadBroadcast)
            {
                ImGui::TextColored(ChannelColorVec(l.Channel), "(broadcast)");
                ImGui::SameLine();
            }
            LengthLabel(l);
            ImGui::SameLine();
            ImGui::TextWrapped("%s", l.Message.c_str());
            LineWarnings(l);
            ImGui::PopID();
        }
        if (aMacro.Lines.size() >= kMaxLinesPerMacro)
            ImGui::TextColored(kWarnColor, "Only the first %zu lines are kept.", kMaxLinesPerMacro);
    }

    // --- Link files ---------------------------------------------------------------------------
    // A macro can be kept in step with a .txt file (src/LinkSync.*). The files are checked once a
    // second on the render thread, outside the tree drawing; only a changed file is read.

    constexpr double kLinkSyncEverySec = 1.0;

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
        if (ec) { aError = "the file couldn't be read."; return false; }
        if (size > LinkSync::kMaxFileBytes) { aError = "the file is too big (over 256 KB)."; return false; }
        std::ifstream in(aFile, std::ios::binary);
        if (!in) { aError = "the file couldn't be opened (is another program locking it?)."; return false; }
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
            if (o.Missing) problem = "The file isn't there. Changes aren't saved to it until it's back or recreated.";
            else if (!o.Error.empty()) problem = "The file can't be read: " + o.Error;
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
                    problem = "The file couldn't be written. Check the Nexus log.";
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
        const std::string action = "Linked \"" + aMacro.Name + "\" to the file \"" + aPath + "\"";
        const std::string bad = LinkSync::CheckPath(aPath);
        if (!bad.empty()) { ShowError(action, bad); return; }
        const std::filesystem::path dir = Storage::AddonDir();
        if (dir.empty()) { ShowError(action, "The addon folder isn't available."); return; }
        const std::filesystem::path file = LinkSync::Resolve(dir, aPath);
        if (const Node* other = LinkedElsewhere(s_Tree.Root(), aMacro, dir, file))
        {
            ShowError(action, "\"" + other->Name + "\" is already linked to that file.");
            return;
        }

        LinkSync::State st;
        bool exists = false;
        FileMtime(file, exists);
        std::string text, error;
        if (exists && !ReadLinkFile(file, text, error))
        {
            ShowError(action, "That file can't be used: " + error); // never overwrite a file we couldn't read
            return;
        }
        const bool hasLines = exists && !ChatLine::ParseRaw(text).empty();
        if (!hasLines)
        {
            // No file yet (or an empty one): it's made from the macro.
            if (!WriteLinkFile(aMacro, file, st))
            {
                ShowError(action, "The file couldn't be created. Check the path and the Nexus log.");
                return;
            }
            SetInfo("Linked. The file was created from the macro's lines.");
        }
        else SetInfo("Linked. The macro now has the file's lines.");
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
            ? std::string("Text file link: none###linkfile")
            : "Text file link: " + aMacro.LinkFile + (problem != s_LinkProblems.end() ? " (needs attention)" : "") + "###linkfile";
        if (!ImGui::CollapsingHeader(header.c_str())) return;
        ImGui::Indent();

        if (aMacro.LinkFile.empty())
        {
            ImGui::TextWrapped("Keep this macro in step with a .txt file, one chat line per row (like the Raw text "
                               "tab). Edit the file in Notepad and the macro follows; edit the macro here and the "
                               "file follows.");
            ImGui::TextDisabled("A name alone goes in <GW2>\\addons\\GW2_Chat_Messages\\links\\. A file that already "
                                "has lines replaces the macro's lines; otherwise it's created from them.");
            ImGui::SetNextItemWidth(-(UiFont::ButtonWidth("Link") + ImGui::GetStyle().ItemSpacing.x));
            const bool enter = InputString("##linkpath", "callouts.txt, or a full path", s_LinkPath, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button("Link") || enter)
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
            ImGui::TextWrapped("File: %s", file.u8string().c_str());
            const bool missing = problem != s_LinkProblems.end() && problem->second.rfind("The file isn't there", 0) == 0;
            if (problem != s_LinkProblems.end()) ImGui::TextColored(kWarnColor, "%s", problem->second.c_str());
            else ImGui::TextDisabled("In step. Changes on either side show up within a second.");
            UiFont::Row row;
            if (ImGui::SmallButton("Open file")) Storage::OpenFolder(file);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Opens it in your text editor (Notepad, unless you chose another).");
            row.Next(UiFont::ButtonWidth("Open folder"));
            if (ImGui::SmallButton("Open folder")) Storage::OpenFolder(file.parent_path());
            if (missing)
            {
                row.Next(UiFont::ButtonWidth("Recreate the file"));
                if (ImGui::SmallButton("Recreate the file"))
                {
                    if (WriteLinkFile(aMacro, file, s_LinkStates[aMacro.Id])) s_LinkProblems.erase(aMacro.Id);
                    else ShowError("Clicked Recreate the file for \"" + aMacro.Name + "\"", "The file couldn't be created. Check the Nexus log.");
                }
            }
            row.Next(UiFont::ButtonWidth("Unlink"));
            if (ImGui::SmallButton("Unlink"))
            {
                aMacro.LinkFile.clear();
                s_LinkStates.erase(aMacro.Id);
                s_LinkProblems.erase(aMacro.Id);
                MarkDirty();
                SetInfo("Unlinked. The file was left as it is.");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stops keeping the macro and the file in step. Neither is deleted.");
        }
        ImGui::Unindent();
    }

    void DrawMacroEditor(Node& aMacro)
    {
        const bool busy = Sender::IsBusy();
        if (busy) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        if (ImGui::Button("Send now") && !busy) SendMacro(aMacro);
        if (busy) ImGui::PopStyleVar();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Types each line into its chat channel and sends it.\n"
                              "Needs your Chat key set in Nexus > Options > Game Keybinds.");
        ImGui::SameLine();
        DrawSendStatus();
        DrawHotkeyRow(aMacro);
        DrawFilters(aMacro);
        DrawLinkFile(aMacro);
        ImGui::Separator();

        UiFont::Row modeRow;
        int mode = static_cast<int>(s_EditMode);
        if (ImGui::RadioButton("Lines", &mode, static_cast<int>(EEditMode::Lines))) s_RawForId = 0;
        modeRow.Next(UiFont::CheckboxWidth("Raw text"));
        if (ImGui::RadioButton("Raw text", &mode, static_cast<int>(EEditMode::Raw))) s_RawForId = 0;
        s_EditMode = static_cast<EEditMode>(mode);
        const std::string count = std::to_string(aMacro.Lines.size()) + " line(s)";
        modeRow.Next(UiFont::TextWidth(count.c_str()));
        ImGui::TextDisabled("%s", count.c_str());
        modeRow.Next(UiFont::TextWidth("Placeholders (?)"));
        ImGui::TextDisabled("Placeholders (?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            UiFont::PushTooltipWrap();
            ImGui::TextUnformatted("Type these in a message; they're filled in when it's sent:");
            ImGui::Separator();
            float syntaxWidth = 0.0f;
            for (const Placeholders::Info& p : Placeholders::List())
                syntaxWidth = std::max(syntaxWidth, UiFont::TextWidth(p.Syntax));
            for (const Placeholders::Info& p : Placeholders::List())
            {
                ImGui::TextUnformatted(p.Syntax);
                ImGui::SameLine(syntaxWidth + ImGui::GetStyle().ItemSpacing.x * 3.0f);
                ImGui::TextDisabled("%s", p.Description);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Text files go in <GW2>\\addons\\GW2_Chat_Messages\\ (or a folder inside it). "
                                "Use \"quotes\" for names with spaces: {txt \"my lines.txt\"}. "
                                "Anything else in { } is sent as typed. Chat links like [&BDAEAAA=] work as is.");
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
            ImGui::TextDisabled("Select a folder or macro.");
            ImGui::Spacing();
            ImGui::TextWrapped("Right-click an item for more options. Drag items to move them: drop on the middle "
                               "of a folder to put them inside it, or above or below an item to place them next to it.");
            return;
        }

        ImGui::TextDisabled("%s", node->IsFolder() ? "Folder" : "Macro");
        ImGui::TextWrapped("%s", node->Name.c_str());
        ImGui::TextDisabled("In: %s", PathOf(*node).c_str());
        // Popups must be opened at the main window's ID level, so these buttons queue an action.
        UiFont::Row nameRow;
        if (ImGui::SmallButton("Rename")) s_Pending = {EAction::OpenName, 0, node->Id, EDrop::Into, 0, ENameMode::Rename};
        nameRow.Next(UiFont::ButtonWidth("Delete..."));
        if (ImGui::SmallButton("Delete...")) s_Pending = {EAction::OpenDelete, node->Id};
        ImGui::Separator();

        if (node->IsFolder())
        {
            size_t folders = 0, macros = 0;
            Tree::CountDescendants(*node, &folders, &macros);
            const int level = Tree::FolderLevel(*node);
            ImGui::Text("Folder level %d of %d", level, kMaxFolderDepth);
            ImGui::Text("Contains %zu folder(s) and %zu macro(s)", folders, macros);
            ImGui::Spacing();
            UiFont::Row newRow;
            if (level < kMaxFolderDepth)
            {
                if (ImGui::Button("New folder inside"))
                    s_Pending = {EAction::OpenName, 0, node->Id, EDrop::Into, 0, ENameMode::NewFolder};
                newRow.Next(UiFont::ButtonWidth("New macro inside"));
            }
            if (ImGui::Button("New macro inside"))
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
        if (aMacro.Lines.empty()) ImGui::TextDisabled("No lines yet.");
        for (size_t i = 0; i < aMacro.Lines.size() && i < kPreviewLines; i++)
        {
            const Line& l = aMacro.Lines[i];
            ImGui::TextColored(ChannelColorVec(l.Channel), "%s", ChatLine::TypedText(l).c_str());
        }
        if (aMacro.Lines.size() > kPreviewLines)
            ImGui::TextDisabled("...and %zu more line(s)", aMacro.Lines.size() - kPreviewLines);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
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
                const EChannel first = n.Lines.empty() ? EChannel::Current : n.Lines.front().Channel;
                const std::string keys = Hotkey::ToString(n.Hotkey);
                ImGui::PushStyleColor(ImGuiCol_Text, ChannelColorVec(first));
                const bool clicked = ImGui::MenuItem(n.Name.c_str(), keys.empty() ? nullptr : keys.c_str(), false, !busy);
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) MacroPreviewTooltip(n);
                if (clicked) SendMacro(n, ESendFrom::Menu); // sending doesn't change the tree
            }
            ImGui::PopID();
        }
    }

    void DrawQuickMenu()
    {
        const Here here = WhereAmI();
        if (!HasActiveMacros(s_Tree.Root(), here))
            ImGui::TextDisabled(s_Tree.Root().Children.empty() ? "No macros yet." : "No macros for this map.");
        else
        {
            if (Sender::IsBusy()) ImGui::TextColored(kWarnColor, "Sending...");
            DrawQuickMenuItems(s_Tree.Root(), here);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(s_ShowMainWindow ? "Hide macro library" : "Open macro library"))
            s_ShowMainWindow = !s_ShowMainWindow;
    }

    void DrawQuickMenuPopup()
    {
        if (s_QuickMenuOpenRequest)
        {
            s_QuickMenuOpenRequest = false;
            ImGui::OpenPopup(kQuickMenuPopup); // opens at the mouse pointer
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
        DrawQuickMenu();
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

    std::optional<nlohmann::json> s_UndoImport; // the library before the last import
    std::string                   s_UndoText;

    void LoadImport(Exchange::Package aPkg)
    {
        s_ImportOpt.PerItem.clear(); // keyed by the old package's nodes
        if (!aPkg.Ok())
        {
            const std::string what = aPkg.Source.empty() ? std::string("it") : "\"" + aPkg.Source + "\"";
            ShowError("Opened " + what + " in the Import window", (aPkg.Source.empty() ? std::string("It") : what) + " can't be imported. " + aPkg.Error);
            s_ImportPkg.reset();
            return;
        }
        s_ImportPkg = std::make_unique<Exchange::Package>(std::move(aPkg));
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
            std::string ext = it->path().extension().u8string();
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (it->is_directory(ec2) || ext == ".json" || ext == ".txt") s_ImportList.push_back(it->path());
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
        return aFolder.Parent ? PathOf(aFolder) + " > " + aFolder.Name : "Top level";
    }

    bool ChoiceCombo(const char* aLabel, Exchange::EChoice& aChoice, bool aFolder)
    {
        using Exchange::EChoice;
        bool changed = false;
        if (ImGui::BeginCombo(aLabel, Exchange::ChoiceLabel(aChoice)))
        {
            const EChoice folderChoices[] = {EChoice::Merge, EChoice::Rename, EChoice::Skip, EChoice::Replace};
            const EChoice macroChoices[]  = {EChoice::Rename, EChoice::Skip, EChoice::Replace};
            const EChoice* list = aFolder ? folderChoices : macroChoices;
            const size_t count  = aFolder ? 4 : 3;
            for (size_t i = 0; i < count; i++)
                if (ImGui::Selectable(Exchange::ChoiceLabel(list[i]), list[i] == aChoice))
                {
                    aChoice = list[i];
                    changed = true;
                }
            ImGui::EndCombo();
        }
        return changed;
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
            case EOutcome::New:       tag = "new"; color = newColor; break;
            case EOutcome::Renamed:   tag = "new, as \"" + item.Name + "\""; color = newColor; break;
            case EOutcome::Merged:    tag = "goes into the existing folder"; break;
            case EOutcome::Replaced:  tag = "replaces the existing one"; color = kWarnColor; break;
            case EOutcome::Skipped:   tag = "skipped"; color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled); break;
            case EOutcome::TooDeep:   tag = "left out: folders would be nested more than 6 deep"; color = kErrorColor; break;
            case EOutcome::Flattened: tag = "folder left out (too deep); its contents move up a level"; color = kWarnColor; break;
            }
            const bool showChildren = src.IsFolder() && !item.Children.empty();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (!showChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | (src.IsMacro() ? ImGuiTreeNodeFlags_Bullet : 0);
            ImGui::PushStyleColor(ImGuiCol_Text, src.IsFolder() ? kFolderColor : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            const bool open = ImGui::TreeNodeEx("##item", flags, "%s", src.Name.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextColored(color, "(%s)", tag.c_str());
            if (item.Conflict)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(Px(170.0f));
                Exchange::EChoice choice = item.Choice;
                if (ChoiceCombo("##choice", choice, src.IsFolder())) s_ImportOpt.PerItem[&src] = choice;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("There's already a %s called \"%s\" there.", src.IsFolder() ? "folder" : "macro", src.Name.c_str());
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
        ImGui::TextUnformatted("1. Choose what to import");
        ImGui::TextDisabled("A .json export, a .txt file (one macro), or a folder of .txt files.");
        if (s_ImportListStale) RefreshImportList();

        ImGui::BeginChild("##imports", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 5.0f), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushTextWrapPos(0.0f);
        if (s_ImportList.empty())
            ImGui::TextDisabled("Put files in <GW2>\\addons\\GW2_Chat_Messages\\imports\\ to see them here.");
        for (const std::filesystem::path& p : s_ImportList)
        {
            std::error_code ec;
            const bool dir = std::filesystem::is_directory(p, ec);
            const std::string label = p.filename().u8string() + (dir ? "\\" : "");
            if (ImGui::Selectable(label.c_str())) LoadImport(Exchange::FromPath(p));
        }
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        UiFont::Row row;
        if (ImGui::SmallButton("Open imports folder")) Storage::OpenFolder(Storage::ImportsDir());
        row.Next(UiFont::ButtonWidth("Refresh"));
        if (ImGui::SmallButton("Refresh")) s_ImportListStale = true;
        row.Next(UiFont::ButtonWidth("Paste from clipboard"));
        if (ImGui::SmallButton("Paste from clipboard"))
        {
            const char* text = ImGui::GetClipboardText();
            if (!text || !*text) ShowError("Clicked Paste from clipboard in the Import window", "The clipboard has no text.");
            else
            {
                Exchange::Package pkg = Exchange::FromText("Pasted macro", text);
                pkg.Source = "Pasted text";
                LoadImport(std::move(pkg));
            }
        }

        ImGui::SetNextItemWidth(-(UiFont::ButtonWidth("Load") + ImGui::GetStyle().ItemSpacing.x));
        const bool enter = InputString("##path", "Or a full path to a file or folder", s_ImportPath, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Load") || enter) && !s_ImportPath.empty())
        {
            std::string path = s_ImportPath;
            if (path.size() >= 2 && path.front() == '"' && path.back() == '"') path = path.substr(1, path.size() - 2); // "Copy as path"
            LoadImport(Exchange::FromPath(std::filesystem::u8path(path)));
        }
    }

    void CloseImport()
    {
        s_ShowImport = false;
        s_ImportPkg.reset();
        s_ImportOpt.PerItem.clear();
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
        const bool shown = ImGui::Begin("Import macros##chatmsg", &s_ShowImport,
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
        ImGui::Text("2. Choose where it goes (from %s)", s_ImportPkg->Source.c_str());
        Node* target = NodeOrRoot(s_ImportTarget);
        if (!target || !target->IsFolder()) { s_ImportTarget = 0; target = &s_Tree.Root(); }
        ImGui::SetNextItemWidth(-(UiFont::TextWidth("Import into") + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f));
        if (ImGui::BeginCombo("Import into", FolderPath(*target).c_str()))
        {
            if (ImGui::Selectable("Top level", target == &s_Tree.Root())) s_ImportTarget = 0;
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
        ChoiceCombo("When a macro's name is taken", s_ImportOpt.MacroConflict, false);
        ImGui::SetNextItemWidth(Px(170.0f));
        ChoiceCombo("When a folder's name is taken", s_ImportOpt.FolderConflict, true);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Merge: the folder's contents go into the existing folder.\nReplace: the existing folder's contents are deleted first.");
        ImGui::Checkbox("Move the contents of too-deep folders up a level", &s_ImportOpt.Flatten);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Folders can be nested at most %d deep. Unticked, folders that would go deeper are left out.", kMaxFolderDepth);
        ImGui::Checkbox("Keep hotkeys", &s_ImportOpt.KeepHotkeys);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Untick this for macros from someone else, so their keys don't clash with yours.");
        for (const std::string& w : s_ImportPkg->Warnings) ImGui::TextColored(kWarnColor, "%s", w.c_str());

        // Built every frame against the live library, so the preview is always what Import does.
        const Exchange::Plan plan = Exchange::BuildPlan(s_Tree, *target, s_ImportPkg->Items, s_ImportOpt);

        ImGui::Separator();
        ImGui::TextUnformatted("3. Check the preview");
        std::string summary = std::to_string(plan.NewMacros) + " new macro(s), " + std::to_string(plan.NewFolders) + " new folder(s)";
        if (plan.Merged)   summary += ", " + std::to_string(plan.Merged) + " merged";
        if (plan.Replaced) summary += ", " + std::to_string(plan.Replaced) + " replaced";
        if (plan.Skipped)  summary += ", " + std::to_string(plan.Skipped) + " skipped";
        if (plan.TooDeep)  summary += ", " + std::to_string(plan.TooDeep) + " left out (too deep)";
        ImGui::TextWrapped("%s.", summary.c_str());
        if (plan.Conflicts) ImGui::TextDisabled("%zu name(s) are already taken: choose for each one below.", plan.Conflicts);

        const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        ImGui::BeginChild("##preview", ImVec2(0.0f, -footer), true, ImGuiWindowFlags_HorizontalScrollbar);
        DrawPlanItems(plan.Items); // one row per item; long rows scroll sideways
        ImGui::EndChild();

        const bool canImport = plan.Changes() > 0;
        if (!canImport) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        if (ImGui::Button("Import") && canImport)
        {
            s_UndoImport = s_Tree.ToJson();
            const size_t n = Exchange::Apply(s_Tree, *target, plan, s_ImportOpt);
            s_UndoText = "Imported " + std::to_string(n) + " item(s) from " + s_ImportPkg->Source + " into " + FolderPath(*target) + ".";
            s_RawForId = 0; // a replaced macro's raw text is stale
            CancelCapture();
            MarkDirty();
            s_ShowMainWindow = true;
            CloseImport();
        }
        if (!canImport) ImGui::PopStyleVar();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) CloseImport();

        ImGui::PopTextWrapPos();
        ImGui::End();
    }

    // Shown in the library window after an import, until dismissed.
    void DrawUndoImport()
    {
        if (!s_UndoImport) return;
        ImGui::TextWrapped("%s", s_UndoText.c_str());
        UiFont::Row row;
        if (ImGui::SmallButton("Undo import"))
        {
            std::vector<std::string> ignored;
            s_Tree.FromJson(*s_UndoImport, &ignored);
            s_UndoImport.reset();
            s_SelectedId = 0;
            s_RawForId   = 0;
            CancelCapture();
            MarkDirty();
            SetInfo("The import was undone.");
            return;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Puts your library back as it was before the import.\nChanges you made since then are undone too.");
        row.Next(UiFont::ButtonWidth("Dismiss##undo"));
        if (ImGui::SmallButton("Dismiss##undo")) s_UndoImport.reset();
        ImGui::Separator();
    }

    void DrawToolbar()
    {
        Node& into = InsertFolder();
        const bool canNest = into.Parent == nullptr || Tree::FolderLevel(into) < kMaxFolderDepth;
        UiFont::Row row; // buttons that don't fit continue on the next line
        if (ImGui::Button("New folder"))
        {
            if (canNest) OpenNamePopup(ENameMode::NewFolder, into.Id);
            else ShowError("Clicked New folder with \"" + into.Name + "\" selected",
                           "Folders can be nested at most " + std::to_string(kMaxFolderDepth) + " levels deep.");
        }
        row.Next(UiFont::ButtonWidth("New macro"));
        if (ImGui::Button("New macro")) OpenNamePopup(ENameMode::NewMacro, into.Id);
        row.Next(UiFont::ButtonWidth("Expand all"));
        if (ImGui::Button("Expand all"))   s_Pending = {EAction::ExpandAll, 0, 0, EDrop::Into, 0, ENameMode::NewFolder, true};
        row.Next(UiFont::ButtonWidth("Collapse all"));
        if (ImGui::Button("Collapse all")) s_Pending = {EAction::ExpandAll, 0, 0, EDrop::Into, 0, ENameMode::NewFolder, false};
        row.Next(UiFont::ButtonWidth("Sort A-Z"));
        if (ImGui::Button("Sort A-Z"))
        {
            s_Tree.SortChildren(s_Tree.Root(), true);
            MarkDirty();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sorts every folder: folders first, then macros, each A-Z.");
        row.Next(UiFont::ButtonWidth("Import..."));
        if (ImGui::Button("Import..."))
        {
            s_ImportTarget = InsertFolder().Parent ? InsertFolder().Id : 0;
            s_ShowImport   = true;
        }
        row.Next(UiFont::ButtonWidth("Export all"));
        if (ImGui::Button("Export all"))
        {
            if (s_Tree.Root().Children.empty()) ShowError("Clicked Export all", "There's nothing to export yet.");
            else ExportToFile(TopLevelNodes(), "Chat Messages library", false);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Saves your whole library to the exports folder.\nRight-click a folder or macro to export just that.");
        row.Next(UiFont::ComboWidth("Text size"));
        UiFont::DrawCombo("Text size");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The size of the text in this addon's windows.\nThe sizes are the fonts Nexus provides.");
        ImGui::TextDisabled("New items go into: %s", into.Parent ? into.Name.c_str() : "Top level");
    }

    void DrawMessages()
    {
        if (s_SaveFailed) // stays until a save works (the error window said why)
            ImGui::TextColored(kErrorColor, "Your macros couldn't be saved. Check the Nexus log. Retrying...");
        if (!s_Status.empty() && ImGui::GetTime() < s_StatusUntil) ImGui::TextWrapped("%s", s_Status.c_str());
        DrawUndoImport();
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

        DrawMessages();
        DrawToolbar();
        ImGui::Separator();

        // Both panes scroll sideways too, for long names and rows that can't wrap.
        const float treeWidth = ImGui::GetContentRegionAvail().x * 0.38f;
        ImGui::BeginChild("##tree", ImVec2(treeWidth, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushTextWrapPos(0.0f);
        if (s_Tree.Root().Children.empty())
        {
            ImGui::TextWrapped("Your library is empty. Create a folder or a macro to get started.");
        }
        DrawTree();
        ImGui::PopTextWrapPos();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##details", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushTextWrapPos(0.0f);
        DrawDetails();
        ImGui::PopTextWrapPos();
        ImGui::EndChild();

        ApplyPending();
        DrawNamePopup();
        DrawDeletePopup();

        ImGui::PopTextWrapPos();
        ImGui::End();
    }
}

void Ui::Init()
{
    UiFont::Init();
    s_Mumble = static_cast<Mumble::Data*>(g_Api->DataLink_Get(DL_MUMBLE_LINK));
    s_LoadMessages.clear();
    Storage::LoadMacros(s_Tree, s_LoadMessages);
    for (const std::string& m : s_LoadMessages)
    {
        LogF(LOGL_WARNING, "%s", m.c_str());
        ShowError("Started the game (the addon loaded macros.json)", m);
    }
    s_LoadMessages.clear();
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
}

void Ui::Render()
{
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
    ImGui::Text("Chat Messages %s", kVersionText);
    ImGui::TextDisabled("Your macros are saved in <GW2>\\addons\\GW2_Chat_Messages\\macros.json.");
    if (ImGui::Button("Open macro library")) s_ShowMainWindow = true;
    ImGui::Spacing();
    UiFont::DrawCombo("Text size");
    ImGui::TextDisabled("The size of the text in this addon's windows (not this page).");
    ImGui::Spacing();
    ImGui::TextWrapped("Quick menu: right-click the scroll icon in the Nexus bar, or set a key for "
                       "\"Open Chat Messages quick menu\" in Nexus > Options > Keybinds.");
}

void Ui::ToggleMainWindow() { s_ShowMainWindow = !s_ShowMainWindow; }

void Ui::RenderQuickAccessMenu()
{
    UiFont::Push();
    DrawQuickMenu();
    UiFont::Pop();
}

void Ui::ToggleQuickMenu()
{
    // ImGui::IsPopupOpen only sees popups at the current ID level, which is ours here (root).
    if (ImGui::IsPopupOpen(kQuickMenuPopup)) s_QuickMenuCloseRequest = true;
    else s_QuickMenuOpenRequest = true;
}
bool* Ui::MainWindowOpenFlag() { return &s_ShowMainWindow; }
