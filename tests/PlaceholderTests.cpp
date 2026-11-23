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
    c.ReadLines = [](const std::string& path, std::vector<std::string>& lines, std::string& err) {
        static const std::map<std::string, std::vector<std::string>> files = {
            {"names.txt", {"Alpha", "", "Beta", "  ", "Gamma"}},
            {"lists\\one.txt", {"Only {time}"}},
            {"my file.txt", {"spaced"}},
            {"empty.txt", {"", " "}},
            {"ctl.txt", {"x\x0By\rz\x08\x08/g hi"}},
            {"ff.txt", {"a", "\x0C", "b"}}, // a line of only a control character still counts as a line
        };
        auto it = files.find(path);
        if (it == files.end()) { err = "\"" + path + "\" wasn't found."; return false; }
        lines = it->second;
        return true;
    };
    c.MapName = [](std::string& name, std::string&) { name = "Lion's Arch"; return true; };
    c.FetchUrl = [](const std::string& url, std::string& body, std::string& err) {
        if (url == "https://example.test/build") { body = R"({"id": 171234, "name": "Line\nbreak", "ok": true, "f": 1.5,
                                                       "list": [{"name": "first"}, {"name": "second"}], "nothing": null})"; return true; }
        if (url == "https://example.test/bad") { body = "<html>"; return true; }
        if (url == "https://example.test/ctl") { body = R"({"motd": "\b\b\u001b/g hi", "mix": "a\u0000b\u007fc\u0085d\u009fe\tf\rg",
                                                      "text": "café Ā€"})"; return true; }
        err = "couldn't reach the address.";
        return false;
    };
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

static void TestTxt()
{
    CHECK(X("{txt names.txt 1}") == "Alpha");
    CHECK(X("{txt names.txt 2}") == "Beta");        // blank lines don't count
    CHECK(X("{txt names.txt 3}") == "Gamma");
    CHECK(X("{txt names.txt}") == "Beta");          // random among the 3 (middle with this Random)
    CHECK(X("{txt lists\\one.txt}") == "Only {time}"); // file text isn't expanded again
    CHECK(X("{txt \"my file.txt\"}") == "spaced");  // quotes keep spaces
    CHECK(E("{txt names.txt 4}").find("only 3 line") != std::string::npos);
    CHECK(!E("{txt names.txt 0}").empty());
    CHECK(!E("{txt missing.txt}").empty());
    CHECK(!E("{txt empty.txt}").empty());
    CHECK(!E("{txt}").empty());
    CHECK(!E("{txt \"open.txt}").empty());          // unclosed quote
    CHECK(!E("{txt ..\\secret.txt}").empty());      // outside the addon folder
    CHECK(!E("{txt C:\\Windows\\win.ini}").empty());
}

