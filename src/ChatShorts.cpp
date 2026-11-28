#include "ChatShorts.h"

#include <algorithm>
#include <cctype>
#include <set>

#include "ChatLine.h"
#include "Exchange.h"
#include "Localization.h"
#include "PathText.h"

using nlohmann::json;
using namespace MacroTree;

namespace
{
    std::string Lower(std::string s)
    {
        for (char& c : s)
            if (static_cast<unsigned char>(c) < 0x80) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::string Trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t");
        if (a == std::string::npos) return {};
        const size_t b = s.find_last_not_of(" \t");
        return s.substr(a, b - a + 1);
    }

    std::string StringOf(const json& aObj, const char* aKey)
    {
        auto it = aObj.find(aKey);
        return it != aObj.end() && it->is_string() ? it->get<std::string>() : std::string();
    }

    // The 24 hex digits of an ObjectId ({"$oid": "..."}), or "".
    std::string OidOf(const json& aValue)
    {
        if (!aValue.is_object()) return {};
        auto it = aValue.find("$oid");
        return it != aValue.end() && it->is_string() ? it->get<std::string>() : std::string();
    }

    // Text from the file: always valid UTF-8 afterwards (the library must save as JSON).
    std::string Clean(std::string aText, bool& aRepaired)
    {
        if (ChatLine::RepairUtf8(aText)) aRepaired = true;
        return aText;
    }

    // Chat Shorts' placeholders that this addon doesn't have. {txt} and {json} are ours too, but
    // removed: the import preview marks those (Placeholders::UsesRemoved).
    const char* const kMissing[] = {"wp", "poi", "area"};
}

namespace ChatShorts
{
    std::filesystem::path DefaultFile(const std::filesystem::path& aDocuments)
    {
        if (aDocuments.empty()) return {};
        return aDocuments / L"Guild Wars 2" / L"addons" / L"blishhud" / L"chat_shorts" / L"macros.db";
    }

    bool IsDbFileName(const std::filesystem::path& aPath)
    {
        const std::string name = Lower(PathText::Utf8(aPath.filename()));
        const std::string ext = ".db";
        const std::string log = "-log.db";
        if (name.size() <= ext.size() || name.compare(name.size() - ext.size(), ext.size(), ext) != 0) return false;
        return !(name.size() > log.size() && name.compare(name.size() - log.size(), log.size(), log) == 0);
    }

    std::filesystem::path LogFileOf(const std::filesystem::path& aDbFile)
    {
        std::filesystem::path p = aDbFile;
        std::filesystem::path stem = aDbFile.stem();
        stem += L"-log";
        stem += aDbFile.extension();
        return p.replace_filename(stem);
    }

    bool ChannelOf(const json& aValue, EChannel& aOut)
    {
        const size_t count = static_cast<size_t>(EChannel::Count);
        if (aValue.is_number_integer())
        {
            const long long v = aValue.get<long long>();
            if (v < 0 || static_cast<size_t>(v) >= count) return false;
            aOut = static_cast<EChannel>(v);
            return true;
        }
        if (!aValue.is_string()) return false;
        // Chat Shorts' names are ours without the case ("Guild3" = "guild3").
        const std::string key = Lower(Trim(aValue.get<std::string>()));
        for (size_t i = 0; i < count; i++)
            if (key == ChannelKey(static_cast<EChannel>(i)))
            {
                aOut = static_cast<EChannel>(i);
                return true;
            }
        return false;
    }

    uint8_t GameModesOf(const json& aValue)
    {
        uint8_t modes = GM_None;
        if (aValue.is_number_integer())
            modes = static_cast<uint8_t>(aValue.get<long long>() & GM_All);
        else if (aValue.is_string())
        {
            const std::string text = aValue.get<std::string>();
            size_t start = 0;
            while (start <= text.size())
            {
                size_t comma = text.find(',', start);
                if (comma == std::string::npos) comma = text.size();
                const std::string part = Lower(Trim(text.substr(start, comma - start)));
                if (part == "pve") modes |= GM_PvE;
                else if (part == "wvw") modes |= GM_WvW;
                else if (part == "pvp") modes |= GM_PvP;
                start = comma + 1;
            }
        }
        else return GM_All; // missing: Chat Shorts' default is every mode
        return modes == GM_None ? static_cast<uint8_t>(GM_All) : modes;
    }

