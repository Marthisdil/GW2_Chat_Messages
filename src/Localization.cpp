#include "Localization.h"

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>

#include <nlohmann/json.hpp>

namespace
{
    using nlohmann::json;
    using Texts = std::unordered_map<std::string, std::string>;

    const Localization::Catalog*                 s_Active = nullptr;
    std::unordered_map<const char*, std::string> s_Cache; // literal address -> text (nodes never move)

    // For TR (any thread): a copy of each language's texts that SetActive has used. Copies are
    // never freed while the addon runs (a thread may still hold a text from one), only by
    // ClearShared; switching back to a language reuses its copy.
    std::mutex                               s_SharedMutex;
    std::vector<std::unique_ptr<const Texts>> s_Shared;
    std::atomic<const Texts*>                s_SharedActive{nullptr};

    // Decodes one UTF-8 character at aText[i]. Returns its length, or 0 if it's invalid.
    size_t DecodeUtf8(const std::string& aText, size_t i, uint32_t& aCp)
    {
        const auto byte = [&](size_t k) { return static_cast<unsigned char>(aText[k]); };
        const unsigned char c = byte(i);
        size_t len = 0;
        uint32_t min = 0;
        if (c < 0x80) { aCp = c; return 1; }
        if ((c & 0xE0) == 0xC0) { len = 2; aCp = c & 0x1F; min = 0x80; }
        else if ((c & 0xF0) == 0xE0) { len = 3; aCp = c & 0x0F; min = 0x800; }
        else if ((c & 0xF8) == 0xF0) { len = 4; aCp = c & 0x07; min = 0x10000; }
        else return 0;
        if (i + len > aText.size()) return 0;
        for (size_t k = 1; k < len; k++)
        {
            if ((byte(i + k) & 0xC0) != 0x80) return 0;
            aCp = (aCp << 6) | (byte(i + k) & 0x3F);
        }
        if (aCp < min || aCp > 0x10FFFF || (aCp >= 0xD800 && aCp <= 0xDFFF)) return 0; // overlong, too big, surrogate
        return len;
    }

    // Marks that change text direction or hide characters: they could make a label read
    // differently from what it is.
    bool IsDirectionMark(uint32_t aCp)
    {
        return aCp == 0x061C || aCp == 0x200E || aCp == 0x200F || (aCp >= 0x202A && aCp <= 0x202E) ||
               (aCp >= 0x2066 && aCp <= 0x2069) || aCp == 0x200B || aCp == 0xFEFF;
    }

    // Deepest nesting of [ and { outside strings, so a hostile file can't nest thousands deep.
    int NestingDepth(const std::string& aText)
    {
        int depth = 0, deepest = 0;
        bool inString = false, escaped = false;
        for (const char c : aText)
        {
            if (inString)
            {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') inString = false;
                continue;
            }
            if (c == '"') inString = true;
            else if (c == '[' || c == '{') { if (++depth > deepest) deepest = depth; }
            else if (c == ']' || c == '}') depth--;
        }
        return deepest;
    }

    // Shortens a key for a log note.
    std::string Short(const std::string& aText)
    {
        if (aText.size() <= 40) return aText;
        size_t cut = 40;
        while (cut > 0 && (static_cast<unsigned char>(aText[cut]) & 0xC0) == 0x80) cut--; // whole characters
        return aText.substr(0, cut) + "...";
    }

    void Skip(Localization::Catalog& aCat, const std::string& aKey, const char* aWhy)
    {
        aCat.Skipped++;
        if (aCat.SkipNotes.size() < Localization::kMaxSkipNotes) aCat.SkipNotes.push_back("\"" + Short(aKey) + "\": " + aWhy);
    }
}

bool Localization::ValidId(const std::string& aId)
{
    size_t i = 0;
    while (i < aId.size() && aId[i] >= 'a' && aId[i] <= 'z') i++;
    if (i < 2 || i > 3) return false;
    if (i == aId.size()) return true;
    if (aId[i] != '-') return false;
    const size_t rest = aId.size() - i - 1;
    if (rest < 2 || rest > 8) return false;
    for (size_t k = i + 1; k < aId.size(); k++)
    {
        const char c = aId[k];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
    }
    return true;
}

bool Localization::SafeText(const std::string& aText, bool aAllowNewline, std::string* aWhy)
{
    const auto fail = [&](const char* aReason) { if (aWhy) *aWhy = aReason; return false; };
    if (aText.find("##") != std::string::npos) return fail("contains \"##\"");
    for (size_t i = 0; i < aText.size();)
    {
        uint32_t cp = 0;
        const size_t len = DecodeUtf8(aText, i, cp);
        if (len == 0) return fail("not valid UTF-8");
        if (cp == '\n' && aAllowNewline) { i += len; continue; }
        if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F)) return fail("contains a control character");
        if (IsDirectionMark(cp)) return fail("contains a text-direction or invisible mark");
        i += len;
    }
    return true;
}

