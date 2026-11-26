#pragma once

// The macro library: a tree of folders and macros (docs/design.md, "Data model").
// No Nexus or ImGui dependency, so it can be unit-tested on its own (tests/MacroTreeTests.cpp).
// Not thread-safe: only the render thread touches the tree.

#include <cstdint>
#include <ctime>
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

    // --- Saved files (docs/design.md, "Saved files") ---
    // The format of macros.json and of exports. Raised only by a *breaking* change: one an older
    // build would read wrongly. A new field is not breaking: older builds keep fields they don't
    // know (Extra) and write them back. Every older format stays readable: FromJson reads it by
    // its number (Parser::Format), and tests/FormatTests.cpp loads a saved file of each one.
    constexpr int kFormatVersion = 1;
    // Unknown fields kept per object: at most this many keys, nested this deep, with this many
    // values in all. More is dropped with a warning, so a crafted file can't make every save slow
    // or nest deep enough to overflow the stack when it's written back.
    constexpr size_t kMaxExtraKeys     = 32;
    constexpr int    kMaxExtraDepth    = 8;
    constexpr size_t kMaxExtraValues   = 256;
    constexpr size_t kMaxChannelKeyLen = 32; // an unknown channel's name, as kept
    constexpr size_t kMaxLines = 50; // lines in one macro
    constexpr size_t kMaxNoteLength = 1000; // bytes of UTF-8 in a macro's note (never sent)
    constexpr size_t kMaxNoteLines  = 10;   // lines in a note; later line breaks become spaces
    // Problems listed from one file (FromJson, an import); the rest are only counted, so a crafted
    // file can't fill memory or the preview with millions of lines (SEC-022).
    constexpr size_t kMaxWarnings = 200;
    std::string MoreWarnings(size_t aCount); // "...and 12 more problems like these."
    // Tree::FromJson's "Gave new ids to N item(s)..." warning (an import gives new ids anyway).
    const char* NewIdsFormat();
    bool IsNewIdsWarning(const std::string& aWarning);

    // The id reserved for the starter library's top Examples folder (src/Examples.*), so it can be
    // found again after a rename or a language change. Nothing else can ever hold it: NewId never
    // returns it, loading keeps it only on one folder (a macro with it gets a new id), and
    // imports and copies always get new ids. Only ClaimReservedId gives it out.
    constexpr uint64_t kReservedExamplesId = 0x4558414D504C4531ull; // "EXAMPLE1"

    enum class ENodeType : uint8_t { Folder, Macro };

    enum class EChannel : uint8_t
    {
        Current, Emote, Say, Map, Party, Squad, Team, Reply, Whisper,
        Guild, Guild1, Guild2, Guild3, Guild4, Guild5, Guild6,
        Count
    };

    // Bit flags; a macro with no mode set behaves like all modes.
    enum EGameMode : uint8_t { GM_None = 0, GM_PvE = 1, GM_WvW = 2, GM_PvP = 4, GM_All = 7 };

    // Fields a file had that this build doesn't know (a newer version's), kept as they were and
    // written back on save. Shared and never changed, so copying a node or line is cheap.
    using Extra = std::shared_ptr<const nlohmann::json>;

    struct Line
    {
        EChannel    Channel        = EChannel::Current;
        std::string Message;
        std::string WhisperTo;      // Whisper only
        bool        SquadBroadcast = false; // Squad only
        // A channel this build doesn't know (from a newer version), as written in the file. Such a
        // line is kept and saved as it is, but never sent (SendPlan refuses it). Empty = Channel.
        // Picking a channel in the editor clears it.
        std::string UnknownChannel;
        MacroTree::Extra Extra;
    };

    struct Node
    {
        uint64_t    Id   = 0;
        ENodeType   Type = ENodeType::Folder;
        bool        Favourite = false; // a folder or macro at the top of the quick menu (QuickLists.h)
        std::string Name;          // folder name or macro title
        Node*       Parent = nullptr;

        // Folder
        std::vector<std::unique_ptr<Node>> Children;
        bool Expanded = true;

        // Filters (folders and macros): where it's active. A folder's limit everything inside it.
        uint8_t           GameModes = GM_All;
        std::vector<int>  MapIds;

        // Macro
        std::vector<Line> Lines;
        std::string       LinkFile;
        Hotkey::Combo     Hotkey;   // empty = none
        int               CooldownSec = 0; // hotkey cooldown, 0 = off, at most Cooldown::kMaxSec
        std::string       Note;     // what it's for, shown in the editor and the quick menu; never sent

        MacroTree::Extra  Extra; // fields from a newer version (the root's: the file's own)

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
    // Folders have filters too, and they add up: a macro is active only where its own filter and
    // every folder above it allow (game modes: all must allow the mode; maps: every list that
    // isn't empty must have the map).
    constexpr size_t kMaxMapIds = 50;

    // The game mode of a Mumble map type (Mumble::EMapType as a number). Character select,
    // creation and redirects are GM_None. Armistice Bastion (the WvW lounge) counts as WvW.
    uint8_t GameModeOfMapType(uint32_t aMapType);

    // Where a node can be active, its own filter and its folders' together.
    struct Reach
    {
        uint8_t          Modes  = GM_All;
        bool             AnyMap = true; // no map list anywhere on the way up
        std::vector<int> Maps;          // else: the maps every list has (sorted); empty = none
        bool Never() const { return Modes == GM_None || (!AnyMap && Maps.empty()); }
    };
    Reach   ReachOf(const Node& aNode);
    // Its own filter only: a game mode left out or a map list (a folder shows it as a tag).
    bool    HasFilter(const Node& aNode);
    // aMode: GM_PvE/GM_WvW/GM_PvP or GM_None (unknown: only what's active in every mode matches).
    // A folder is active where its own filter and the folders above it allow.
    bool    IsActive(const Node& aNode, uint8_t aMode, uint32_t aMapId);
    // Whether it can never be active: its filters and its folders' have no game mode or no map
    // in common.
    bool    NeverActive(const Node& aNode);
    // Whether two macros can ever be active at the same time (a shared hotkey is then a conflict),
    // with their folders' filters.
    bool    FiltersOverlap(const Node& aA, const Node& aB);

    const char* ChannelKey(EChannel aChannel);   // stable JSON key, e.g. "say", "guild3"
    const char* ChannelLabel(EChannel aChannel); // display name, e.g. "Say", "Guild 3"
    EChannel    ChannelFromKey(const std::string& aKey); // unknown keys -> Current

    // One folder (with everything inside) or macro, in the macros.json format. Used by export.
    nlohmann::json NodeToJson(const Node& aNode);

    // A macro with a line for a channel this build doesn't know (it can't be sent or written as
    // text), and an item with anything from a newer version (unknown fields or such a line), for
    // a folder anywhere inside it.
    bool HasUnknownChannel(const Node& aMacro);
    bool HasNewerData(const Node& aNode);

    // Times in the files are UTC, "2026-11-21T14:03:00Z". False if aText isn't one.
    std::string UtcStamp(std::time_t aWhen);
    bool        ParseUtcStamp(const std::string& aText, std::time_t& aWhen);

    // --- Trash bin (docs/design.md, "Trash bin") ---
    // A deleted folder or macro, kept until it's restored or deleted for good. Items in the bin
    // aren't in the tree, so they can't be sent, edited or triggered by a hotkey.
    struct TrashItem
    {
        std::unique_ptr<Node>    Item;      // Parent is nullptr while in the bin
        std::vector<uint64_t>    FromIds;   // the folders it was in, top level first (empty = top level)
        std::vector<std::string> FromNames; // their names, same order (to recreate them)
        size_t                   FromIndex = 0; // its place in that folder
        std::string              DeletedAt; // UTC, "2026-11-21T14:03:00Z" (UtcStamp)
        MacroTree::Extra         Extra;     // the entry's fields from a newer version
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
        // Takes aNode out of the tree into the bin (newest first). aWhen (UtcStamp) is shown in the bin.
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

        // "kind": "library" (macros.json, backups) or "export". The format written is Format().
        nlohmann::json ToJson(const char* aKind = "library") const;
        // Replaces the tree. Bad nodes are skipped, missing or duplicate ids are regenerated.
        // aWarnings collects what was fixed. Returns false (tree unchanged) if aJson isn't a tree at all.
        bool FromJson(const nlohmann::json& aJson, std::vector<std::string>* aWarnings = nullptr);

        // The format it's written in: kFormatVersion (an older file is upgraded as it's read), or
        // a newer version's number. Newer: it's read-only (Storage never saves it over the file).
        int  Format() const { return m_FormatRead > kFormatVersion ? m_FormatRead : kFormatVersion; }
        bool FromNewerVersion() const { return m_FormatRead > kFormatVersion; }
        // The format the file had (kFormatVersion for a new tree). Older: Storage keeps a copy of
        // that file ("macros.json.v1.bak") before it first saves over it.
        int  FormatRead() const { return m_FormatRead; }
        // "saved_by" from the file: the version that wrote it (empty if it didn't say).
        const std::string& SavedBy() const { return m_SavedBy; }

        static std::string CleanName(const std::string& aName); // trims spaces, cuts to kMaxNameLength
        // A note as kept: control characters but '\n' become spaces ("\r\n" becomes "\n"), at
        // most kMaxNoteLines lines, trailing spaces and line breaks go, cut to kMaxNoteLength
        // (never inside a character).
        // aCut (optional): set to whether text was cut off (not just spaces trimmed).
        static std::string CleanNote(const std::string& aNote, bool* aCut = nullptr);

        // Copies a macro right after itself: "Name (2)" (a name already ending in " (n)" gets the
        // next free number instead), with its lines, filters, cooldown, note and a newer version's
        // fields, but no hotkey (it must stay unique), favourite mark or text file link, and a new
        // id. Result::Target is the copy. Folders are refused.
        Result Duplicate(Node& aMacro);

        // Gives aFolder the reserved Examples id. False (and nothing changes) if aFolder isn't a
        // folder in the tree, or a folder in the tree or the bin already has it.
        bool ClaimReservedId(Node& aFolder);
        // Gives whatever holds the reserved Examples id (in the tree or the bin) an ordinary new
        // id, so it becomes an ordinary folder. False if nothing holds it.
        bool ReleaseReservedId();
        // aName + " (n)", with aName cut short (never inside a UTF-8 sequence) so all of " (n)" fits.
        // Every n gives a different name, so a loop over n always ends.
        static std::string NumberedName(const std::string& aName, int aN);

    private:
        std::unique_ptr<Node> m_Root;
        std::vector<TrashItem> m_Trash;
        Extra                  m_RootExtra; // the "root" object's own fields from a newer version
        int                    m_FormatRead = kFormatVersion;
        std::string            m_SavedBy;
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