    Hotkey::Combo HotkeyOf(const json& aValue)
    {
        if (!aValue.is_string()) return {};
        const json kb = json::parse(aValue.get<std::string>(), nullptr, false);
        if (!kb.is_object()) return {};
        auto key = kb.find("PrimaryKey");
        if (key == kb.end() || !key->is_number_integer()) return {};
        const long long code = key->get<long long>();
        if (code <= 0 || code > 0xFE) return {};
        Hotkey::Combo c;
        c.Key = static_cast<uint16_t>(code);
        auto mods = kb.find("ModifierKeys");
        if (mods != kb.end() && mods->is_number_integer())
        {
            const long long m = mods->get<long long>();
            c.Ctrl  = (m & 1) != 0;
            c.Alt   = (m & 2) != 0;
            c.Shift = (m & 4) != 0;
        }
        return Hotkey::KeepOnLoad(c) ? c : Hotkey::Combo{};
    }

    std::string FixPlaceholders(const std::string& aText, std::vector<std::string>& aMissing)
    {
        std::string out;
        size_t i = 0;
        while (i < aText.size())
        {
            const size_t open = aText.find('{', i);
            if (open == std::string::npos) break;
            const size_t close = aText.find('}', open + 1);
            if (close == std::string::npos) break;
            const size_t nextOpen = aText.find('{', open + 1);
            if (nextOpen != std::string::npos && nextOpen < close) // "{ {time}": the inner one counts
            {
                out.append(aText, i, nextOpen - i);
                i = nextOpen;
                continue;
            }
            const std::string inner = aText.substr(open + 1, close - open - 1);
            const std::string name = Lower(inner.substr(0, inner.find(' ')));
            out.append(aText, i, open - i);
            if (name == "blish") out += "{nexus}";
            else
            {
                out.append(aText, open, close - open + 1);
                for (const char* m : kMissing)
                    if (name == m && std::find(aMissing.begin(), aMissing.end(), name) == aMissing.end()) aMissing.push_back(name);
            }
            i = close + 1;
        }
        out.append(aText, i, std::string::npos);
        return out;
    }

