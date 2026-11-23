#include "QuickLists.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <nlohmann/json.hpp>

namespace
{
    bool ParseId(const std::string& s, uint64_t& aOut)
    {
        if (s.size() != 16) return false;
        uint64_t v = 0;
        for (char c : s)
        {
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            v = (v << 4) | static_cast<uint64_t>(d);
        }
        if (v == 0) return false;
        aOut = v;
        return true;
    }

    void CollectFavourites(const MacroTree::Node& aFolder, const std::function<bool(const MacroTree::Node&)>& aShow,
                           std::vector<const MacroTree::Node*>& aOut)
    {
        for (const auto& c : aFolder.Children)
        {
            if (c->IsFolder()) CollectFavourites(*c, aShow, aOut);
            else if (c->Favourite && (!aShow || aShow(*c))) aOut.push_back(c.get());
        }
    }
}

void QuickLists::Recent::Sent(uint64_t aId)
{
    if (aId == 0) return;
    m_Ids.erase(std::remove(m_Ids.begin(), m_Ids.end(), aId), m_Ids.end());
    m_Ids.insert(m_Ids.begin(), aId);
    if (m_Ids.size() > static_cast<size_t>(kMaxRecent)) m_Ids.resize(kMaxRecent);
}

void QuickLists::Recent::FromJson(const nlohmann::json& aJson)
{
    m_Ids.clear();
    if (!aJson.is_array()) return;
    for (const nlohmann::json& j : aJson)
    {
        if (m_Ids.size() >= static_cast<size_t>(kMaxRecent)) break;
        uint64_t id = 0;
        if (!j.is_string() || !ParseId(j.get<std::string>(), id)) continue;
        if (std::find(m_Ids.begin(), m_Ids.end(), id) == m_Ids.end()) m_Ids.push_back(id);
    }
}

nlohmann::json QuickLists::Recent::ToJson() const
{
    nlohmann::json out = nlohmann::json::array();
    for (uint64_t id : m_Ids)
    {
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(id));
        out.push_back(buf);
    }
    return out;
}

int QuickLists::CountFromJson(const nlohmann::json& aJson)
{
    if (!aJson.is_number_integer()) return kDefaultRecent;
    if (aJson.is_number_unsigned())
    {
        const uint64_t v = aJson.get<uint64_t>();
        return v <= static_cast<uint64_t>(kMaxRecent) ? static_cast<int>(v) : kDefaultRecent;
    }
    const int64_t v = aJson.get<int64_t>();
    return v >= 0 && v <= kMaxRecent ? static_cast<int>(v) : kDefaultRecent;
}

QuickLists::Lists QuickLists::Build(const MacroTree::Tree& aTree, const Recent& aRecent, int aRecentCount,
                                    const std::function<bool(const MacroTree::Node&)>& aShow)
{
    Lists out;
    CollectFavourites(aTree.Root(), aShow, out.Favourites);
    const size_t count = static_cast<size_t>(std::clamp(aRecentCount, 0, kMaxRecent));
    for (uint64_t id : aRecent.Ids())
    {
        if (out.Recent.size() >= count) break;
        const MacroTree::Node* n = aTree.Find(id);
        if (!n || !n->IsMacro() || n->Favourite) continue;
        if (aShow && !aShow(*n)) continue;
        out.Recent.push_back(n);
    }
    return out;
}
