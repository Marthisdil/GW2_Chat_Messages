#pragma once

// The macro library: a tree of folders and macros (docs/design.md, "Data model").
// No Nexus or ImGui dependency, so it can be unit-tested on its own (tests/MacroTreeTests.cpp).
// Not thread-safe: only the render thread touches the tree.

#include <cstdint>
#include <memory>
#include <string>
#include <set>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Hotkey.h"

namespace MacroTree
{
    // Folders may nest this deep (root -> level 1 -> ... -> level 6). Macros can live in any folder.
    constexpr int kMaxFolderDepth = 6;
    constexpr size_t kMaxNameLength = 100; // bytes of UTF-8, same as bhm-chat-macros' title limit
    constexpr int kFormatVersion = 1;
    constexpr size_t kMaxLines = 50; // lines in one macro
    // Problems listed from one file (FromJson, an import); the rest are only counted, so a crafted
    // file can't fill memory or the preview with millions of lines (SEC-022).
    constexpr size_t kMaxWarnings = 200;
    std::string MoreWarnings(size_t aCount); // "...and 12 more problems like these."
    // Tree::FromJson's "Gave new ids to N item(s)..." warning (an import gives new ids anyway).
    const char* NewIdsFormat();
    bool IsNewIdsWarning(const std::string& aWarning);

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
        std::string      Note;             // something the user should know on success, e.g. a rename

        static Result Fail(std::string aError) { Result r; r.Ok = false; r.Error = std::move(aError); return r; }
    };

    // Where a moved node goes relative to the drop target.
    enum class EDrop : uint8_t { Before, Into, After };

    // --- Filters (docs/design.md, "Filters") ---
    // A macro is *active* when it matches where the player is. Inactive macros never fire from a
    // hotkey and are hidden from the quick menu; "Send now" in the library still works.
    constexpr size_t kMaxMapIds = 50;

    // The game mode of a Mumble map type (Mumble::EMapType as a number). Character select,
    // creation and redirects are GM_None. Armistice Bastion (the WvW lounge) counts as WvW.
    uint8_t GameModeOfMapType(uint32_t aMapType);
    // aMode: GM_PvE/GM_WvW/GM_PvP or GM_None (unknown: only macros active in every mode match).
    bool    IsActive(const Node& aMacro, uint8_t aMode, uint32_t aMapId);
    // Whether two macros can ever be active at the same time (a shared hotkey is then a conflict).
    bool    FiltersOverlap(const Node& aA, const Node& aB);

    const char* ChannelKey(EChannel aChannel);   // stable JSON key, e.g. "say", "guild3"
    const char* ChannelLabel(EChannel aChannel); // display name, e.g. "Say", "Guild 3"
    EChannel    ChannelFromKey(const std::string& aKey); // unknown keys -> Current

    // One folder (with everything inside) or macro, in the macros.json format. Used by export.
    nlohmann::json NodeToJson(const Node& aNode);

    // --- Trash bin (docs/design.md, "Trash bin") ---
    // A deleted folder or macro, kept until it's restored or deleted for good. Items in the bin
    // aren't in the tree, so they can't be sent, edited or triggered by a hotkey.
    struct TrashItem
    {
        std::unique_ptr<Node>    Item;      // Parent is nullptr while in the bin
        std::vector<uint64_t>    FromIds;   // the folders it was in, top level first (empty = top level)
        std::vector<std::string> FromNames; // their names, same order (to recreate them)
        size_t                   FromIndex = 0; // its place in that folder
        std::string              DeletedAt; // local time, "2026-11-21 14:03"
    };

    enum class ERestoreTo : uint8_t
    {
        Original,        // back into the folder it came from (fails if that folder is gone)
        RecreateFolders, // recreate the missing folders (by name) and put it there
        TopLevel,        // at the top level
    };

    class Tree // NOLINT(bugprone-exception-escape): SEC-030, accepted (MSVC set move allocates)
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
        // For a caller that keeps its own index of the names in aParent (import's Apply): aName must
        // already be clean (CleanName) and not taken by a sibling of this type. Skips the scan of
        // every sibling, which made adding N items to one folder cost N² (SEC-027). Still refuses
        // an empty name, a non-folder parent and a folder that would nest too deep.
        Result AddKnownFree(Node* aParent, ENodeType aType, const std::string& aName);
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
        // --- Trash bin ---
        // Takes aNode out of the tree into the bin (newest first). aWhen is shown in the bin.
        Result MoveToTrash(Node& aNode, const std::string& aWhen);
        const std::vector<TrashItem>& Trash() const { return m_Trash; }
        // Index of the bin item whose top node has this id, or -1.
        int    TrashIndexOf(uint64_t aId) const;
        // Whether the folder a bin item came from is still there (always true for the top level).
        bool   OriginalFolderExists(size_t aIndex) const;
        // "Raids > Wing 3", or "the top level".
        static std::string FromPathText(const TrashItem& aItem);
        // Puts a bin item back. A name already taken there gets " (2)" etc. (said in Result::Note).
        // Fails, leaving it in the bin, if the folder is gone (Original) or it would nest too deep.
        Result Restore(size_t aIndex, ERestoreTo aTo);
        Result DeleteFromTrash(size_t aIndex);
        void   EmptyTrash() { m_Trash.clear(); }

        // Every macro using this hotkey, in tree order (more than one = a conflict). Empty combo -> none.
        std::vector<Node*> MacrosWithHotkey(const Hotkey::Combo& aCombo) const;

        nlohmann::json ToJson() const;
        // Replaces the tree. Bad nodes are skipped, missing or duplicate ids are regenerated.
        // aWarnings collects what was fixed. Returns false (tree unchanged) if aJson isn't a tree at all.
        bool FromJson(const nlohmann::json& aJson, std::vector<std::string>* aWarnings = nullptr);

        static std::string CleanName(const std::string& aName); // trims spaces, cuts to kMaxNameLength
        // aName + " (n)", with aName cut short (never inside a UTF-8 sequence) so all of " (n)" fits.
        // Every n gives a different name, so a loop over n always ends.
        static std::string NumberedName(const std::string& aName, int aN);

    private:
        std::unique_ptr<Node> m_Root;
        std::vector<TrashItem> m_Trash;
        // Every id in the tree or the bin (plus some since deleted, which is harmless), filled on the
        // first NewId, so a new id is one lookup instead of a walk over the whole tree (SEC-017).
        mutable std::set<uint64_t> m_UsedIds;
        mutable bool                         m_UsedIdsReady = false;

        uint64_t NewId() const;
        bool     IdInUse(uint64_t aId) const; // in the tree or the bin
        std::string FreeName(const Node& aFolder, ENodeType aType, const std::string& aName) const;
        Result   CheckName(const Node& aParent, ENodeType aType, const std::string& aName, const Node* aIgnore) const;
        Result   Add(Node* aParent, ENodeType aType, const std::string& aName, bool aCheckName = true);
    };
}
