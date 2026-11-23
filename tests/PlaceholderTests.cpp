// Standalone tests for src/Placeholders.cpp.

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "Placeholders.h"
#include "TestCheck.h"

using namespace Placeholders;

static Context Ctx()
{
    Context c;
    c.Year = 2026; c.Month = 11; c.Day = 20; c.Hour = 9; c.Minute = 5;
    c.Weekday = "Friday";
    c.AddonVersion = "Chat Messages v0.1.0";
    c.Random = [](long long lo, long long hi) { return lo + (hi - lo) / 2; }; // deterministic: the middle
    c.MapName = [](std::string& name, std::string&) { name = "Lion's Arch"; return true; };
    return c;
}

static std::string X(const std::string& s) { return Expand(s, Ctx()).Text; }
static std::string E(const std::string& s) { return Expand(s, Ctx()).Error; }

static void TestTime()
{
    CHECK(X("{time}") == "09:05");
    CHECK(X("{hour}h{min}") == "09h05");
    CHECK(X("Today is {today}.") == "Today is Friday, 20.11.2026.");
    CHECK(X("{TIME}") == "09:05"); // names are case-insensitive
    CHECK(X("{nexus}") == "Chat Messages v0.1.0");
}

static void TestUnknownAndPlainText()
{
    CHECK(X("no braces") == "no braces");
    CHECK(X("{smile} hi") == "{smile} hi");      // unknown: kept as typed
    CHECK(X("{ time}") == "{ time}");            // a space first: not a placeholder name
    CHECK(X("open { only") == "open { only");
    CHECK(X("a } b") == "a } b");
    CHECK(X("{ {time}") == "{ 09:05");
    CHECK(X("{}") == "{}");
    CHECK(X("[&BDAEAAA=] {time}") == "[&BDAEAAA=] 09:05"); // chat links pass through
    CHECK(E("{smile}").empty());
}

static void TestRandom()
{
    CHECK(X("{random}") == "50");
    CHECK(X("{random 10}") == "5");
    CHECK(X("{random 10 20}") == "15");
    CHECK(X("{random 20 10}") == "15");          // swapped bounds
    CHECK(X("{random -10 -2}") == "-6");
    CHECK(X("{random 7 7}") == "7");
    CHECK(!E("{random x}").empty());
    CHECK(!E("{random 1 2 3}").empty());
    CHECK(!E("{random 99999999999999999}").empty()); // too big
    CHECK(X("{random x}").empty());                  // on error nothing is returned
}

// {txt} and {json} were removed (owner, 2026-11-23): a line that still has one is refused, so a
// file name or web address is never typed into chat as it stands.
static void TestRemoved()
{
    CHECK(!E("{txt names.txt}").empty());
    CHECK(X("{txt names.txt}").empty());
    CHECK(E("say {TXT a.txt 2} now").find("{txt}") != std::string::npos);
    CHECK(!E("{json id https://api.guildwars2.com/v2/build}").empty());
    CHECK(!E("{Json}").empty());
    CHECK(!E("at {time} {json x y}").empty());           // the whole line, not just that part
    CHECK(!E("{ {txt a}").empty());                      // the inner one counts, as for any name
    CHECK(X("{txts a}") == "{txts a}");                  // other names are left as typed
    CHECK(X("{ txt a}") == "{ txt a}");
    CHECK(X("{txt") == "{txt");                          // not closed: plain text
    CHECK(X("txt and json") == "txt and json");
    CHECK(!HasPlaceholders("{txt a}"));                  // not a placeholder any more
    for (const Info& i : List())
        CHECK(std::string(i.Syntax).find("txt") == std::string::npos && std::string(i.Syntax).find("json") == std::string::npos);
}

static void TestMap()
{
    CHECK(X("We're in {map}!") == "We're in Lion's Arch!");
    CHECK(!E("{map extra}").empty());

    Context c = Ctx();
    c.MapName = [](std::string&, std::string& err) { err = "the map name couldn't be looked up."; return false; };
    CHECK(Expand("{map}", c).Error == "{map}: the map name couldn't be looked up.");
    c.MapName = nullptr;
    CHECK(!Expand("{map}", c).Error.empty());
}

