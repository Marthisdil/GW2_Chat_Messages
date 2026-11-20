#include "Ui.h"

#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

#include "ChatLine.h"
#include "Globals.h"
#include "Hotkeys.h"
#include "MacroTree.h"
#include "Mumble.h"
#include "Placeholders.h"
#include "Sender.h"
#include "Storage.h"

using namespace MacroTree;

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
    std::string s_Status;
    double      s_StatusUntil = 0.0;

    uint64_t s_SelectedId = 0;

    // Name popup (new folder, new macro, rename).
    enum class ENameMode { NewFolder, NewMacro, Rename };
    ENameMode   s_NameMode     = ENameMode::NewFolder;
    uint64_t    s_NameTargetId = 0; // parent folder for New*, the node for Rename (0 = root)
    char        s_NameBuf[256] = {};
    std::string s_NameError;
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

    void SaveNow()
    {
        if (!s_Dirty) return;
        s_SaveFailed = !Storage::SaveMacros(s_Tree);
        if (!s_SaveFailed) s_Dirty = false;
        else s_SaveAt = ImGui::GetTime() + 5.0; // retry later, without hammering the disk
    }

    void SetStatus(const std::string& aText)
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

    // --- Sending ------------------------------------------------------------------------------
    // The sender runs on its own thread; the UI only starts it and shows its status.

    Sender::EState s_SendSeen = Sender::EState::Idle; // last state shown, to notice when a send ends
    std::string    s_SendNote;                        // "Sent 2 lines." / why it failed
    bool           s_SendNoteError = false;
    double         s_SendNoteUntil = 0.0;

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
        std::string error;
        if (Sender::Start(aMacro.Id, aMacro.Lines, error))
        {
            s_SendAlertOnFail = alert;
            return;
        }
        s_SendNote      = error;
        s_SendNoteError = true;
        s_SendNoteUntil = ImGui::GetTime() + kStatusShowSec;
        s_SendSeen      = Sender::GetStatus().State; // already reported here
        if (alert) Alert(aMacro.Name + ": " + error);
    }

    void PollSender()
    {
        const Sender::Status st = Sender::GetStatus();
        if (st.State == s_SendSeen) return;
        s_SendSeen = st.State;
        if (st.State == Sender::EState::Done || st.State == Sender::EState::Failed)
        {
            s_SendNote      = st.Message;
            s_SendNoteError = st.State == Sender::EState::Failed;
            s_SendNoteUntil = ImGui::GetTime() + kStatusShowSec;
            if (s_SendNoteError && s_SendAlertOnFail)
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
                if (!why.empty()) SetStatus(why);
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
                Alert(Hotkey::ToString(combo) + " is set on " + std::to_string(macros.size()) +
                      " macros, so nothing was sent. Give each macro its own hotkey.");
                continue;
            }
            SendMacro(*macros[0], ESendFrom::Hotkey);
        }
    }

    void DrawHotkeyRow(Node& aMacro)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Hotkey:");
        ImGui::SameLine();
        if (s_CaptureFor == aMacro.Id && Hotkeys::IsCapturing())
        {
            ImGui::TextColored(kWarnColor, "Press a key or mouse button (Escape cancels)...");
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel")) CancelCapture();
            return;
        }

        if (aMacro.Hotkey.Empty()) ImGui::TextDisabled("None");
        else ImGui::TextUnformatted(Hotkey::ToString(aMacro.Hotkey).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(aMacro.Hotkey.Empty() ? "Set..." : "Change..."))
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
            ImGui::SameLine();
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
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Game modes:");
        const uint8_t bits[] = {GM_PvE, GM_WvW, GM_PvP};
        for (uint8_t b : bits)
        {
            ImGui::SameLine();
            bool on = (aMacro.GameModes & b) != 0;
            if (ImGui::Checkbox(ModeName(b), &on))
            {
                const uint8_t next = on ? static_cast<uint8_t>(aMacro.GameModes | b)
                                        : static_cast<uint8_t>(aMacro.GameModes & ~b);
                if (next == GM_None) SetStatus("A macro needs at least one game mode.");
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

        // Maps.
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Maps:");
        ImGui::SameLine();
        if (aMacro.MapIds.empty()) ImGui::TextDisabled("Any map");
        int removeAt = -1;
        for (size_t i = 0; i < aMacro.MapIds.size(); i++)
        {
            ImGui::SameLine();
            ImGui::PushID(static_cast<int>(i));
            const std::string label = std::to_string(aMacro.MapIds[i]) + " x";
            if (ImGui::SmallButton(label.c_str())) removeAt = static_cast<int>(i);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove map %d", aMacro.MapIds[i]);
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
            if (aId <= 0) SetStatus("Map ids are positive numbers.");
            else if (hasMap(aId)) SetStatus("That map is already in the list.");
            else if (aMacro.MapIds.size() >= kMaxMapIds)
                SetStatus("A macro can list at most " + std::to_string(kMaxMapIds) + " maps.");
            else
            {
                aMacro.MapIds.push_back(aId);
                MarkDirty();
            }
        };

        const int current = static_cast<int>(here.MapId);
        const bool canAddCurrent = current > 0 && !hasMap(current);
        if (!canAddCurrent) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        const std::string addCurrent = current > 0 ? "Add this map (" + std::to_string(current) + ")" : "Add this map";
        if (ImGui::SmallButton(addCurrent.c_str()) && canAddCurrent) addMap(current);
        if (!canAddCurrent) ImGui::PopStyleVar();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputInt("##mapid", &s_MapIdInput, 0, 0);
        ImGui::SameLine();
        if (ImGui::SmallButton("Add id")) addMap(s_MapIdInput);
        ImGui::TextDisabled("Map names come later. Ids are on the GW2 wiki and in the API (/v2/maps).");

        if (active) ImGui::TextDisabled("Active here (%s, map %u).", ModeName(here.Mode), here.MapId);
        else ImGui::TextColored(kWarnColor, "Not active here (%s, map %u).", ModeName(here.Mode), here.MapId);
        ImGui::Unindent();
    }

    void DrawSendStatus()
    {
        const Sender::Status st = Sender::GetStatus();
        if (st.State == Sender::EState::Sending)
            ImGui::TextColored(kWarnColor, "Sending line %zu of %zu...", st.Line, st.Lines);
        else if (!s_SendNote.empty() && ImGui::GetTime() < s_SendNoteUntil)
        {
            if (s_SendNoteError) ImGui::TextColored(kErrorColor, "%s", s_SendNote.c_str());
            else ImGui::TextDisabled("%s", s_SendNote.c_str());
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

    void OpenNamePopup(ENameMode aMode, uint64_t aTargetId)
    {
        s_NameMode     = aMode;
        s_NameTargetId = aTargetId;
        s_NameError.clear();
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
            ImGui::Text("%s %s", aNode.IsFolder() ? "Folder:" : "Macro:", aNode.Name.c_str());
            if (!s_DragError.empty()) ImGui::TextColored(kErrorColor, "%s", s_DragError.c_str());
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
        if (avail.y < 24.0f) avail.y = 24.0f;
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
            else SetStatus(r.Error);
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
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(kNamePopup, nullptr, ImGuiWindowFlags_NoResize)) return;

        const char* title = s_NameMode == ENameMode::NewFolder ? "New folder name:"
                          : s_NameMode == ENameMode::NewMacro  ? "New macro title:"
                                                               : "New name:";
        ImGui::TextUnformatted(title);
        if (s_NameFocus) { ImGui::SetKeyboardFocusHere(); s_NameFocus = false; }
        ImGui::SetNextItemWidth(-1.0f);
        const bool enter = ImGui::InputText("##name", s_NameBuf, kMaxNameLength + 1,
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (!s_NameError.empty()) ImGui::TextColored(kErrorColor, "%s", s_NameError.c_str());

        const bool ok = ImGui::Button(s_NameMode == ENameMode::Rename ? "Rename" : "Create", ImVec2(100.0f, 0.0f)) || enter;
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) ImGui::CloseCurrentPopup();

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
                s_NameError = r.Error;
                s_NameFocus = true;
            }
        }
        ImGui::EndPopup();
    }

    void DrawDeletePopup()
    {
        ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(kDeletePopup, nullptr, ImGuiWindowFlags_NoResize)) return;

        Node* node = s_Tree.Find(s_DeleteId);
        if (!node)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
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

        if (ImGui::Button("Delete", ImVec2(100.0f, 0.0f)))
        {
            if (s_SelectedId == node->Id || (s_Tree.Find(s_SelectedId) && Tree::IsAncestorOf(*node, *s_Tree.Find(s_SelectedId))))
                s_SelectedId = 0;
            s_Tree.Delete(*node);
            MarkDirty();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // --- Macro editor -------------------------------------------------------------------------

    constexpr size_t kMaxLinesPerMacro = 50;

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

            if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0) { moveFrom = i; moveTo = i - 1; }
            ImGui::SameLine(0.0f, 2.0f);
            if (ImGui::ArrowButton("##down", ImGuiDir_Down) && i + 1 < static_cast<int>(lines.size())) { moveFrom = i; moveTo = i + 1; }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            if (ChannelCombo("##channel", l.Channel)) MarkDirty();
            if (l.Channel == EChannel::Whisper)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150.0f);
                if (InputString("##to", "Recipient", l.WhisperTo)) MarkDirty();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Character or account name to whisper.");
            }
            else if (l.Channel == EChannel::Squad)
            {
                ImGui::SameLine();
                if (ImGui::Checkbox("Broadcast", &l.SquadBroadcast)) MarkDirty();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Send as a squad broadcast. Only works while you're the commander;\n"
                                      "otherwise it's sent as a normal squad message.");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) removeAt = i;

            ImGui::SetNextItemWidth(-70.0f);
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
        ImGui::Separator();

        int mode = static_cast<int>(s_EditMode);
        if (ImGui::RadioButton("Lines", &mode, static_cast<int>(EEditMode::Lines))) s_RawForId = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Raw text", &mode, static_cast<int>(EEditMode::Raw))) s_RawForId = 0;
        s_EditMode = static_cast<EEditMode>(mode);
        ImGui::SameLine();
        ImGui::TextDisabled("%zu line(s)", aMacro.Lines.size());
        ImGui::SameLine();
        ImGui::TextDisabled("Placeholders (?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted("Type these in a message; they're filled in when it's sent:");
            ImGui::Separator();
            for (const Placeholders::Info& p : Placeholders::List())
            {
                ImGui::TextUnformatted(p.Syntax);
                ImGui::SameLine(150.0f);
                ImGui::TextDisabled("%s", p.Description);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Text files go in <GW2>\\addons\\Chat Messages\\ (or a folder inside it).\n"
                                "Use \"quotes\" for names with spaces: {txt \"my lines.txt\"}.\n"
                                "Anything else in { } is sent as typed. Chat links like [&BDAEAAA=] work as is.");
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
        if (ImGui::SmallButton("Rename")) s_Pending = {EAction::OpenName, 0, node->Id, EDrop::Into, 0, ENameMode::Rename};
        ImGui::SameLine();
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
            if (level < kMaxFolderDepth)
            {
                if (ImGui::Button("New folder inside"))
                    s_Pending = {EAction::OpenName, 0, node->Id, EDrop::Into, 0, ENameMode::NewFolder};
                ImGui::SameLine();
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
        if (aMacro.Lines.empty()) ImGui::TextDisabled("No lines yet.");
        for (size_t i = 0; i < aMacro.Lines.size() && i < kPreviewLines; i++)
        {
            const Line& l = aMacro.Lines[i];
            ImGui::TextColored(ChannelColorVec(l.Channel), "%s", ChatLine::TypedText(l).c_str());
        }
        if (aMacro.Lines.size() > kPreviewLines)
            ImGui::TextDisabled("...and %zu more line(s)", aMacro.Lines.size() - kPreviewLines);
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

    void DrawToolbar()
    {
        Node& into = InsertFolder();
        const bool canNest = into.Parent == nullptr || Tree::FolderLevel(into) < kMaxFolderDepth;
        if (ImGui::Button("New folder"))
        {
            if (canNest) OpenNamePopup(ENameMode::NewFolder, into.Id);
            else SetStatus("Folders can be nested at most " + std::to_string(kMaxFolderDepth) + " levels deep.");
        }
        ImGui::SameLine();
        if (ImGui::Button("New macro")) OpenNamePopup(ENameMode::NewMacro, into.Id);
        ImGui::SameLine();
        if (ImGui::Button("Expand all"))   s_Pending = {EAction::ExpandAll, 0, 0, EDrop::Into, 0, ENameMode::NewFolder, true};
        ImGui::SameLine();
        if (ImGui::Button("Collapse all")) s_Pending = {EAction::ExpandAll, 0, 0, EDrop::Into, 0, ENameMode::NewFolder, false};
        ImGui::SameLine();
        if (ImGui::Button("Sort A-Z"))
        {
            s_Tree.SortChildren(s_Tree.Root(), true);
            MarkDirty();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sorts every folder: folders first, then macros, each A-Z.");
        ImGui::TextDisabled("New items go into: %s", into.Parent ? into.Name.c_str() : "Top level");
    }

    void DrawMessages()
    {
        if (!s_LoadMessages.empty())
        {
            for (const std::string& m : s_LoadMessages) ImGui::TextColored(kWarnColor, "%s", m.c_str());
            if (ImGui::SmallButton("Dismiss")) s_LoadMessages.clear();
            ImGui::Separator();
        }
        if (s_SaveFailed)
            ImGui::TextColored(kErrorColor, "Your macros couldn't be saved. Check the Nexus log. Retrying...");
        if (!s_Status.empty() && ImGui::GetTime() < s_StatusUntil)
            ImGui::TextColored(kErrorColor, "%s", s_Status.c_str());
    }

    void DrawMainWindow()
    {
        ImGui::SetNextWindowSize(ImVec2(820.0f, 500.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(560.0f, 300.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (!ImGui::Begin(Ui::kMainWindowName, &s_ShowMainWindow, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        DrawMessages();
        DrawToolbar();
        ImGui::Separator();

        const float treeWidth = ImGui::GetContentRegionAvail().x * 0.38f;
        ImGui::BeginChild("##tree", ImVec2(treeWidth, 0.0f), true);
        if (s_Tree.Root().Children.empty())
        {
            ImGui::TextWrapped("Your library is empty. Create a folder or a macro to get started.");
        }
        DrawTree();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##details", ImVec2(0.0f, 0.0f), true);
        DrawDetails();
        ImGui::EndChild();

        ApplyPending();
        DrawNamePopup();
        DrawDeletePopup();

        ImGui::End();
    }
}

void Ui::Init()
{
    s_Mumble = static_cast<Mumble::Data*>(g_Api->DataLink_Get(DL_MUMBLE_LINK));
    s_LoadMessages.clear();
    Storage::LoadMacros(s_Tree, s_LoadMessages);
    for (const std::string& m : s_LoadMessages) LogF(LOGL_WARNING, "%s", m.c_str());
    s_Dirty = false;
}

void Ui::Shutdown()
{
    if (s_Dirty && !Storage::SaveMacros(s_Tree))
        LogF(LOGL_WARNING, "Unsaved macro changes could not be written on unload.");
    s_Dirty = false;
    s_Tree  = Tree();
    s_SelectedId = 0;
}

void Ui::Render()
{
    PollSender();
    HandleHotkeys();
    if (s_ShowMainWindow) DrawMainWindow();
    DrawQuickMenuPopup();
    if (s_Dirty && ImGui::GetTime() >= s_SaveAt) SaveNow();
}

void Ui::RenderOptions()
{
    ImGui::Text("Chat Messages %s", kVersionText);
    ImGui::TextDisabled("Your macros are saved in <GW2>\\addons\\Chat Messages\\macros.json.");
    if (ImGui::Button("Open macro library")) s_ShowMainWindow = true;
    ImGui::Spacing();
    ImGui::TextWrapped("Quick menu: right-click the scroll icon in the Nexus bar, or set a key for "
                       "\"Open Chat Messages quick menu\" in Nexus > Options > Keybinds.");
}

void Ui::ToggleMainWindow() { s_ShowMainWindow = !s_ShowMainWindow; }

void Ui::RenderQuickAccessMenu() { DrawQuickMenu(); }

void Ui::ToggleQuickMenu()
{
    // ImGui::IsPopupOpen only sees popups at the current ID level, which is ours here (root).
    if (ImGui::IsPopupOpen(kQuickMenuPopup)) s_QuickMenuCloseRequest = true;
    else s_QuickMenuOpenRequest = true;
}
bool* Ui::MainWindowOpenFlag() { return &s_ShowMainWindow; }