static void TestSafePath()
{
    CHECK(IsSafeRelativePath("a.txt"));
    CHECK(IsSafeRelativePath("lists/names.txt"));
    CHECK(IsSafeRelativePath("lists\\names.txt"));
    CHECK(!IsSafeRelativePath(""));
    CHECK(!IsSafeRelativePath("/etc/passwd"));
    CHECK(!IsSafeRelativePath("\\\\server\\share\\x.txt"));
    CHECK(!IsSafeRelativePath("C:\\x.txt"));
    CHECK(!IsSafeRelativePath("a.txt:stream"));
    CHECK(!IsSafeRelativePath("../x.txt"));
    CHECK(!IsSafeRelativePath("lists/../../x.txt"));
    CHECK(!IsSafeRelativePath("lists//x.txt"));
    CHECK(!IsSafeRelativePath("./x.txt"));
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

static void TestJson()
{
    CHECK(X("{json id https://example.test/build}") == "171234");
    CHECK(X("{json name https://example.test/build}") == "Line break"); // newlines become spaces
    CHECK(X("{json ok https://example.test/build}") == "true");
    CHECK(X("{json f https://example.test/build}") == "1.5");
    CHECK(X("{json list.1.name https://example.test/build}") == "second");
    CHECK(X("{JSON list.0.name HTTPS://example.test/build}").empty()); // the URL is passed as typed
    CHECK(!E("{json list https://example.test/build}").empty());       // a list isn't one value
    CHECK(!E("{json nothing https://example.test/build}").empty());    // null
    CHECK(!E("{json list.5.name https://example.test/build}").empty()); // past the end
    CHECK(!E("{json missing https://example.test/build}").empty());
    CHECK(!E("{json id https://example.test/bad}").empty());           // not JSON
    CHECK(!E("{json id https://example.test/down}").empty());          // download failed
    CHECK(!E("{json id ftp://example.test/build}").empty());           // http(s) only
    CHECK(!E("{json id file:///C:/x.json}").empty());
    CHECK(!E("{json https://example.test/build}").empty());            // missing path

    std::string out, err;
    CHECK(JsonValue("[1, [2, 3]]", "1.0", out, err) && out == "2");
    CHECK(JsonValue("{\"a\": {\"b\": \"c\"}}", "a.b", out, err) && out == "c");
    CHECK(!JsonValue("{\"a\": 1}", "", out, err));
}

// SEC-013: a value can't press a key (control characters become spaces, from any source), and
// FromValue marks which bytes came from values so SendPlan can refuse a "/" a value would add.
static void TestControlCharacters()
{
    CHECK(X("{json motd https://example.test/ctl}") == "   /g hi");
    CHECK(X("{json mix https://example.test/ctl}") == "a b c d e f g");
    // Other text is kept, including U+00A0 (0xC2 0xA0) and U+0100 (0xC4 0x80), which aren't C1.
    CHECK(X("{json text https://example.test/ctl}") == "caf\xC3\xA9\xC2\xA0\xC4\x80\xE2\x82\xAC");

    CHECK(X("{txt ctl.txt}") == "x y z  /g hi");
    // Line numbers and the random pick count lines as before: blank means only spaces and tabs.
    CHECK(X("{txt ff.txt 3}") == "b");
    CHECK(X("{txt ff.txt 2}") == " ");                // the \x0C line, as a space
    CHECK(X("{txt ff.txt}") == " ");                  // random among the 3 (the middle with this Random)

    Context c = Ctx();
    c.MapName = [](std::string& name, std::string&) { name = "Lion\x07s\nArch\xC2\x9B"; return true; };
    CHECK(Expand("{map}", c).Text == "Lion s Arch ");

    // The macro's own text is left as it is (SendPlan refuses control characters in it).
    CHECK(X("a\tb {time}") == "a\tb 09:05");
}

static void TestFromValue()
{
    Result r = Expand("/g {time}!", Ctx());
    CHECK(r.Text == "/g 09:05!");
    CHECK(r.FromValue == (std::vector<bool>{false, false, false, true, true, true, true, true, false}));
    // Unknown names and plain braces are the user's text.
    r = Expand("{smile}{txt \"my file.txt\"}", Ctx());
    CHECK(r.Text == "{smile}spaced");
    CHECK(r.FromValue == (std::vector<bool>{false, false, false, false, false, false, false,
                                            true, true, true, true, true, true}));
    // No placeholders: nothing from a value.
    r = Expand("/wave", Ctx());
    CHECK(r.FromValue == std::vector<bool>(5, false));
    // A failed placeholder clears both.
    r = Expand("a {txt missing.txt}", Ctx());
    CHECK(!r.Error.empty() && r.Text.empty() && r.FromValue.empty());
}

static void TestHas()
{
    CHECK(HasPlaceholders("at {time}"));
    CHECK(HasPlaceholders("{Random 5}"));
    CHECK(!HasPlaceholders("{smile}"));
    // Uses: by name, in any case, as Expand finds them.
    CHECK(Placeholders::Uses("say {TXT jokes.txt} now", "txt"));
    CHECK(Placeholders::Uses("{Json id https://a}", "json"));
    CHECK(!Placeholders::Uses("{txts a}", "txt"));
    CHECK(!Placeholders::Uses("{ {time}", "txt"));
    CHECK(!Placeholders::Uses("no braces txt", "txt"));
    CHECK(Placeholders::Uses("{x {txt a}", "txt"));
    CHECK(Placeholders::Uses("{{txt a}", "txt"));
    CHECK(Placeholders::Uses("}{txt}", "txt"));
    CHECK(Placeholders::Uses("{a}}{txt b}", "txt"));
    CHECK(!Placeholders::Uses("{txt", "txt"));
    CHECK(!Placeholders::Uses("{txt a {b}", "txt")); // the inner one counts, as in Expand
    CHECK(HasPlaceholders("{{time}"));
    CHECK(!HasPlaceholders("{time"));
    // SEC-028: one pass, not a scan to the next "}" per "{" (run every frame in the preview).
    {
        const std::string braces(2 * 1024 * 1024, '{');
        const auto start = std::chrono::steady_clock::now();
        CHECK(!Placeholders::Uses(braces, "txt"));
        CHECK(!HasPlaceholders(braces + "}"));
        CHECK(Placeholders::Uses(braces + "txt}", "txt"));
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
    TestTxt();
    TestSafePath();
    TestMap();
    TestJson();
    TestControlCharacters();
    TestFromValue();
    TestHas();
}
