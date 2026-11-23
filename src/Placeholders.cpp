#include "Placeholders.h"

#include "ChatLine.h"
#include "Localization.h"

#include <cctype>
#include <cstdio>

#include <nlohmann/json.hpp>

namespace
{
    std::string Lower(std::string s)
    {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::string Two(int v)
    {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02d", v);
        return buf;
    }

    // Splits "random 1 10" / "txt \"my file.txt\" 2" into words. False if a quote isn't closed.
    bool Split(const std::string& aInner, std::vector<std::string>& aOut)
    {
        std::string cur;
        bool inQuote = false, have = false;
        for (char c : aInner)
        {
            if (c == '"') { inQuote = !inQuote; have = true; continue; }
            if (c == ' ' && !inQuote)
            {
                if (have) aOut.push_back(cur);
                cur.clear();
                have = false;
                continue;
            }
            cur += c;
            have = true;
        }
        if (inQuote) return false;
        if (have) aOut.push_back(cur);
        return true;
    }

    // Whole string must be an integer within +-10^15 (enough for any chat use, no overflow).
    bool ParseInt(const std::string& s, long long& aOut)
    {
        size_t i = 0;
        bool neg = false;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) neg = s[i++] == '-';
        if (i == s.size() || s.size() - i > 15) return false;
        long long v = 0;
        for (; i < s.size(); i++)
        {
            if (s[i] < '0' || s[i] > '9') return false;
            v = v * 10 + (s[i] - '0');
        }
        aOut = neg ? -v : v;
        return true;
    }

    const char* const kNames[] = {"time", "hour", "min", "today", "random", "txt", "nexus", "map", "json"};

    bool IsWebLink(const std::string& aUrl)
    {
        const std::string l = Lower(aUrl.substr(0, 8));
        return (l.rfind("https://", 0) == 0 && aUrl.size() > 8) || (l.rfind("http://", 0) == 0 && aUrl.size() > 7);
    }

    bool IsKnown(const std::string& aLowerName)
    {
        for (const char* n : kNames)
            if (aLowerName == n) return true;
        return false;
    }

    // The name of the placeholder starting at aText[aOpen] == '{', or "" if there isn't one.
    // aClose gets the index of the matching '}'. aNextClose carries the first '}' found by the
    // previous call (start it at 0; aOpen only grows): without it, a line full of '{' was scanned
    // to the next '}' once per '{', quadratic (SEC-028).
    std::string NameAt(const std::string& aText, size_t aOpen, size_t& aClose, size_t& aNextClose)
    {
        if (aNextClose != std::string::npos && aNextClose <= aOpen) aNextClose = aText.find('}', aOpen + 1);
        aClose = aNextClose;
        if (aClose == std::string::npos) return {};
        const size_t nextOpen = aText.find('{', aOpen + 1);
        if (nextOpen != std::string::npos && nextOpen < aClose) return {}; // "{ {time}": the inner one counts
        const std::string inner = aText.substr(aOpen + 1, aClose - aOpen - 1);
        const size_t end = inner.find(' ');
        return Lower(inner.substr(0, end));
    }

