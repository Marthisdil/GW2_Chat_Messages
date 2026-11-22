#include "Search.h"

namespace
{
    // U+00C0..U+00FF, folded. nullptr = keep as is (× and ÷).
    const char* const kLatin1[64] = {
        "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
        "d", "n", "o", "o", "o", "o", "o", nullptr, "o", "u", "u", "u", "u", "y", "th", "ss",
        "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
        "d", "n", "o", "o", "o", "o", "o", nullptr, "o", "u", "u", "u", "u", "y", "th", "y",
    };

    void Put(std::string& aOut, std::vector<size_t>* aMap, const char* aText, size_t aFrom)
    {
        for (const char* p = aText; *p; ++p)
        {
            aOut += *p;
            if (aMap) aMap->push_back(aFrom);
        }
    }

    bool Contains(const std::string& aText, const std::string& aFoldedQuery)
    {
        return !aText.empty() && Search::Fold(aText).find(aFoldedQuery) != std::string::npos;
    }

    struct Walked
    {
        bool Shown  = false; // something at or below is shown
        bool Reason = false; // ...because it matches the text, or (filtering) is an active macro:
                             // the folders above it are opened
    };

    // aInMatch: an ancestor folder matched the text, so its contents are shown too.
    Walked Walk(const MacroTree::Node& aNode, const std::string& aQuery, const Search::Where& aWhere,
                bool aInMatch, Search::Result& aOut)
    {
        Walked below;
        for (const auto& c : aNode.Children)
        {
            const bool self = !aQuery.empty() && Search::NodeMatches(*c, aQuery);
            if (c->IsMacro())
            {
                const bool byText = aQuery.empty() || self || aInMatch;
                const bool here   = !aWhere.ActiveOnly || MacroTree::IsActive(*c, aWhere.Mode, aWhere.MapId);
                if (!byText || !here) continue;
                aOut.Visible.insert(c->Id);
                aOut.ShownMacros++;
                if (self)
                {
                    aOut.Matches.insert(c->Id);
                    aOut.MacroMatches++;
                }
                if (aInMatch && !self) aOut.Inside.insert(c->Id);
                below.Shown = true;
                // A macro shown only because its folder matched doesn't open folders; a filtered one does.
                if (self || aWhere.ActiveOnly) below.Reason = true;
                continue;
            }
            const Walked inner = Walk(*c, aQuery, aWhere, aInMatch || self, aOut);
            // Without the filter a matched folder shows everything inside it, even empty folders;
            // with it, a folder only shows when an active macro is below it.
            const bool shown = inner.Shown || (!aWhere.ActiveOnly && (self || aInMatch));
            if (self)
            {
                aOut.Matches.insert(c->Id);
                aOut.FolderMatches++;
            }
            if (!shown) continue;
            aOut.Visible.insert(c->Id);
            if (aInMatch && !self) aOut.Inside.insert(c->Id);
            if (inner.Reason) aOut.Open.insert(c->Id);
            below.Shown = true;
            if (self || inner.Reason) below.Reason = true;
        }
        return below;
    }
}

std::string Search::Trim(const std::string& aText)
{
    size_t a = 0, b = aText.size();
    while (a < b && (aText[a] == ' ' || aText[a] == '\t')) a++;
    while (b > a && (aText[b - 1] == ' ' || aText[b - 1] == '\t')) b--;
    return aText.substr(a, b - a);
}

std::string Search::Fold(const std::string& aText, std::vector<size_t>* aMap)
{
    std::string out;
    out.reserve(aText.size());
    if (aMap)
    {
        aMap->clear();
        aMap->reserve(aText.size());
    }
    for (size_t i = 0; i < aText.size(); i++)
    {
        const unsigned char c = static_cast<unsigned char>(aText[i]);
        if (c < 0x80)
        {
            const char low = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
            out += low;
            if (aMap) aMap->push_back(i);
            continue;
        }
        if (i + 1 < aText.size())
        {
            const unsigned char d = static_cast<unsigned char>(aText[i + 1]);
            const char* folded = nullptr;
            if (c == 0xC3 && d >= 0x80 && d <= 0xBF) folded = kLatin1[d - 0x80];
            else if (c == 0xC5 && (d == 0x92 || d == 0x93)) folded = "oe"; // Œ œ
            if (folded)
            {
                Put(out, aMap, folded, i);
                i++;
                continue;
            }
        }
        out += static_cast<char>(c); // other UTF-8 bytes are kept as they are
        if (aMap) aMap->push_back(i);
    }
    return out;
}

bool Search::FindRange(const std::string& aText, const std::string& aQuery, size_t& aStart, size_t& aLen)
{
    const std::string q = Fold(Trim(aQuery));
    if (q.empty()) return false;
    std::vector<size_t> map;
    const std::string f = Fold(aText, &map);
    const size_t at = f.find(q);
    if (at == std::string::npos) return false;
    aStart = map[at];
    // End after the whole source character of the last matched byte (a match may end inside the
    // "ss" of ß or the "ae" of æ: that character is highlighted whole).
    size_t end = map[at + q.size() - 1] + 1;
    while (end < aText.size() && (static_cast<unsigned char>(aText[end]) & 0xC0) == 0x80) end++;
    aLen = end - aStart;
    return true;
}

bool Search::NodeMatches(const MacroTree::Node& aNode, const std::string& aFoldedQuery)
{
    if (aFoldedQuery.empty()) return false;
    if (Contains(aNode.Name, aFoldedQuery)) return true;
    if (!aNode.IsMacro()) return false;
    for (const MacroTree::Line& l : aNode.Lines)
        if (Contains(l.Message, aFoldedQuery) || Contains(l.WhisperTo, aFoldedQuery)) return true;
    return !aNode.Hotkey.Empty() && Contains(Hotkey::ToString(aNode.Hotkey), aFoldedQuery);
}

void Search::Run(const MacroTree::Node& aRoot, const std::string& aQuery, const Where& aWhere, Result& aOut)
{
    aOut.Active = false;
    aOut.Matches.clear();
    aOut.Visible.clear();
    aOut.Open.clear();
    aOut.Inside.clear();
    aOut.FolderMatches = 0;
    aOut.MacroMatches  = 0;
    aOut.ShownMacros   = 0;
    const std::string q = Fold(Trim(aQuery));
    if (q.empty() && !aWhere.ActiveOnly) return;
    aOut.Active = true;
    Walk(aRoot, q, aWhere, false, aOut);
}

void Search::Run(const MacroTree::Node& aRoot, const std::string& aQuery, Result& aOut)
{
    Run(aRoot, aQuery, Where{}, aOut);
}