// SEC-013: a value can't press a key (control characters become spaces, from any source), and
// FromValue marks which bytes came from values so SendPlan can refuse a "/" a value would add.
static void TestControlCharacters()
{
    Context c = Ctx();
    c.MapName = [](std::string& name, std::string&) { name = "Lion\x07s\nArch\xC2\x9B"; return true; };
    CHECK(Expand("{map}", c).Text == "Lion s Arch ");
    c.MapName = [](std::string& name, std::string&) { name = "\b\b\x1b/g hi"; return true; };
    CHECK(Expand("{map}", c).Text == "   /g hi");
    c.MapName = [](std::string& name, std::string&) { name = std::string("a\0b\x7f" "c\xC2\x85" "d\xC2\x9F" "e\tf\rg", 15); return true; };
    CHECK(Expand("{map}", c).Text == "a b c d e f g");
    // Other text is kept, including U+00A0 (0xC2 0xA0) and U+0100 (0xC4 0x80), which aren't C1.
    c.MapName = [](std::string& name, std::string&) { name = "caf\xC3\xA9\xC2\xA0\xC4\x80\xE2\x82\xAC"; return true; };
    CHECK(Expand("{map}", c).Text == "caf\xC3\xA9\xC2\xA0\xC4\x80\xE2\x82\xAC");
    // A value is never filled in again: a map name with "{time}" in it stays as it is.
    c.MapName = [](std::string& name, std::string&) { name = "Only {time}"; return true; };
    CHECK(Expand("{map}", c).Text == "Only {time}");

    // The macro's own text is left as it is (SendPlan refuses control characters in it).
    CHECK(X("a\tb {time}") == "a\tb 09:05");
}

static void TestFromValue()
{
    Result r = Expand("/g {time}!", Ctx());
    CHECK(r.Text == "/g 09:05!");
    CHECK(r.FromValue == (std::vector<bool>{false, false, false, true, true, true, true, true, false}));
    // Unknown names and plain braces are the user's text.
    r = Expand("{smile}{min}", Ctx());
    CHECK(r.Text == "{smile}05");
    CHECK(r.FromValue == (std::vector<bool>{false, false, false, false, false, false, false, true, true}));
    // No placeholders: nothing from a value.
    r = Expand("/wave", Ctx());
    CHECK(r.FromValue == std::vector<bool>(5, false));
    // A failed placeholder clears both.
    r = Expand("a {random x}", Ctx());
    CHECK(!r.Error.empty() && r.Text.empty() && r.FromValue.empty());
}

static void TestHas()
{
    CHECK(HasPlaceholders("at {time}"));
    CHECK(HasPlaceholders("{Random 5}"));
    CHECK(!HasPlaceholders("{smile}"));
    // UsesRemoved: {txt} and {json} by name, in any case, as Expand finds them.
    CHECK(UsesRemoved("say {TXT jokes.txt} now"));
    CHECK(UsesRemoved("{Json id https://a}"));
    CHECK(!UsesRemoved("{txts a}"));
    CHECK(!UsesRemoved("{time} {random}"));
    CHECK(!UsesRemoved("no braces txt json"));
    CHECK(UsesRemoved("{x {txt a}"));
    CHECK(UsesRemoved("{{txt a}"));
    CHECK(UsesRemoved("}{txt}"));
    CHECK(UsesRemoved("{a}}{json b}"));
    CHECK(!UsesRemoved("{txt"));
    CHECK(!UsesRemoved("{txt a {b}")); // the inner one counts, as in Expand
    CHECK(!UsesRemoved(""));
    CHECK(HasPlaceholders("{{time}"));
    CHECK(!HasPlaceholders("{time"));
    // SEC-028: one pass, not a scan to the next "}" per "{" (run every frame in the preview).
    {
        const std::string braces(2 * 1024 * 1024, '{');
        const auto start = std::chrono::steady_clock::now();
        CHECK(!UsesRemoved(braces));
        CHECK(!HasPlaceholders(braces + "}"));
        CHECK(UsesRemoved(braces + "txt}"));
        // Expand too (on the sender thread, but a send shouldn't take minutes).
        CHECK(X(braces) == braces);
        CHECK(X(braces + "}") == braces + "}");
        CHECK(X(braces + "time}") == braces.substr(1) + "09:05");
        CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
    }
    // The cached "}" still finds the right one each time.
    CHECK(X("{a}{time}") == "{a}09:05");
    CHECK(X("{ {x} {time}}") == "{ {x} 09:05}");
    CHECK(X("{{{time}}}") == "{{09:05}}");
    CHECK(X("{time}{hour}") == "09:0509");
    CHECK(X("{x {y {time}") == "{x {y 09:05");
    CHECK(!HasPlaceholders("plain"));
    CHECK(!List().empty());
}

void RunPlaceholderTests()
{
    TestTime();
    TestUnknownAndPlainText();
    TestRandom();
    TestRemoved();
    TestMap();
    TestControlCharacters();
    TestFromValue();
    TestHas();
}
