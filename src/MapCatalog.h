#pragma once

// The map list behind the filter editor's map picker and {map} (docs/design.md, "Filters" and
// "Placeholders"): map names and regions from the official GW2 API (/v2/maps, no API key), the
// API's full list of map ids, and the maps the player has been to. The names and the list are a
// cache in <GW2>\addons\GW2_Chat_Messages\maps.json; the maps visited are the player's own, so
// they're in settings.json ("visited_maps") and survive the cache being thrown away. No Nexus,
// ImGui or windows.h dependency: the network and the threads are in MapNames.*
// (tests/MapCatalogTests.cpp).
//
// maps.json is rebuilt from the API, so it's only trusted as far as its shape: a file that
// doesn't parse, has another format number or wrongly typed fields is thrown away.

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace MapCatalog
{
    constexpr int     kFormat         = 2;
    constexpr const char* kVisitedSetting = "visited_maps"; // settings.json: the maps visited
    constexpr size_t  kMaxMaps        = 20000;    // the API lists about 1,500
    constexpr size_t  kMaxNameBytes   = 200;
    constexpr size_t  kMaxVisited     = 200;
    constexpr int64_t kRefreshSeconds = 7 * 24 * 60 * 60; // the full list is fetched again after a week
    constexpr int64_t kRecheckSeconds = 24 * 60 * 60;     // after a game update, once more a day later
                                                          // (the API may list new maps late)

    struct Map
    {
        std::string Name;
        std::string Region; // "Kryta"; may be empty
        std::string Folded; // Name folded for searching (Search::Fold)
    };

    struct Cache // NOLINT(bugprone-exception-escape): SEC-030, accepted (MSVC set move allocates)
    {
        std::string Lang = "en";                    // the API language the names are in
        std::unordered_map<uint32_t, Map> Maps;     // every map whose name is known
        std::set<uint32_t> Listed;                  // the API's full list of ids (empty: not fetched yet)
        int64_t ListedAt = 0;                       // when the full list was fetched (UTC, seconds)
        std::vector<uint32_t> Visited;              // maps the player has been to, most recent first
        // The game's build (MumbleLink's BuildID), so a game update fetches the list again:
        uint32_t Build = 0;                         // the newest build seen (0: none yet)
        int64_t  BuildSeenAt = 0;                   // when it was first seen (UTC, seconds; 0: no recheck)
        uint32_t ListedBuild = 0;                   // the build when the full list was fetched
    };

    // The API language for the addon's language: "de", "fr", "es" and "zh" have map names in the
    // API, anything else gets English.
    std::string ApiLang(const std::string& aAddonLang);

    // /v2/maps: a JSON array of ids. False if it isn't one (or is too long).
    bool ParseIds(const std::string& aBody, std::vector<uint32_t>& aOut);
    // /v2/maps?ids=...: an array of map objects; each with an id and a name is added to aOut
    // (region_name too). False if the answer isn't an array.
    bool ParseMaps(const std::string& aBody, std::unordered_map<uint32_t, Map>& aOut);

    // Reads maps.json. The names and the full list are only kept when the file's language is
    // aLang. False when nothing usable was in it. Neither reads or writes the maps visited.
    bool FromJson(const std::string& aText, const std::string& aLang, Cache& aOut);
    std::string ToJson(const Cache& aCache);

    // The maps visited, as kept in settings.json (kVisitedSetting): ids, most recent first, at
    // most kMaxVisited. Anything that isn't a map id is skipped.
    std::vector<uint32_t> VisitedFromJson(const nlohmann::json& aValue);
    nlohmann::json        VisitedToJson(const Cache& aCache);

    // Whether the full list should be fetched (again): never fetched, older than a week,
    // apparently from the future (a clock change), fetched before the game's newest build, or a
    // day after a new build was first seen when it hasn't been fetched since (the recheck).
    bool RefreshDue(const Cache& aCache, int64_t aNow);

    // The game's build, as read now (0 = not known yet, ignored). Returns true when it's a new
    // one (the cache changed and should be saved). The very first build ever seen is only
    // remembered: a list fetched before it (an older addon version, or before MumbleLink had it)
    // counts as that build's, so a first start doesn't fetch twice.
    bool NoteBuild(Cache& aCache, uint32_t aBuild, int64_t aNow);
    // A full list was fetched now: it's the current build's.
    void ListFetched(Cache& aCache, int64_t aNow);

    // Puts aMapId first in the visited list (0 is ignored). Returns false if it was already first.
    bool NoteVisit(Cache& aCache, uint32_t aMapId);

    // Whether the full list is known and doesn't have aMapId (a map removed or replaced in a game
    // update, or an instance the API doesn't list).
    bool Unlisted(const Cache& aCache, uint32_t aMapId);

    // Maps whose name contains aQuery (case and accents ignored), or whose id is aQuery. Visited
    // maps come first (most recent first), then the rest by name, then id. At most aMax.
    std::vector<uint32_t> Find(const Cache& aCache, const std::string& aQuery, size_t aMax);

    Map MakeMap(const std::string& aName, const std::string& aRegion);
}
