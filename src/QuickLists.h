#pragma once
// The quick menu's top lists (docs/design.md, "Favourites and recently sent"): the favourite
// folders and macros (shown wherever you are), and the macros sent last. No Nexus or ImGui dependency. Tested by
// tests/QuickListsTests.cpp.

#include <cstdint>
#include <functional>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "MacroTree.h"

namespace QuickLists
{
    constexpr int kMaxRecent     = 10; // how many sent macros are remembered (and may be shown)
    constexpr int kDefaultRecent = 5;  // shown by default
    constexpr int kMaxFavourites = 10; // folders and macros marked as favourites, at most

    // The macros sent last, newest first, by id. Kept in settings.json "recently_sent".
    class Recent
    {
    public:
        // A send of the macro started: it goes to the front (once).
        void Sent(uint64_t aId);
        void Clear() { m_Ids.clear(); }
        const std::vector<uint64_t>& Ids() const { return m_Ids; }

        // A list of ids as 16-digit hex strings, like macros.json. Anything else is skipped.
        void FromJson(const nlohmann::json& aJson);
        nlohmann::json ToJson() const;

    private:
        std::vector<uint64_t> m_Ids;
    };

    // How many recently sent macros to show: 0 (off) to kMaxRecent; anything else is the default.
    int CountFromJson(const nlohmann::json& aJson);

    // Folders and macros marked as favourites in the tree (not the trash bin).
    size_t CountFavourites(const MacroTree::Tree& aTree);
    // Whether one more may be marked (fewer than kMaxFavourites now).
    bool CanAddFavourite(const MacroTree::Tree& aTree);

    // Whether a folder has a macro, at any depth, that aActive accepts.
    bool HasActive(const MacroTree::Node& aFolder, const std::function<bool(const MacroTree::Node&)>& aActive);

    struct Favourite
    {
        const MacroTree::Node* Item = nullptr; // a folder or a macro
        bool Active = true; // a macro active here, or a folder with one inside (else shown dimmed)
    };

    struct Lists
    {
        std::vector<Favourite>              Favourites; // tree order, at most kMaxFavourites
        std::vector<const MacroTree::Node*> Recent;     // newest first, without favourite macros
    };

    // What the quick menu shows at its top: every favourite folder and macro, wherever you are
    // (aActive only marks which are active here), then up to aRecentCount of the recently sent
    // macros that aActive accepts and aren't favourites. Ids that are gone (deleted, in the trash
    // bin) are skipped. A file with more than kMaxFavourites marked shows the first ones.
    Lists Build(const MacroTree::Tree& aTree, const Recent& aRecent, int aRecentCount,
                const std::function<bool(const MacroTree::Node&)>& aActive);
}
