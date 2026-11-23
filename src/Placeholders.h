#pragma once

// Placeholders in macro text, filled in at send time (docs/design.md, "Placeholders").
// No Nexus, ImGui or windows.h dependency: everything from the outside world (clock, random
// numbers, files) comes in through Context, so it can be unit-tested (tests/PlaceholderTests.cpp).
//
// Syntax: {name} or {name arg arg ...}. Arguments are separated by spaces; "double quotes" keep
// spaces inside one argument. Names are case-insensitive.
//  - A name we don't know (e.g. {smile}) is left exactly as typed.
//  - A known placeholder that can't be filled in (bad number, missing file) is an error: the
//    macro isn't sent at all, rather than sending a half-filled line.
//  - A filled-in value never holds a control character (C0, DEL, C1): each becomes a space.
//  - Result::FromValue marks which bytes came from a value, so SendPlan can refuse a value that
//    would start a chat command (SEC-013).

#include <functional>
#include <string>
#include <vector>

namespace Placeholders
{
    struct Context
    {
        // Local date and time.
        int         Year = 2000, Month = 1, Day = 1, Hour = 0, Minute = 0;
        std::string Weekday = "Monday"; // in the user's language
        std::string AddonVersion;       // e.g. "Chat Messages v0.1.0"

        // A random integer in [aMin, aMax] (both inclusive, aMin <= aMax).
        std::function<long long(long long aMin, long long aMax)> Random;
        // Reads a text file relative to the addon folder (already checked by IsSafeRelativePath).
        // Returns false with aError set if it can't.
        std::function<bool(const std::string& aRelPath, std::vector<std::string>& aLines, std::string& aError)> ReadLines;
        // The current map's name. May block briefly (a cached GW2 API lookup).
        std::function<bool(std::string& aName, std::string& aError)> MapName;
        // Downloads aUrl (http/https only, already checked). May block briefly. The URL may come
        // from an imported macro, so it must refuse servers on this PC or a local network
        // (Sender uses Http::GetPublic).
        std::function<bool(const std::string& aUrl, std::string& aBody, std::string& aError)> FetchUrl;
    };

    struct Result
    {
        std::string       Text;
        std::vector<bool> FromValue; // one per byte of Text: true where it came from a filled-in value
        std::string       Error;     // empty on success
    };

    Result Expand(const std::string& aText, const Context& aContext);

    // One value from a JSON document: aPath is dot-separated, numbers index arrays
    // ("data.0.name"). Strings, numbers and true/false work; objects, arrays and null don't.
    bool JsonValue(const std::string& aJson, const std::string& aPath, std::string& aOut, std::string& aError);

    // Whether aText has at least one known placeholder (for "filled in when sent" hints).
    bool HasPlaceholders(const std::string& aText);

    // Files for {txt} must stay inside the addon folder: no drive letters, no leading slash,
    // no ".." parts, nothing empty.
    bool IsSafeRelativePath(const std::string& aPath);

    struct Info
    {
        const char* Syntax;
        const char* Description;
    };
    const std::vector<Info>& List(); // for the help tooltip, in display order
}
