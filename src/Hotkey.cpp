#include "Hotkey.h"

#include <cctype>
#include <cstdio>
#include <vector>

namespace
{
    struct Named { uint16_t Key; const char* Name; };

    // Names for keys that aren't letters, digits or F-keys. Stable: they're saved in macros.json.
    const Named kNames[] = {
        {Hotkey::kVkMButton, "Mouse 3"}, {Hotkey::kVkXButton1, "Mouse 4"}, {Hotkey::kVkXButton2, "Mouse 5"},
        {0x08, "Backspace"}, {0x09, "Tab"}, {0x0D, "Enter"}, {0x13, "Pause"}, {0x14, "Caps Lock"},
        {0x1B, "Escape"}, {0x20, "Space"}, {0x21, "Page Up"}, {0x22, "Page Down"}, {0x23, "End"},
        {0x24, "Home"}, {0x25, "Left"}, {0x26, "Up"}, {0x27, "Right"}, {0x28, "Down"},
        {0x2C, "Print Screen"}, {0x2D, "Insert"}, {0x2E, "Delete"},
        {0x6A, "Num *"}, {0x6B, "Num +"}, {0x6D, "Num -"}, {0x6E, "Num ."}, {0x6F, "Num /"},
        {0x90, "Num Lock"}, {0x91, "Scroll Lock"},
        {0xBA, ";"}, {0xBB, "="}, {0xBC, ","}, {0xBD, "-"}, {0xBE, "."}, {0xBF, "/"}, {0xC0, "`"},
        {0xDB, "["}, {0xDC, "\\"}, {0xDD, "]"}, {0xDE, "'"},
    };

    std::string Lower(std::string s)
    {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::string Trim(const std::string& s)
    {
        size_t a = 0, b = s.size();
        while (a < b && s[a] == ' ') a++;
        while (b > a && s[b - 1] == ' ') b--;
        return s.substr(a, b - a);
    }

    // Whole string must be digits (base 10 or 16), 1-4 of them.
    bool ParseUnsigned(const std::string& s, unsigned aBase, unsigned& aOut)
    {
        if (s.empty() || s.size() > 4) return false;
        aOut = 0;
        for (char c : s)
        {
            unsigned d;
            if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
            else if (aBase == 16 && c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
            else return false;
            aOut = aOut * aBase + d;
        }
        return true;
    }

    uint16_t KeyFromName(const std::string& aName)
    {
        const std::string n = Lower(Trim(aName));
        if (n.empty()) return 0;
        for (const Named& k : kNames)
            if (n == Lower(k.Name)) return k.Key;
        if (n.size() == 1 && std::isalnum(static_cast<unsigned char>(n[0])))
            return static_cast<uint16_t>(std::toupper(static_cast<unsigned char>(n[0])));
        unsigned v = 0;
        if (n[0] == 'f' && ParseUnsigned(n.substr(1), 10, v) && v >= 1 && v <= 24)
            return static_cast<uint16_t>(0x70 + v - 1);
        if (n.rfind("num ", 0) == 0 && n.size() == 5 && n[4] >= '0' && n[4] <= '9')
            return static_cast<uint16_t>(0x60 + (n[4] - '0'));
        if (n.rfind("key 0x", 0) == 0 && ParseUnsigned(n.substr(6), 16, v) && v > 0 && v < 0xFF)
            return static_cast<uint16_t>(v);
        return 0;
    }
}

uint32_t Hotkey::Combo::Pack() const
{
    return static_cast<uint32_t>(Key) | (Ctrl ? 1u << 16 : 0u) | (Alt ? 1u << 17 : 0u) | (Shift ? 1u << 18 : 0u);
}

Hotkey::Combo Hotkey::Combo::Unpack(uint32_t aPacked)
{
    Combo c;
    c.Key   = static_cast<uint16_t>(aPacked & 0xFFFF);
    c.Ctrl  = (aPacked >> 16) & 1;
    c.Alt   = (aPacked >> 17) & 1;
    c.Shift = (aPacked >> 18) & 1;
    return c;
}

std::string Hotkey::KeyName(uint16_t aKey)
{
    for (const Named& k : kNames)
        if (k.Key == aKey) return k.Name;
    if ((aKey >= '0' && aKey <= '9') || (aKey >= 'A' && aKey <= 'Z')) return std::string(1, static_cast<char>(aKey));
    if (aKey >= 0x70 && aKey <= 0x87) return "F" + std::to_string(aKey - 0x70 + 1);
    if (aKey >= 0x60 && aKey <= 0x69) return "Num " + std::to_string(aKey - 0x60);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "Key 0x%02X", static_cast<unsigned>(aKey));
    return buf;
}

std::string Hotkey::ToString(const Combo& aCombo)
{
    if (aCombo.Empty()) return {};
    std::string s;
    if (aCombo.Ctrl) s += "Ctrl+";
    if (aCombo.Alt) s += "Alt+";
    if (aCombo.Shift) s += "Shift+";
    return s + KeyName(aCombo.Key);
}

Hotkey::Combo Hotkey::FromString(const std::string& aText)
{
    // Split on '+', but a trailing "+" after a '+' is the key "Num +" or "=": handle "Ctrl+Num +".
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i < aText.size(); i++)
    {
        const char c = aText[i];
        if (c == '+' && !Trim(cur).empty() && Lower(Trim(cur)) != "num")
        {
            parts.push_back(cur);
            cur.clear();
        }
        else cur += c;
    }
    parts.push_back(cur);

    Combo c;
    for (size_t i = 0; i + 1 < parts.size(); i++)
    {
        const std::string m = Lower(Trim(parts[i]));
        if (m == "ctrl") c.Ctrl = true;
        else if (m == "alt") c.Alt = true;
        else if (m == "shift") c.Shift = true;
        else return {};
    }
    c.Key = KeyFromName(parts.back());
    if (c.Key == 0) return {};
    return c;
}

bool Hotkey::IsModifierKey(uint16_t aKey)
{
    return aKey == 0x10 || aKey == 0x11 || aKey == 0x12 || (aKey >= 0xA0 && aKey <= 0xA5) || aKey == 0x5B || aKey == 0x5C;
}

std::string Hotkey::Refusal(const Combo& aCombo)
{
    if (aCombo.Empty()) return "No key.";
    if (aCombo.Key == 0x01 || aCombo.Key == 0x02) return "The left and right mouse buttons can't be hotkeys.";
    if (aCombo.Key == kVkEscape) return "Escape can't be a hotkey.";
    if (IsModifierKey(aCombo.Key)) return "Shift, Ctrl, Alt and Windows can only be used together with another key.";
    if (!aCombo.HasModifier() && (aCombo.Key == 0x09 || aCombo.Key == 0x0D || aCombo.Key == 0x20 || aCombo.Key == 0x08))
        return KeyName(aCombo.Key) + " needs Ctrl, Alt or Shift: on its own it's used all the time in game.";
    return {};
}
