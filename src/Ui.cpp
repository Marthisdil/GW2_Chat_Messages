#include "Ui.h"

#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

#include "Globals.h"
#include "MacroTree.h"
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
            ImGui::TreeNodeEx("##node", flags, "%s", aNode.Name.c_str());
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
        ImGui::Separator();

        if (node->IsFolder())
        {
            size_t folders = 0, macros = 0;
            Tree::CountDescendants(*node, &folders, &macros);
            const int level = Tree::FolderLevel(*node);
            ImGui::Text("Folder level %d of %d", level, kMaxFolderDepth);
            ImGui::Text("Contains %zu folder(s) and %zu macro(s)", folders, macros);
            ImGui::Spacing();
            // Popups must be opened at the main window's ID level, so these queue an action.
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
            ImGui::Text("%zu line(s)", node->Lines.size());
            for (const Line& l : node->Lines)
                ImGui::TextWrapped("[%s] %s", ChannelLabel(l.Channel), l.Message.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("Editing the lines comes in the next update.");
        }

        ImGui::Spacing();
        if (ImGui::Button("Rename")) s_Pending = {EAction::OpenName, 0, node->Id, EDrop::Into, 0, ENameMode::Rename};
        ImGui::SameLine();
        if (ImGui::Button("Delete...")) s_Pending = {EAction::OpenDelete, node->Id};
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
        ImGui::SetNextWindowSize(ImVec2(640.0f, 420.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 260.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (!ImGui::Begin(Ui::kMainWindowName, &s_ShowMainWindow, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        DrawMessages();
        DrawToolbar();
        ImGui::Separator();

        const float treeWidth = ImGui::GetContentRegionAvail().x * 0.5f;
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
    if (s_ShowMainWindow) DrawMainWindow();
    if (s_Dirty && ImGui::GetTime() >= s_SaveAt) SaveNow();
}

void Ui::RenderOptions()
{
    ImGui::Text("Chat Messages %s", kVersionText);
    ImGui::TextDisabled("Your macros are saved in <GW2>\\addons\\Chat Messages\\macros.json.");
    if (ImGui::Button("Open macro library")) s_ShowMainWindow = true;
}

void Ui::ToggleMainWindow() { s_ShowMainWindow = !s_ShowMainWindow; }
bool* Ui::MainWindowOpenFlag() { return &s_ShowMainWindow; }