bool Localization::Placeholders(const std::string& aText, std::vector<std::string>& aOut)
{
    aOut.clear();
    for (size_t i = 0; i < aText.size(); i++)
    {
        if (aText[i] != '%') continue;
        const size_t start = i++;
        if (i < aText.size() && aText[i] == '%') continue; // "%%" is a plain percent sign
        while (i < aText.size() && std::string("-+ #0").find(aText[i]) != std::string::npos) i++;
        size_t digits = 0;
        while (i < aText.size() && aText[i] >= '0' && aText[i] <= '9') { i++; digits++; }
        if (digits > 2) return false;
        if (i < aText.size() && aText[i] == '.')
        {
            i++;
            digits = 0;
            while (i < aText.size() && aText[i] >= '0' && aText[i] <= '9') { i++; digits++; }
            if (digits == 0 || digits > 2) return false;
        }
        // Length: hh h l ll z (I64 and the rest aren't used).
        if (i < aText.size() && (aText[i] == 'h' || aText[i] == 'l'))
        {
            const char l = aText[i++];
            if (i < aText.size() && aText[i] == l) i++;
        }
        else if (i < aText.size() && aText[i] == 'z') i++;
        if (i >= aText.size()) return false;
        if (std::string("sdiuxXfgc").find(aText[i]) == std::string::npos) return false; // never %n, %p, %*, %1$
        aOut.push_back(aText.substr(start, i - start + 1));
    }
    return true;
}

bool Localization::SamePlaceholders(const std::string& aEnglish, const std::string& aTranslation)
{
    std::vector<std::string> a, b;
    return Placeholders(aEnglish, a) && Placeholders(aTranslation, b) && a == b;
}

bool Localization::OwnFilesFromSettings(const json& aSettings)
{
    if (!aSettings.is_object()) return false;
    const auto it = aSettings.find(kOwnFilesSetting);
    return it != aSettings.end() && it->is_boolean() && it->get<bool>();
}

bool Localization::ParseCatalog(const std::string& aJson, const std::unordered_set<std::string>* aKnown,
                                Catalog& aOut, std::string& aError)
{
    aOut = Catalog{};
    if (aJson.size() > kMaxFileBytes) { aError = TX("The file is too big (over 512 KB)."); return false; }
    std::string text = aJson;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3); // a BOM from Notepad
    if (NestingDepth(text) > kMaxDepth) { aError = TX("The file is nested too deeply to be a language file."); return false; }
    const json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded()) { aError = TX("The file isn't valid JSON."); return false; }
    if (!doc.is_object()) { aError = TX("The file isn't a language file (no { ... } at the top)."); return false; }

    const auto id = doc.find("Identifier");
    if (id == doc.end() || !id->is_string() || !ValidId(id->get<std::string>()))
    {
        aError = TX("\"Identifier\" is missing or isn't a language code like \"de\" or \"pt-br\".");
        return false;
    }
    aOut.Id = id->get<std::string>();

    // "Format": a number; anything else (or none) is format 1. Huge or odd values are clamped, so
    // a newer file never reads as an older one.
    const auto format = doc.find("Format");
    if (format != doc.end() && format->is_number())
    {
        const double f = format->get<double>();
        if (f >= 1000000.0) aOut.Format = 1000000;
        else if (f >= 1.0) aOut.Format = static_cast<int>(f);
    }
    aOut.FromNewer = aOut.Format > kFormat;

    const auto name = doc.find("DisplayName");
    std::string why;
    if (name == doc.end() || !name->is_string() || name->get<std::string>().empty() ||
        name->get<std::string>().size() > kMaxNameBytes || !SafeText(name->get<std::string>(), false, &why))
    {
        aError = TX("\"DisplayName\" is missing, too long, or has characters that aren't allowed.");
        return false;
    }
    aOut.DisplayName = name->get<std::string>();

    const auto ver = doc.find("AddonVersion");
    if (ver != doc.end() && ver->is_string() && ver->get<std::string>().size() <= kMaxNameBytes &&
        SafeText(ver->get<std::string>(), false))
        aOut.AddonVersion = ver->get<std::string>();

    const auto texts = doc.find("Texts");
    if (texts == doc.end() || !texts->is_object()) { aError = TX("\"Texts\" is missing or isn't a { ... } list."); return false; }
    if (texts->size() > kMaxTexts) { aError = TX("\"Texts\" has too many entries."); return false; }

    for (auto it = texts->begin(); it != texts->end(); ++it)
    {
        const std::string& key = it.key();
        if (key.empty() || key.size() > kMaxTextBytes || !SafeText(key, true)) { Skip(aOut, "(an entry)", "its English text isn't valid"); continue; }
        if (aKnown && !aKnown->count(key)) { aOut.Unused++; continue; } // reworded, removed, or newer: English is used
        if (!it.value().is_string())
        {
            // A newer format may store a text differently: English is used, quietly.
            if (aOut.FromNewer) aOut.Unused++;
            else Skip(aOut, key, "the translation isn't text");
            continue;
        }
        const std::string value = it.value().get<std::string>();
        if (value.empty()) continue; // not translated yet: English is used, not counted as a problem
        if (value.size() > kMaxTextBytes) { Skip(aOut, key, "the translation is too long"); continue; }
        if (!SafeText(value, true, &why)) { Skip(aOut, key, ("the translation " + why).c_str()); continue; }
        if (!SamePlaceholders(key, value)) { Skip(aOut, key, "its %-codes don't match the English"); continue; }
        aOut.Texts[key] = value;
    }
    return true;
}

