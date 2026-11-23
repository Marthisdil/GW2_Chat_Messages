#pragma once

// Import and export of folders and macros (docs/design.md, "Import / export").
// No Nexus or ImGui dependency (tested by tests/ExchangeTests.cpp). Reads files with
// std::filesystem; the UI decides which paths.
//
// An import never touches the library directly: a source is first read into a Package (its own
// small tree), then BuildPlan shows what would happen (new, merged, renamed, replaced, skipped,
// too deep), and only Apply changes the library.

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "MacroTree.h"

namespace Exchange
{
    using MacroTree::Node;
    using MacroTree::Tree;

    constexpr size_t kMaxJsonBytes   = 4 * 1024 * 1024; // an export file
    constexpr size_t kMaxTextBytes   = 256 * 1024;      // one .txt macro
    constexpr size_t kMaxFolderFiles = 2000;            // .txt files read from one folder
    constexpr size_t kMaxFolderBytes = 16 * 1024 * 1024; // their text in all (SEC-025)
    constexpr size_t kMaxImportItems = 5000;            // folders + macros in one import

    // --- Export ---------------------------------------------------------------------------------

    // The export format: the same as macros.json, with these nodes at the top level. Link files
    // are left out (they're paths on this PC), and so is the trash bin (it isn't in the tree).
    nlohmann::json ExportJson(const std::vector<const Node*>& aNodes);
    std::string    ExportJsonText(const std::vector<const Node*>& aNodes); // pretty-printed
    // A macro as raw text, one line per row ("/s hi").
    std::string    ExportText(const Node& aMacro);
    // A safe Windows file name from a folder or macro name, with the extension (".json", ".txt").
    std::string    FileNameFor(const std::string& aName, const std::string& aExt);

    // --- Reading a source -----------------------------------------------------------------------

    struct Package
    {
        Tree                     Items;   // what was read, at its top level
        std::string              Error;   // set if nothing could be read
        std::vector<std::string> Warnings;
        std::string              Source;  // what was read, for display ("Pasted text", a file name)

        bool Ok() const { return Error.empty(); }
    };

    // Our export format or a whole macros.json. A trash bin in it is never imported (a warning
    // says so).
    Package FromJsonText(const std::string& aText);
    // Raw text (one row per chat line) as one macro called aName.
    Package FromRawText(const std::string& aName, const std::string& aText);
    // Pasted text: JSON if it looks like JSON, otherwise raw text.
    Package FromText(const std::string& aName, const std::string& aText);
    // A .json or .txt file, or a folder of .txt files (sub-folders become folders).
    Package FromPath(const std::filesystem::path& aPath);

    // --- A whole library (someone else's macros.json) -------------------------------------------
    // Read like an export (its trash bin and link files are left out), but it's put into one new
    // folder at the top level (WrapInFolder), so it never mixes into the user's own folders.

    // macros.json or its backup, macros.json.bak: a .json or .bak file.
    bool    IsLibraryFileName(const std::filesystem::path& aPath);
    Package FromLibraryFile(const std::filesystem::path& aPath);
    // The suggested folder name: "From <file name>" ("From Alex" for Alex.json), or "Imported
    // library" for a file just called macros.json / macros.json.bak.
    std::string LibraryFolderName(const std::string& aSource);
    constexpr const char* kLibraryFolderFallback = "Imported library";
    // Moves everything at the package's top level into one new folder called aName (cleaned;
    // kLibraryFolderFallback if empty). Returns that folder, or nullptr if the package is empty.
    Node* WrapInFolder(Package& aPkg, const std::string& aName);

    // --- Preview and import ---------------------------------------------------------------------

    // What to do when an item has the same name as one already in the target folder.
    enum class EChoice : uint8_t { Rename, Skip, Replace, Merge /* folders only */ };
    const char* ChoiceLabel(EChoice aChoice);

    enum class EOutcome : uint8_t
    {
        New,       // added as it is
        Renamed,   // added as "Name (2)"
        Merged,    // a folder whose contents go into the existing folder of that name
        Replaced,  // the existing item's contents are replaced
        Skipped,   // left out (with everything inside)
        TooDeep,   // a folder that would be nested past level 6: left out
        Flattened, // a folder that would be too deep: left out, its contents go one level up
        NotSelected, // unticked in the preview: left out (with everything inside)
    };