    // Fills in one known placeholder. False with aError set if it can't.
    bool Resolve(const std::string& aName, const std::vector<std::string>& aArgs, const Placeholders::Context& c,
                 std::string& aOut, std::string& aError)
    {
        const std::string tag = "{" + aName + "}: ";
        if (aName == "time") { aOut = Two(c.Hour) + ":" + Two(c.Minute); return true; }
        if (aName == "hour") { aOut = Two(c.Hour); return true; }
        if (aName == "min")  { aOut = Two(c.Minute); return true; }
        if (aName == "today")
        {
            aOut = c.Weekday + ", " + std::to_string(c.Day) + "." + std::to_string(c.Month) + "." + std::to_string(c.Year);
            return true;
        }
        if (aName == "nexus") { aOut = c.AddonVersion; return true; }
        if (aName == "random")
        {
            long long lo = 0, hi = 100;
            if (aArgs.size() == 1)
            {
                if (!ParseInt(aArgs[0], hi)) { aError = tag + "\"" + aArgs[0] + "\" isn't a whole number."; return false; }
            }
            else if (aArgs.size() == 2)
            {
                if (!ParseInt(aArgs[0], lo)) { aError = tag + "\"" + aArgs[0] + "\" isn't a whole number."; return false; }
                if (!ParseInt(aArgs[1], hi)) { aError = tag + "\"" + aArgs[1] + "\" isn't a whole number."; return false; }
            }
            else if (aArgs.size() > 2) { aError = tag + "use {random}, {random max} or {random min max}."; return false; }
            if (lo > hi) std::swap(lo, hi);
            if (!c.Random) { aError = tag + "random numbers aren't available."; return false; }
            aOut = std::to_string(c.Random(lo, hi));
            return true;
        }
        if (aName == "txt")
        {
            if (aArgs.empty() || aArgs.size() > 2) { aError = tag + "use {txt file} or {txt file line}."; return false; }
            if (!Placeholders::IsSafeRelativePath(aArgs[0]))
            {
                aError = tag + "\"" + aArgs[0] + "\" must be a file inside the addon folder (like notes.txt or lists\\names.txt).";
                return false;
            }
            long long line = 0;
            if (aArgs.size() == 2 && (!ParseInt(aArgs[1], line) || line < 1))
            {
                aError = tag + "the line number must be 1 or more.";
                return false;
            }
            std::vector<std::string> lines;
            std::string why;
            if (!c.ReadLines || !c.ReadLines(aArgs[0], lines, why)) { aError = tag + (why.empty() ? "couldn't read the file." : why); return false; }
            // Blank lines don't count: a random pick should never send an empty message.
            std::vector<std::string> used;
            for (const std::string& l : lines)
                if (l.find_first_not_of(" \t") != std::string::npos) used.push_back(l);
            if (used.empty()) { aError = tag + "\"" + aArgs[0] + "\" has no text in it."; return false; }
            if (line > static_cast<long long>(used.size()))
            {
                aError = tag + "\"" + aArgs[0] + "\" has only " + std::to_string(used.size()) + " line(s).";
                return false;
            }
            if (line == 0)
            {
                if (!c.Random) { aError = tag + "random numbers aren't available."; return false; }
                line = c.Random(1, static_cast<long long>(used.size()));
            }
            aOut = used[static_cast<size_t>(line - 1)];
            return true;
        }
        if (aName == "map")
        {
            if (!aArgs.empty()) { aError = tag + "it doesn't take anything after the name."; return false; }
            std::string why;
            if (!c.MapName || !c.MapName(aOut, why)) { aError = tag + (why.empty() ? "the map name isn't known." : why); return false; }
            return true;
        }
        if (aName == "json")
        {
            if (aArgs.size() != 2) { aError = tag + "use {json path url}, e.g. {json name https://api.guildwars2.com/v2/build}."; return false; }
            if (!IsWebLink(aArgs[1])) { aError = tag + "the address must start with https:// or http://."; return false; }
            std::string body, why;
            if (!c.FetchUrl || !c.FetchUrl(aArgs[1], body, why)) { aError = tag + (why.empty() ? "the download failed." : why); return false; }
            if (!Placeholders::JsonValue(body, aArgs[0], aOut, why)) { aError = tag + why; return false; }
            return true;
        }
        aError = tag + "unknown placeholder.";
        return false;
    }
}

Placeholders::Result Placeholders::Expand(const std::string& aText, const Context& aContext)
{
    Result r;
    // Written text and filled-in values, with FromValue kept the same length as Text.
    auto written = [&r](const std::string& s) { r.Text += s; r.FromValue.insert(r.FromValue.end(), s.size(), false); };
    auto filled  = [&r](const std::string& s) { r.Text += s; r.FromValue.insert(r.FromValue.end(), s.size(), true); };
    auto fail    = [&r]() { r.Text.clear(); r.FromValue.clear(); };
    size_t i = 0, nextClose = 0;
    while (i < aText.size())
    {
        size_t open = aText.find('{', i);
        if (open == std::string::npos)
        {
            written(aText.substr(i));
            break;
        }
        written(aText.substr(i, open - i));
        // In a run of "{" only the last one can start a placeholder ("{ {time}": the inner one
        // counts), so the others are written text, copied in one go.
        const size_t notBrace = aText.find_first_not_of('{', open);
        const size_t last = (notBrace == std::string::npos ? aText.size() : notBrace) - 1;
        if (last > open)
        {
            written(aText.substr(open, last - open));
            open = last;
        }
        size_t close = 0;
        const std::string name = NameAt(aText, open, close, nextClose);
        if (name.empty() || !IsKnown(name))
        {
            written("{"); // not ours: keep it as typed
            i = open + 1;
            continue;
        }
        std::vector<std::string> words;
        if (!Split(aText.substr(open + 1, close - open - 1), words))
        {
            r.Error = "{" + name + "}: a quote isn't closed.";
            fail();
            return r;
        }
        std::string value;
        if (!Resolve(name, std::vector<std::string>(words.begin() + 1, words.end()), aContext, value, r.Error))
        {
            fail();
            return r;
        }
        // Every value, whatever its source (a file, the map name, a web page, Windows' weekday name),
        // is one line of plain text: its control characters become spaces, so it can't press Enter,
        // Tab, Backspace or Escape (SEC-013). Never re-scanned: a file line with "{time}" stays as written.
        filled(ChatLine::ControlsToSpaces(value));
        i = close + 1;
    }
    return r;
}

