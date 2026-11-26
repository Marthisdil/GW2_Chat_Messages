#include "MapCatalog.h"

#include <algorithm>
#include <charconv>

#include <nlohmann/json.hpp>

#include "Localization.h" // SafeText
#include "Search.h"       // Fold, Trim

namespace
{
    using json = nlohmann::json;

    bool ParseId(const std::string& aText, uint32_t& aOut)
    {
        if (aText.empty() || aText.size() > 10) return false;
        const auto [end, ec] = std::from_chars(aText.data(), aText.data() + aText.size(), aOut);
        return ec == std::errc() && end == aText.data() + aText.size() && aOut != 0;
    }

    // A name or region from the API or the cache: shown in the UI and typed by {map}, so it must
    // be clean text of a sensible length (no control or bidi characters, no "##").
    bool Clean(const std::string& aText, bool aMayBeEmpty)
    {
        if (aText.empty()) return aMayBeEmpty;
        return aText.size() <= MapCatalog::kMaxNameBytes && Localization::SafeText(aText, false);
    }

    // Reads one {"id": n, "name": "...", "region_name": "..."} object. The cache uses the same shape.
    bool ReadMap(const json& aObj, uint32_t& aId, MapCatalog::Map& aMap)
    {
        if (!aObj.is_object()) return false;
        const auto id = aObj.find("id");
        const auto name = aObj.find("name");
        if (id == aObj.end() || !id->is_number_unsigned() || name == aObj.end() || !name->is_string()) return false;
        const uint64_t raw = id->get<uint64_t>();
        if (raw == 0 || raw > 0xFFFFFFFFull) return false;
        std::string region;
        const auto reg = aObj.find("region_name");
        if (reg != aObj.end() && reg->is_string()) region = reg->get<std::string>();
        const std::string& n = name->get_ref<const std::string&>();
        if (!Clean(n, false)) return false;
        if (!Clean(region, true)) region.clear();
        aId = static_cast<uint32_t>(raw);
        aMap = MapCatalog::MakeMap(n, region);
        return true;
    }
}

MapCatalog::Map MapCatalog::MakeMap(const std::string& aName, const std::string& aRegion)
{
    Map m;
    m.Name = aName;
    m.Region = aRegion;
    m.Folded = Search::Fold(aName);
    return m;
}

std::string MapCatalog::ApiLang(const std::string& aAddonLang)
{
    const std::string base = aAddonLang.substr(0, aAddonLang.find('-'));
    if (base == "de" || base == "fr" || base == "es" || base == "zh") return base;
    return "en";
}

bool MapCatalog::ParseIds(const std::string& aBody, std::vector<uint32_t>& aOut)
{
    aOut.clear();
    const json j = json::parse(aBody, nullptr, false);
    if (j.is_discarded() || !j.is_array() || j.size() > kMaxMaps) return false;
    for (const json& v : j)
    {
        if (!v.is_number_unsigned()) continue;
        const uint64_t raw = v.get<uint64_t>();
        if (raw != 0 && raw <= 0xFFFFFFFFull) aOut.push_back(static_cast<uint32_t>(raw));
    }
    std::sort(aOut.begin(), aOut.end());
    aOut.erase(std::unique(aOut.begin(), aOut.end()), aOut.end());
    return true;
}

bool MapCatalog::ParseMaps(const std::string& aBody, std::unordered_map<uint32_t, Map>& aOut)
{
    const json j = json::parse(aBody, nullptr, false);
    if (j.is_discarded() || !j.is_array()) return false;
    for (const json& m : j)
    {
        if (aOut.size() >= kMaxMaps) break;
        uint32_t id = 0;
        Map map;
        if (ReadMap(m, id, map)) aOut[id] = std::move(map);
    }
    return true;
}