    size_t Build(const std::map<std::string, std::vector<json>>& aCollections, Tree& aTree, std::vector<std::string>& aWarnings)
    {
        static const std::vector<json> kNone;
        auto find = [&](const char* aName) -> const std::vector<json>& {
            auto it = aCollections.find(aName);
            return it != aCollections.end() ? it->second : kNone;
        };
        const std::vector<json>& macros = find("chat_macros");
        const std::vector<json>& lineDocs = find("chat_lines");

        std::map<std::string, const json*> lines; // by ObjectId
        for (const json& l : lineDocs)
        {
            const std::string id = l.is_object() ? OidOf(l.value("_id", json())) : std::string();
            if (!id.empty()) lines.emplace(id, &l);
        }

        size_t added = 0, links = 0, voice = 0;
        bool repaired = false;
        for (const json& m : macros)
        {
            if (!m.is_object()) continue;
            std::string title = Tree::CleanName(Clean(StringOf(m, "title"), repaired));
            if (title.empty()) title = TR("Imported macro");

            // The lines, in the macro's order: {"$id": {"$oid": ...}, "$ref": "chat_lines"}.
            std::vector<Line> out;
            size_t missing = 0, unknownChannel = 0, overLimit = 0;
            bool longRecipient = false;
            std::vector<std::string> placeholders;
            auto refs = m.find("lines");
            if (refs != m.end() && refs->is_array())
                for (const json& ref : *refs)
                {
                    const std::string id = ref.is_object() ? OidOf(ref.value("$id", json())) : OidOf(ref);
                    auto it = lines.find(id);
                    if (id.empty() || it == lines.end()) { missing++; continue; }
                    const json& l = *it->second;
                    Line line;
                    if (!ChannelOf(l.value("channel", json()), line.Channel)) { unknownChannel++; continue; }
                    if (out.size() >= kMaxLines) { overLimit++; continue; }
                    // As for any file (Tree::FromJson): no control characters to type.
                    line.Message = ChatLine::ControlsToSpaces(FixPlaceholders(Clean(StringOf(l, "message"), repaired), placeholders));
                    if (line.Channel == EChannel::Whisper)
                    {
                        line.WhisperTo = ChatLine::ControlsToSpaces(Clean(StringOf(l, "whisper_to"), repaired));
                        // No longer than a name can be (SEC-035): sending types every character.
                        if (ChatLine::Utf16Length(line.WhisperTo) > ChatLine::kMaxRecipientLength)
                        {
                            line.WhisperTo = ChatLine::CutToUtf16(line.WhisperTo, ChatLine::kMaxRecipientLength);
                            longRecipient = true;
                        }
                    }
                    if (line.Channel == EChannel::Squad)
                    {
                        auto b = l.find("squad_broadcast");
                        line.SquadBroadcast = b != l.end() && b->is_boolean() && b->get<bool>();
                    }
                    out.push_back(std::move(line));
                }
            if (out.empty())
            {
                if (missing || unknownChannel)
                    aWarnings.push_back(Localization::Fmt(TR("\"%s\" was left out: none of its lines could be read (%zu aren't in the "
                                                             "file, %zu are for a channel this addon doesn't know)."),
                                                          title.c_str(), missing, unknownChannel));
                else aWarnings.push_back(Localization::Fmt(TR("\"%s\" has no lines, so it was left out."), title.c_str()));
                continue;
            }
            if (added >= Exchange::kMaxImportItems - 1) // with the folder they go into, at most kMaxImportItems
            {
                aWarnings.push_back(Localization::Fmt(TR("It has more than %zu macros. Only the first %zu were read."),
                                                      Exchange::kMaxImportItems - 1, Exchange::kMaxImportItems - 1));
                break;
            }
            // Chat Shorts allows the same title twice; here a later one becomes "Title (2)".
            Result r = aTree.AddMacro(nullptr, title);
            for (int n = 2; !r.Ok && n <= 1000; n++)
            {
                const std::string suffix = " (" + std::to_string(n) + ")";
                std::string base = title;
                // Shortened a whole character at a time: its continuation bytes, then its first byte.
                while (!base.empty() && base.size() + suffix.size() > kMaxNameLength)
                {
                    while (!base.empty() && (static_cast<unsigned char>(base.back()) & 0xC0) == 0x80) base.pop_back();
                    if (!base.empty()) base.pop_back();
                }
                r = aTree.AddMacro(nullptr, base + suffix);
            }
            if (!r.Ok)
            {
                aWarnings.push_back(Localization::Fmt(TR("\"%s\" was left out: %s"), title.c_str(), r.Error.c_str()));
                continue;
            }
            Node& n = *r.Target;
            n.Lines = std::move(out);
            added++;

            // Where it's active. Chat Shorts ignores the game modes when a macro has maps.
            std::vector<int> maps;
            auto mapIds = m.find("map_ids");
            if (mapIds != m.end() && mapIds->is_array())
                for (const json& v : *mapIds)
                {
                    if (!v.is_number_integer()) continue;
                    const long long id = v.get<long long>();
                    if (id <= 0 || id > 0x7FFFFFFF) continue;
                    if (std::find(maps.begin(), maps.end(), static_cast<int>(id)) != maps.end()) continue;
                    if (maps.size() >= kMaxMapIds)
                    {
                        aWarnings.push_back(Localization::Fmt(TR("Macro \"%s\" has more than %zu maps. Only the first %zu were kept."),
                                                              n.Name.c_str(), kMaxMapIds, kMaxMapIds));
                        break;
                    }
                    maps.push_back(static_cast<int>(id));
                }
            if (!maps.empty())
            {
                n.MapIds = std::move(maps);
                n.GameModes = GM_All;
            }
            else n.GameModes = GameModesOf(m.contains("game_mode") ? m["game_mode"] : json());
            n.Hotkey = HotkeyOf(m.value("key_binding", json()));

            if (!StringOf(m, "link_file").empty()) links++;
            auto vc = m.find("voice_commands");
            if (vc != m.end() && vc->is_array() && !vc->empty()) voice++;
            if (missing)
                aWarnings.push_back(Localization::Fmt(TR("Macro \"%s\": %zu of its lines aren't in the file and were left out."),
                                                      n.Name.c_str(), missing));
            if (unknownChannel)
                aWarnings.push_back(Localization::Fmt(TR("Macro \"%s\": %zu line(s) for a channel this addon doesn't know were left out."),
                                                      n.Name.c_str(), unknownChannel));
            if (longRecipient)
                aWarnings.push_back(Localization::Fmt(TR("\"%s\": a whisper recipient was longer than %zu characters and was shortened."),
                                                      n.Name.c_str(), ChatLine::kMaxRecipientLength));
            if (overLimit)
                aWarnings.push_back(Localization::Fmt(TR("\"%s\" has more than %zu lines. Only the first %zu were kept."),
                                                      n.Name.c_str(), kMaxLines, kMaxLines));
            for (const std::string& p : placeholders)
                aWarnings.push_back(Localization::Fmt(TR("Macro \"%s\" uses {%s}, which this addon doesn't have: it's sent as typed."),
                                                      n.Name.c_str(), p.c_str()));
        }
        aTree.SortChildren(aTree.Root(), false);
        if (links)
            aWarnings.push_back(Localization::Fmt(TR("%zu macro(s) were linked to a text file in Blish HUD. Links aren't imported: "
                                                     "set them again in the editor if you want them."), links));
        if (voice)
            aWarnings.push_back(Localization::Fmt(TR("%zu macro(s) had voice commands, which this addon doesn't have."), voice));
        if (repaired)
            aWarnings.push_back(TR("Some text in the file wasn't valid UTF-8, so some characters were replaced."));
        return added;
    }
}
