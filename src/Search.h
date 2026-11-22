#pragma once

// The library's search box (docs/design.md, "Search"): which folders and macros match what was
// typed. No Nexus or ImGui dependency, so it can be unit-tested (tests/SearchTests.cpp).
// Matching ignores case and accents ("cafe" finds "Café"). A macro matches on its name, the text
// of its lines, a whisper's recipient, or its hotkey ("ctrl+f1"); a folder on its name.

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "MacroTree.h"

namespace Search
{
    // Lower case with accents removed (Latin-1 letters and œ; ß -> "ss"). Other text is kept as
    // is. aMap, if given, gets the byte offset in aText where each byte of the result came from.
    std::string Fold(const std::string& aText, std::vector<size_t>* aMap = nullptr);

    // Where aQuery (trimmed, folded) is in aText, as a byte range of aText, for highlighting.
    bool FindRange(const std::string& aText, const std::string& aQuery, size_t& aStart, size_t& aLen);

    // Whether one node matches an already folded, trimmed, non-empty query.
    bool NodeMatches(const MacroTree::Node& aNode, const std::string& aFoldedQuery);

    // "Only active here" (the library's checkbox, and always for the quick menu): only macros that
    // are active in this game mode and on this map (MacroTree::IsActive) are shown.
    struct Where
    {
        bool     ActiveOnly = false;
        uint8_t  Mode  = MacroTree::GM_None;
        uint32_t MapId = 0;
    };

    struct Result
    {
        bool Active = false;                    // false: no search text and no filter, show the whole tree
        std::unordered_set<uint64_t> Matches;   // the nodes that match the text (highlighted)
        std::unordered_set<uint64_t> Visible;   // every node shown
        std::unordered_set<uint64_t> Open;      // folders above a match (or, filtering, a shown macro): opened
        std::unordered_set<uint64_t> Inside;    // shown because they're inside a matched folder
        size_t FolderMatches = 0;
        size_t MacroMatches  = 0;
        size_t ShownMacros   = 0;               // macros shown (matches plus, without the filter, those inside matched folders)

        bool Shows(uint64_t aId) const { return !Active || Visible.count(aId); }
        // A folder with a match below it is opened while searching (its own open state is kept).
        bool ForcedOpen(uint64_t aId) const { return Active && Open.count(aId); }
    };

    // Searches the tree under aRoot (the root itself never matches) into aOut, which is cleared
    // first (filled in place: the UI runs it every frame, so its sets keep their memory).
    // The trash bin isn't searched.
    // - Text only: matches, the folders above them, and everything inside a matched folder.
    // - aWhere.ActiveOnly: only macros active there are shown (with the folders above them); with
    //   text too, an active macro shows when it matches or sits in a matched folder.
    void Run(const MacroTree::Node& aRoot, const std::string& aQuery, const Where& aWhere, Result& aOut);
    void Run(const MacroTree::Node& aRoot, const std::string& aQuery, Result& aOut); // no filter

    std::string Trim(const std::string& aText);
}
