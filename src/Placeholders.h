#pragma once

// Placeholders in macro text, filled in at send time (docs/design.md, "Placeholders").
// No Nexus, ImGui or windows.h dependency: everything from the outside world (clock, random
// numbers, the map name) comes in through Context, so it can be unit-tested
// (tests/PlaceholderTests.cpp).
//
// Syntax: {name} or {name arg arg ...}. Arguments are separated by spaces. Names are
// case-insensitive.
//  - A name we don't know (e.g. {smile}) is left exactly as typed.
//  - A known placeholder that can't be filled in (bad number, unknown map) is an error: the
//    macro isn't sent at all, rather than sending a half-filled line.
//  - {txt} and {json} were removed (owner, 2026-11-23). A line that still uses one isn't sent
//    either (an error), so a file name or web address never reaches chat by surprise; the editor
//    and the import preview point them out (UsesRemoved).
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
        // The current map's name. May block briefly (a cached GW2 API lookup).
        std::function<bool(std::string& aName, std::string& aError)> MapName;
    };

    struct Result
    {
        std::string       Text;
        std::vector<bool> FromValue; // one per byte of Text: true where it came from a filled-in value
        std::string       Error;     // empty on success
    };

    Result Expand(const std::string& aText, const Context& aContext);

    // Whether aText has at least one known placeholder (for "filled in when sent" hints).
    bool HasPlaceholders(const std::string& aText);

    // Whether aText still uses a removed placeholder ({txt} or {json}, in any case).
    bool UsesRemoved(const std::string& aText);

    struct Info
    {
        const char* Syntax;
        const char* Description;
    };
    const std::vector<Info>& List(); // for the help tooltip, in display order
}
