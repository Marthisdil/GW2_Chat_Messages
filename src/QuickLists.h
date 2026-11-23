#pragma once
// The quick menu's top lists (docs/design.md, "Favourites and recently sent"): the favourite
// macros, and the ones sent last. No Nexus or ImGui dependency. Tested by
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

    struct Lists
    {
        std::vector<const MacroTree::Node*> Favourites; // in tree order
        std::vector<const MacroTree::Node*> Recent;     // newest first, without the favourites
    };

    // What the quick menu shows at its top: the favourite macros that aShow accepts (active here),
    // then up to aRecentCount of the recently sent ones it accepts that aren't favourites. Ids
    // that are gone (deleted, in the trash bin) are skipped.
    Lists Build(const MacroTree::Tree& aTree, const Recent& aRecent, int aRecentCount,
                const std::function<bool(const MacroTree::Node&)>& aShow);
}
