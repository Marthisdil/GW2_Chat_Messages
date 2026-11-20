#pragma once

// The macro library: a tree of folders and macros (docs/design.md, "Data model").
// No Nexus or ImGui dependency, so it can be unit-tested on its own (tests/MacroTreeTests.cpp).
// Not thread-safe: only the render thread touches the tree.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Hotkey.h"

namespace MacroTree
{
    // Folders may nest this deep (root -> level 1 -> ... -> level 6). Macros can live in any folder.
    constexpr int kMaxFolderDepth = 6;
    constexpr size_t kMaxNameLength = 100; // bytes of UTF-8, same as bhm-chat-macros' title limit
    constexpr int kFormatVersion = 1;

    enum class ENodeType : uint8_t { Folder, Macro };

    enum class EChannel : uint8_t
    {
        Current, Emote, Say, Map, Party, Squad, Team, Reply, Whisper,
        Guild, Guild1, Guild2, Guild3, Guild4, Guild5, Guild6,
        Count
    };

    // Bit flags; a macro with no mode set behaves like all modes.
    enum EGameMode : uint8_t { GM_None = 0, GM_PvE = 1, GM_WvW = 2, GM_PvP = 4, GM_All = 7 };

    struct Line
    {
        EChannel    Channel        = EChannel::Current;
        std::string Message;
        std::string WhisperTo;      // Whisper only
        bool        SquadBroadcast = false; // Squad only
    };

    struct Node
    {
        uint64_t    Id   = 0;
        ENodeType   Type = ENodeType::Folder;
        std::string Name;          // folder name or macro title
        Node*       Parent = nullptr;

        // Folder
        std::vector<std::unique_ptr<Node>> Children;
        bool Expanded = true;

        // Macro
        std::vector<Line> Lines;
        uint8_t           GameModes = GM_All;
        std::vector<int>  MapIds;
        std::string       LinkFile;
        Hotkey::Combo     Hotkey;   // empty = none

        bool IsFolder() const { return Type == ENodeType::Folder; }
        bool IsMacro() const  { return Type == ENodeType::Macro; }
    };

    // Result of an edit. On failure, Error says why in words the user can read.
    struct Result
    {
        bool        Ok = true;
        std::string Error;
        MacroTree::Node* Target = nullptr; // the created or moved node, when there is one

        static Result Fail(std::string aError) { Result r; r.Ok = false; r.Error = std::move(aError); return r; }
    };

    // Where a moved node goes relative to the drop target.
    enum class EDrop : uint8_t { Before, Into, After };

    const char* ChannelKey(EChannel aChannel);   // stable JSON key, e.g. "say", "guild3"
    const char* ChannelLabel(EChannel aChannel); // display name, e.g. "Say", "Guild 3"
    EChannel    ChannelFromKey(const std::string& aKey); // unknown keys -> Current

    class Tree
    {
    public:
        Tree();

        Node&       Root()       { return *m_Root; }
        const Node& Root() const { return *m_Root; }

        Node* Find(uint64_t aId) const;

        // Folder level of a node: root = 0, its folders = 1. A macro reports its parent's level.
        static int  FolderLevel(const Node& aNode);
        // How many folder levels a subtree adds: a macro 0, an empty folder 1, a folder with a subfolder 2...
        static int  FolderHeight(const Node& aNode);
        static bool IsAncestorOf(const Node& aAncestor, const Node& aNode);
        static size_t CountDescendants(const Node& aNode, size_t* aFolders = nullptr, size_t* aMacros = nullptr);

        // aParent must be a folder (nullptr = root).
        Result AddFolder(Node* aParent, const std::string& aName);
        Result AddMacro(Node* aParent, const std::string& aTitle);
        Result Rename(Node& aNode, const std::string& aName);
        Result Delete(Node& aNode);
        // Move aNode before/after aTarget (in aTarget's folder) or into aTarget (a folder, appended).
        Result Move(Node& aNode, Node& aTarget, EDrop aDrop);
        // Checks a move without doing it (used to show the drop indicator while dragging).
        Result CanMove(const Node& aNode, const Node& aTarget, EDrop aDrop) const;
        // Move one place up or down among its siblings. Returns false at either end.
        bool   Shift(Node& aNode, int aDelta);
        // Sorts a folder's children: folders first, then macros, each A-Z (case-insensitive).
        void   SortChildren(Node& aFolder, bool aRecursive);
        // Every macro using this hotkey, in tree order (more than one = a conflict). Empty combo -> none.
        std::vector<Node*> MacrosWithHotkey(const Hotkey::Combo& aCombo) const;

        nlohmann::json ToJson() const;
        // Replaces the tree. Bad nodes are skipped, missing or duplicate ids are regenerated.
        // aWarnings collects what was fixed. Returns false (tree unchanged) if aJson isn't a tree at all.
        bool FromJson(const nlohmann::json& aJson, std::vector<std::string>* aWarnings = nullptr);

        static std::string CleanName(const std::string& aName); // trims spaces, cuts to kMaxNameLength

    private:
        std::unique_ptr<Node> m_Root;

        uint64_t NewId() const;
        Result   CheckName(const Node& aParent, ENodeType aType, const std::string& aName, const Node* aIgnore) const;
        Result   Add(Node* aParent, ENodeType aType, const std::string& aName);
    };
}