    struct Options
    {
        EChoice MacroConflict  = EChoice::Rename;
        EChoice FolderConflict = EChoice::Merge;
        bool    Flatten        = false; // too-deep folders: flatten (true) or leave out (false)
        bool    KeepHotkeys    = false; // off unless ticked: an imported hotkey sends its macro on that key
        std::map<const Node*, EChoice> PerItem; // overrides, keyed by the package's node
        std::set<const Node*> Unselected;       // unticked in the preview, keyed by the package's node
    };

    // --- Choosing what to import (the preview's checkboxes) ---
    // Everything is ticked at first. Ticking or unticking a folder does the same to everything
    // inside it. Ticking a macro (or folder) also ticks the folders above it, so it has somewhere
    // to go; a folder whose contents are all unticked is unticked too (an empty folder keeps its own
    // tick). A folder that's ticked imports only its ticked contents.
    bool   IsSelected(const Options& aOptions, const Node& aNode);
    void   SetSelected(Options& aOptions, const Node& aNode, bool aOn);
    void   SelectAll(Options& aOptions, const Tree& aPackage, bool aOn);
    // Whether a folder has both ticked and unticked items inside (shown as "some").
    bool   IsPartlySelected(const Options& aOptions, const Node& aFolder);

    // Why an imported macro's hotkey is flagged in the preview.
    enum class EHotkeyRisk : uint8_t
    {
        None,
        NoModifier, // no Ctrl, Alt or Shift: it fires on a key used while playing
        Clash,      // the same as one of the user's own hotkeys (other than the macro it replaces)
    };

    struct PlanItem
    {
        const Node*           Src = nullptr;  // the package's node
        EOutcome              Outcome = EOutcome::New;
        std::string           Name;           // its name after the import
        bool                  Conflict = false; // an item of that name was already there
        EChoice               Choice = EChoice::Rename; // the choice used (when Conflict)
        std::vector<PlanItem> Children;       // folders: what happens inside
        // Macros that are imported (new, renamed or replacing), for the preview's flags:
        bool        ReadsFile = false;        // a line uses {txt} (reads a file in the addon folder)
        bool        ReadsWeb  = false;        // a line uses {json} (fetches a web page)
        EHotkeyRisk HotkeyRisk = EHotkeyRisk::None; // its hotkey, whether or not Keep hotkeys is ticked
        // Replace with Keep hotkeys ticked and a risky hotkey: the macro comes without it.
        bool        HotkeyCleared = false;
        // Replace where the existing macro had a hotkey of its own, which goes.
        bool        LosesOwnHotkey = false;
    };

    struct Plan
    {
        std::vector<PlanItem> Items;
        size_t NewFolders = 0, NewMacros = 0, Merged = 0, Renamed = 0, Replaced = 0, Skipped = 0, TooDeep = 0;
        size_t NotSelected = 0; // folders and macros unticked in the preview
        size_t Conflicts = 0;
        // Counts of the PlanItem flags above, over the macros that are imported. HotkeysNoModifier
        // and HotkeysClash count only hotkeys that aren't cleared (they come along if kept).
        size_t ReadsFile = 0, ReadsWeb = 0, HotkeysNoModifier = 0, HotkeysClash = 0, HotkeysCleared = 0, OwnHotkeysLost = 0;
        size_t HotkeysChanged = 0; // replaced macros with HotkeyCleared or LosesOwnHotkey (the notice after Import)

        size_t Changes() const { return NewFolders + NewMacros + Replaced; }
    };

    // What importing aPackage into aTarget (a folder of aTree) would do. Changes nothing.
    // Imported macros are flagged when they read files or web pages, or when their hotkey has no
    // modifier or is one aTree already uses. Replace never brings such a hotkey, even with Keep
    // hotkeys ticked (HotkeyCleared); other new macros only show the flag.
    Plan BuildPlan(const Tree& aTree, const Node& aTarget, const Tree& aPackage, const Options& aOptions);

    // Imports by the plan (built from the same tree, target and package, unchanged since).
    // Returns the number of folders and macros added or replaced.
    size_t Apply(Tree& aTree, Node& aTarget, const Plan& aPlan, const Options& aOptions);
}