int Localization::PercentDone(const Catalog& aCatalog, const std::unordered_set<std::string>& aKnown)
{
    if (aKnown.empty()) return 100;
    size_t done = 0;
    for (const auto& k : aKnown) done += aCatalog.Texts.count(k);
    if (done >= aKnown.size()) return 100;
    return static_cast<int>(done * 100 / aKnown.size()); // < 100 while anything is missing
}

void Localization::SplitId(const std::string& aText, std::string& aVisible, std::string& aSuffix)
{
    const size_t at = aText.find("##");
    if (at == std::string::npos) { aVisible = aText; aSuffix.clear(); return; }
    aVisible = aText.substr(0, at);
    aSuffix  = aText.substr(at);
}

void Localization::SetActive(const Catalog* aCatalog)
{
    s_Active = aCatalog;
    s_Cache.clear();

    const Texts* shared = nullptr;
    if (aCatalog)
    {
        std::lock_guard<std::mutex> lock(s_SharedMutex);
        for (const auto& t : s_Shared)
            if (*t == aCatalog->Texts) shared = t.get();
        if (!shared)
        {
            s_Shared.push_back(std::make_unique<const Texts>(aCatalog->Texts));
            shared = s_Shared.back().get();
        }
    }
    s_SharedActive.store(shared, std::memory_order_release);
}

const char* Localization::Translate(const char* aEnglish)
{
    if (!aEnglish) return "";
    const Texts* texts = s_SharedActive.load(std::memory_order_acquire);
    if (!texts) return aEnglish;
    const auto it = texts->find(aEnglish);
    return it != texts->end() ? it->second.c_str() : aEnglish;
}

void Localization::ClearShared()
{
    s_SharedActive.store(nullptr, std::memory_order_release);
    std::lock_guard<std::mutex> lock(s_SharedMutex);
    s_Shared.clear();
}

const Localization::Catalog* Localization::Active() { return s_Active; }

std::string Localization::LookupText(const std::string& aEnglish)
{
    std::string visible, suffix;
    SplitId(aEnglish, visible, suffix);
    if (s_Active && !visible.empty())
    {
        const auto it = s_Active->Texts.find(visible);
        if (it != s_Active->Texts.end()) return it->second + suffix;
    }
    return aEnglish;
}

const char* Localization::Lookup(const char* aEnglishLiteral)
{
    if (!aEnglishLiteral) return "";
    if (!s_Active) return aEnglishLiteral; // English: nothing to look up
    const auto hit = s_Cache.find(aEnglishLiteral);
    if (hit != s_Cache.end()) return hit->second.c_str();
    return s_Cache.emplace(aEnglishLiteral, LookupText(aEnglishLiteral)).first->second.c_str();
}

std::string Localization::Fmt(const char* aFormat, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, aFormat);
    const int n = std::vsnprintf(buf, sizeof(buf), aFormat, args);
    va_end(args);
    if (n < 0) return std::string();
    if (static_cast<size_t>(n) < sizeof(buf)) return std::string(buf, static_cast<size_t>(n));
    std::string big(static_cast<size_t>(n) + 1, '\0');
    va_start(args, aFormat);
    std::vsnprintf(&big[0], big.size(), aFormat, args);
    va_end(args);
    big.resize(static_cast<size_t>(n));
    return big;
}