bool MapCatalog::FromJson(const std::string& aText, const std::string& aLang, Cache& aOut)
{
    aOut = Cache();
    aOut.Lang = aLang;
    const json j = json::parse(aText, nullptr, false);
    // Fields are checked with find + type, never value(), which throws on a wrong type (SEC-021).
    if (j.is_discarded() || !j.is_object()) return false;
    const auto format = j.find("format");
    if (format == j.end() || !format->is_number_integer() || format->get<long long>() != kFormat) return false;

    // The game's build: about the game, not the language, so it's kept whatever the language.
    const auto readU32 = [&j](const char* aKey, uint32_t& aOut) {
        const auto it = j.find(aKey);
        if (it != j.end() && it->is_number_unsigned() && it->get<uint64_t>() <= 0xFFFFFFFFull)
            aOut = static_cast<uint32_t>(it->get<uint64_t>());
    };
    readU32("build", aOut.Build);
    readU32("listed_build", aOut.ListedBuild);
    const auto seen = j.find("build_seen_at");
    // Only a plausible time (up to the year 3000), so RefreshDue's sums can't overflow.
    constexpr int64_t kLatest = 32503680000;
    if (seen != j.end() && seen->is_number_integer() && aOut.Build != 0)
    {
        const int64_t at = seen->get<int64_t>();
        if (at > 0 && at <= kLatest) aOut.BuildSeenAt = at;
    }

    bool any = false;
    const auto lang = j.find("lang");
    if (lang == j.end() || !lang->is_string() || lang->get_ref<const std::string&>() != aLang) return any;

    const auto maps = j.find("maps");
    if (maps != j.end() && maps->is_array())
        for (const json& m : *maps)
        {
            if (aOut.Maps.size() >= kMaxMaps) break;
            uint32_t id = 0;
            Map map;
            if (ReadMap(m, id, map)) { aOut.Maps[id] = std::move(map); any = true; }
        }
    const auto listed = j.find("listed");
    const auto at = j.find("listed_at");
    if (listed != j.end() && listed->is_array() && listed->size() <= kMaxMaps && at != j.end() && at->is_number_integer())
    {
        for (const json& v : *listed)
            if (v.is_number_unsigned() && v.get<uint64_t>() != 0 && v.get<uint64_t>() <= 0xFFFFFFFFull)
                aOut.Listed.insert(static_cast<uint32_t>(v.get<uint64_t>()));
        aOut.ListedAt = aOut.Listed.empty() ? 0 : at->get<int64_t>();
        any = any || !aOut.Listed.empty();
    }
    return any;
}

std::string MapCatalog::ToJson(const Cache& aCache)
{
    // Sorted by id, so the file doesn't change order from one save to the next.
    std::vector<uint32_t> ids;
    ids.reserve(aCache.Maps.size());
    for (const auto& kv : aCache.Maps) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    json maps = json::array();
    for (uint32_t id : ids)
    {
        const Map& m = aCache.Maps.at(id);
        json o = {{"id", id}, {"name", m.Name}};
        if (!m.Region.empty()) o["region_name"] = m.Region;
        maps.push_back(std::move(o));
    }
    json j = {{"format", kFormat}, {"lang", aCache.Lang}, {"maps", std::move(maps)}};
    if (aCache.Build != 0)
    {
        j["build"] = aCache.Build;
        if (aCache.BuildSeenAt > 0) j["build_seen_at"] = aCache.BuildSeenAt;
    }
    if (aCache.ListedBuild != 0) j["listed_build"] = aCache.ListedBuild;
    if (!aCache.Listed.empty())
    {
        j["listed"] = std::vector<uint32_t>(aCache.Listed.begin(), aCache.Listed.end());
        j["listed_at"] = aCache.ListedAt;
    }
    // Names come from the API and may hold any UTF-8; a bad sequence is replaced, not thrown.
    return j.dump(1, '\t', false, json::error_handler_t::replace);
}

std::vector<uint32_t> MapCatalog::VisitedFromJson(const json& aValue)
{
    std::vector<uint32_t> out;
    if (!aValue.is_array()) return out;
    for (const json& v : aValue)
    {
        if (out.size() >= kMaxVisited) break;
        if (!v.is_number_unsigned()) continue;
        const uint64_t raw = v.get<uint64_t>();
        if (raw == 0 || raw > 0xFFFFFFFFull) continue;
        const uint32_t id = static_cast<uint32_t>(raw);
        if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    }
    return out;
}

