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

    // Marks every node inside aFolder.
    void MarkInside(const MacroTree::Node& aFolder, Search::Result& aOut)
    {
        for (const auto& c : aFolder.Children)
        {
            aOut.Inside.insert(c->Id);
            if (c->IsFolder()) MarkInside(*c, aOut);
        }
    }

    // Returns whether anything at or below aNode matches.
    bool Walk(const MacroTree::Node& aNode, const std::string& aQuery, Search::Result& aOut)
    {
        bool below = false;
        for (const auto& c : aNode.Children)
        {
            const bool self = Search::NodeMatches(*c, aQuery);
            if (self)
            {
                aOut.Matches.insert(c->Id);
                if (c->IsFolder())
                {
                    aOut.FolderMatches++;
                    MarkInside(*c, aOut);
                }
                else aOut.MacroMatches++;
            }
            bool inner = false;
            if (c->IsFolder()) inner = Walk(*c, aQuery, aOut);
            if (inner) aOut.Open.insert(c->Id);
            below = below || self || inner;
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

Search::Result Search::Run(const MacroTree::Node& aRoot, const std::string& aQuery)
{
    Result r;
    const std::string q = Fold(Trim(aQuery));
    if (q.empty()) return r;
    r.Active = true;
    Walk(aRoot, q, r);
    return r;
}
