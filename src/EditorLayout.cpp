#include "EditorLayout.h"

namespace
{
    void ReadBool(const nlohmann::json& aObj, const char* aKey, bool& aOut)
    {
        const auto it = aObj.find(aKey);
        if (it != aObj.end() && it->is_boolean()) aOut = it->get<bool>();
    }
}

EditorLayout::Sections EditorLayout::FromJson(const nlohmann::json& aValue)
{
    Sections s;
    if (!aValue.is_object()) return s;
    ReadBool(aValue, "where", s.Where);
    ReadBool(aValue, "hotkey", s.Hotkey);
    ReadBool(aValue, "note", s.Note);
    ReadBool(aValue, "advanced", s.Advanced);
    return s;
}

nlohmann::json EditorLayout::ToJson(const Sections& aSections)
{
    return {{"where", aSections.Where}, {"hotkey", aSections.Hotkey}, {"note", aSections.Note},
            {"advanced", aSections.Advanced}};
}

std::string EditorLayout::ShortNote(const std::string& aNote, size_t aMaxBytes)
{
    const size_t nl = aNote.find('\n');
    std::string first = aNote.substr(0, nl);
    bool more = nl != std::string::npos;
    if (first.size() > aMaxBytes)
    {
        size_t cut = aMaxBytes;
        while (cut > 0 && (static_cast<unsigned char>(first[cut]) & 0xC0) == 0x80) cut--; // not inside a character
        first.resize(cut);
        more = true;
    }
    while (!first.empty() && first.back() == ' ') first.pop_back();
    if (more) first += "...";
    return first;
}