json MapCatalog::VisitedToJson(const Cache& aCache) { return aCache.Visited; }

bool MapCatalog::RefreshDue(const Cache& aCache, int64_t aNow)
{
    if (aCache.Listed.empty() || aCache.ListedAt <= 0) return true;
    if (aCache.ListedAt > aNow + 24 * 60 * 60) return true; // from the future: the clock changed
    if (aNow - aCache.ListedAt >= kRefreshSeconds) return true;
    if (aCache.Build != 0 && aCache.Build != aCache.ListedBuild) return true; // a game update since
    // A day after the update, once more, unless it was fetched since then.
    return aCache.BuildSeenAt > 0 && aNow >= aCache.BuildSeenAt + kRecheckSeconds &&
           aCache.ListedAt < aCache.BuildSeenAt + kRecheckSeconds;
}

bool MapCatalog::NoteBuild(Cache& aCache, uint32_t aBuild, int64_t aNow)
{
    if (aBuild == 0 || aBuild == aCache.Build) return false;
    const bool first = aCache.Build == 0 && aCache.ListedBuild == 0;
    aCache.Build = aBuild;
    if (first)
    {
        // Nothing to compare with: take the list we have (if any) as this build's.
        aCache.BuildSeenAt = 0;
        if (!aCache.Listed.empty()) aCache.ListedBuild = aBuild;
        return true;
    }
    aCache.BuildSeenAt = aNow;
    return true;
}

void MapCatalog::ListFetched(Cache& aCache, int64_t aNow)
{
    aCache.ListedAt = aNow;
    aCache.ListedBuild = aCache.Build;
}

bool MapCatalog::NoteVisit(Cache& aCache, uint32_t aMapId)
{
    if (aMapId == 0) return false;
    auto& v = aCache.Visited;
    if (!v.empty() && v.front() == aMapId) return false;
    const auto it = std::find(v.begin(), v.end(), aMapId);
    if (it != v.end()) v.erase(it);
    v.insert(v.begin(), aMapId);
    if (v.size() > kMaxVisited) v.resize(kMaxVisited);
    return true;
}

bool MapCatalog::Unlisted(const Cache& aCache, uint32_t aMapId)
{
    return !aCache.Listed.empty() && aMapId != 0 && !aCache.Listed.count(aMapId);
}

std::vector<uint32_t> MapCatalog::Find(const Cache& aCache, const std::string& aQuery, size_t aMax)
{
    std::vector<uint32_t> out;
    const std::string q = Search::Fold(Search::Trim(aQuery));
    if (q.empty() || aMax == 0) return out;
    uint32_t wantId = 0;
    const bool isId = ParseId(q, wantId);

    auto matches = [&](uint32_t aId) {
        if (isId && aId == wantId) return true;
        const auto it = aCache.Maps.find(aId);
        return it != aCache.Maps.end() && it->second.Folded.find(q) != std::string::npos;
    };

    for (uint32_t id : aCache.Visited)
        if (out.size() < aMax && matches(id)) out.push_back(id);

    std::vector<uint32_t> rest;
    for (const auto& kv : aCache.Maps)
        if (matches(kv.first) && std::find(out.begin(), out.end(), kv.first) == out.end()) rest.push_back(kv.first);
    if (isId && !aCache.Maps.count(wantId) && std::find(out.begin(), out.end(), wantId) == out.end() &&
        aCache.Listed.count(wantId))
        rest.push_back(wantId); // a listed id whose name isn't known yet
    std::sort(rest.begin(), rest.end(), [&](uint32_t a, uint32_t b) {
        const auto ia = aCache.Maps.find(a), ib = aCache.Maps.find(b);
        const std::string& na = ia != aCache.Maps.end() ? ia->second.Folded : std::string();
        const std::string& nb = ib != aCache.Maps.end() ? ib->second.Folded : std::string();
        if (na != nb) return na < nb;
        return a < b;
    });
    for (uint32_t id : rest)
    {
        if (out.size() >= aMax) break;
        out.push_back(id);
    }
    return out;
}
