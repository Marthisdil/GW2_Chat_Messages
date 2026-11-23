#include "Placeholders.h"

#include "ChatLine.h"
#include "Localization.h"

#include <cctype>
#include <cstdio>

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

    // Splits "random 1 10" into words.
    std::vector<std::string> Split(const std::string& aInner)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char c : aInner)
        {
            if (c == ' ')
            {
                if (!cur.empty()) out.push_back(cur);
                cur.clear();
                continue;
            }
            cur += c;
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
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

    const char* const kNames[] = {"time", "hour", "min", "today", "random", "nexus", "map"};
    // Removed 2026-11-23 (owner): a line that still uses one is refused, never sent as typed.
    const char* const kRemoved[] = {"txt", "json"};

    bool IsKnown(const std::string& aLowerName)
    {
        for (const char* n : kNames)
            if (aLowerName == n) return true;
        return false;
    }

    bool IsRemoved(const std::string& aLowerName)
    {
        for (const char* n : kRemoved)
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
                if (!ParseInt(aArgs[0], hi)) { aError = tag + Localization::Fmt(TR("\"%s\" isn't a whole number."), aArgs[0].c_str()); return false; }
            }
            else if (aArgs.size() == 2)
            {
                if (!ParseInt(aArgs[0], lo)) { aError = tag + Localization::Fmt(TR("\"%s\" isn't a whole number."), aArgs[0].c_str()); return false; }
                if (!ParseInt(aArgs[1], hi)) { aError = tag + Localization::Fmt(TR("\"%s\" isn't a whole number."), aArgs[1].c_str()); return false; }
            }
            else if (aArgs.size() > 2) { aError = tag + TR("use {random}, {random max} or {random min max}."); return false; }
            if (lo > hi) std::swap(lo, hi);
            if (!c.Random) { aError = tag + TR("random numbers aren't available."); return false; }
            aOut = std::to_string(c.Random(lo, hi));
            return true;
        }
        if (aName == "map")
        {
            if (!aArgs.empty()) { aError = tag + TR("it doesn't take anything after the name."); return false; }
            std::string why;
            if (!c.MapName || !c.MapName(aOut, why)) { aError = tag + (why.empty() ? std::string(TR("the map name isn't known.")) : why); return false; }
            return true;
        }
        aError = tag + TR("unknown placeholder.");
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
        if (IsRemoved(name))
        {
            r.Error = Localization::Fmt(TR("{%s} isn't available any more: edit this line to remove it."), name.c_str());
            fail();
            return r;
        }
        if (name.empty() || !IsKnown(name))
        {
            written("{"); // not ours: keep it as typed
            i = open + 1;
            continue;
        }
        const std::vector<std::string> words = Split(aText.substr(open + 1, close - open - 1));
        std::string value;
        if (!Resolve(name, std::vector<std::string>(words.begin() + 1, words.end()), aContext, value, r.Error))
        {
            fail();
            return r;
        }
        // Every value, whatever its source (the map name, Windows' weekday name), is one line of
        // plain text: its control characters become spaces, so it can't press Enter, Tab, Backspace
        // or Escape (SEC-013). Never re-scanned: a value with "{time}" in it stays as it is.
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

bool Placeholders::UsesRemoved(const std::string& aText)
{
    return AnyName(aText, [](const std::string& aName) { return IsRemoved(aName); });
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
        {"{map}",                  TX("the name of the map you're on")},
        {"{nexus}",                TX("this addon's name and version")},
    };
    return kList;
}