namespace
{
    // Whether aMatch(name) holds for a placeholder name in aText, as Expand finds them (NameAt: a
    // "{" whose first "}" comes before any other "{"; the name is its first word, lower case). One
    // pass, without building any text: the import preview and the editor run this every frame
    // (SEC-028).
    template <class Match>
    bool AnyName(const std::string& aText, Match aMatch)
    {
        size_t open = std::string::npos; // the last "{" not yet closed
        for (size_t i = 0; i < aText.size(); i++)
        {
            if (aText[i] == '{') open = i;
            else if (aText[i] == '}' && open != std::string::npos)
            {
                size_t end = open + 1;
                while (end < i && aText[end] != ' ') end++;
                if (aMatch(Lower(aText.substr(open + 1, end - open - 1)))) return true;
                open = std::string::npos;
            }
        }
        return false;
    }
}

bool Placeholders::HasPlaceholders(const std::string& aText)
{
    return AnyName(aText, [](const std::string& aName) { return !aName.empty() && IsKnown(aName); });
}

bool Placeholders::Uses(const std::string& aText, const std::string& aName)
{
    return AnyName(aText, [&aName](const std::string& aFound) { return aFound == aName; });
}

bool Placeholders::JsonValue(const std::string& aJson, const std::string& aPath, std::string& aOut, std::string& aError)
{
    const nlohmann::json doc = nlohmann::json::parse(aJson, nullptr, false);
    if (doc.is_discarded()) { aError = "the page isn't valid JSON."; return false; }
    if (aPath.empty()) { aError = "the path is empty."; return false; }

    const nlohmann::json* cur = &doc;
    size_t start = 0;
    while (start <= aPath.size())
    {
        size_t end = aPath.find('.', start);
        if (end == std::string::npos) end = aPath.size();
        const std::string seg = aPath.substr(start, end - start);
        long long index = 0;
        if (cur->is_array() && ParseInt(seg, index))
        {
            if (index < 0 || index >= static_cast<long long>(cur->size()))
            {
                aError = "\"" + seg + "\" is past the end of the list.";
                return false;
            }
            cur = &(*cur)[static_cast<size_t>(index)];
        }
        else if (cur->is_object() && cur->contains(seg))
        {
            cur = &(*cur)[seg];
        }
        else
        {
            aError = "\"" + aPath + "\" wasn't found in the page.";
            return false;
        }
        start = end + 1;
    }

    if (cur->is_string()) aOut = cur->get<std::string>();
    else if (cur->is_boolean()) aOut = cur->get<bool>() ? "true" : "false";
    else if (cur->is_number_integer()) aOut = std::to_string(cur->get<long long>());
    else if (cur->is_number_unsigned()) aOut = std::to_string(cur->get<unsigned long long>());
    else if (cur->is_number_float()) aOut = cur->dump();
    else
    {
        aError = "\"" + aPath + "\" isn't a single value (it's a list, an object or empty).";
        return false;
    }
    return true;
}

bool Placeholders::IsSafeRelativePath(const std::string& aPath)
{
    if (aPath.empty() || aPath[0] == '/' || aPath[0] == '\\') return false;
    if (aPath.find(':') != std::string::npos) return false; // drive letters, alternate streams
    size_t start = 0;
    while (start <= aPath.size())
    {
        size_t end = aPath.find_first_of("/\\", start);
        if (end == std::string::npos) end = aPath.size();
        const std::string part = aPath.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") return false;
        start = end + 1;
    }
    return true;
}

const std::vector<Placeholders::Info>& Placeholders::List()
{
    static const std::vector<Info> kList = {
        {"{time}",                 TX("the time now, like 21:05")},
        {"{hour}",                 TX("the hour, like 21")},
        {"{min}",                  TX("the minutes, like 05")},
        {"{today}",                TX("the date, like Friday, 20.11.2026")},
        {"{random}",               TX("a random number from 0 to 100")},
        {"{random max}",           TX("a random number from 0 to max")},
        {"{random min max}",       TX("a random number from min to max")},
        {"{txt file}",             TX("a random line from a text file in the addon folder")},
        {"{txt file line}",        TX("that line of the file (1 = first)")},
        {"{map}",                  TX("the name of the map you're on")},
        {"{json path url}",        TX("one value from a JSON page, e.g. {json id https://...}")},
        {"{nexus}",                TX("this addon's name and version")},
    };
    return kList;
}
