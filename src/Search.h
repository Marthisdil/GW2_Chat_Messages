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

    struct Result
    {
        bool Active = false;                    // false: the box is empty, show the whole tree
        std::unordered_set<uint64_t> Matches;   // the nodes that match (highlighted)
        std::unordered_set<uint64_t> Open;      // folders above a match: shown and opened
        std::unordered_set<uint64_t> Inside;    // everything inside a matched folder: shown as is
        size_t FolderMatches = 0;
        size_t MacroMatches  = 0;

        bool Shows(uint64_t aId) const { return !Active || Matches.count(aId) || Open.count(aId) || Inside.count(aId); }
        // A folder with a match below it is opened while searching (its own open state is kept).
        bool ForcedOpen(uint64_t aId) const { return Active && Open.count(aId); }
    };

    // Searches the tree under aRoot (the root itself never matches). The trash bin isn't searched.
    Result Run(const MacroTree::Node& aRoot, const std::string& aQuery);

    std::string Trim(const std::string& aText);
}
