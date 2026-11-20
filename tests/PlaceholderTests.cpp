// Standalone tests for src/Placeholders.cpp.

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
        };
        auto it = files.find(path);
        if (it == files.end()) { err = "\"" + path + "\" wasn't found."; return false; }
        lines = it->second;
        return true;
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

static void TestHas()
{
    CHECK(HasPlaceholders("at {time}"));
    CHECK(HasPlaceholders("{Random 5}"));
    CHECK(!HasPlaceholders("{smile}"));
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
    TestHas();
}
