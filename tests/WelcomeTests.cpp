// Standalone tests for src/Welcome.cpp and src/ReleaseNotes.cpp (the first-run welcome, the tour
// and the update notes).

#include <string>

#include <nlohmann/json.hpp>

#include "ReleaseNotes.h"
#include "TestCheck.h"
#include "Welcome.h"

static void TestDecideWelcome()
{
    // A real first run: no library, nothing remembered.
    Welcome::Start s = Welcome::Decide(false, false, true, true);
    CHECK(s.ShowWelcome && !s.MarkShown && !s.MarkExamplesAdded);

    // An upgrade with a library: no welcome, both settings saved.
    s = Welcome::Decide(false, false, false, false);
    CHECK(!s.ShowWelcome && s.MarkShown && s.MarkExamplesAdded);
    // An upgrade where the examples were added before.
    s = Welcome::Decide(true, false, false, false);
    CHECK(!s.ShowWelcome && s.MarkShown && !s.MarkExamplesAdded);
    // macros.json removed on purpose after the examples were added: no welcome.
    s = Welcome::Decide(true, false, true, true);
    CHECK(!s.ShowWelcome && s.MarkShown);
    // macros.json gone but its .bak there (not a first run).
    s = Welcome::Decide(false, false, false, true);
    CHECK(!s.ShowWelcome && s.MarkShown && s.MarkExamplesAdded);

    // Welcome answered with "Start empty", then the game restarted with still no library.
    s = Welcome::Decide(false, true, true, true);
    CHECK(!s.ShowWelcome && !s.MarkShown && !s.MarkExamplesAdded);
    s = Welcome::Decide(true, true, false, false);
    CHECK(!s.ShowWelcome && !s.MarkShown && !s.MarkExamplesAdded);
}

static void TestTourSteps()
{
    const auto& steps = Welcome::Steps();
    CHECK(steps.size() >= 6);
    bool tree = false, header = false;
    for (const Welcome::Step& st : steps)
    {
        CHECK(st.Title && *st.Title && st.Text && *st.Text);
        CHECK(st.Target != Welcome::ETarget::Count);
        tree   = tree || st.Target == Welcome::ETarget::Tree;
        header = header || st.Target == Welcome::ETarget::Header;
    }
    CHECK(tree && header);
    CHECK(steps.front().Target == Welcome::ETarget::Tree);
    CHECK(Welcome::NeedsMacro(Welcome::ETarget::Header) && Welcome::NeedsMacro(Welcome::ETarget::Lines) &&
          Welcome::NeedsMacro(Welcome::ETarget::Sections));
    CHECK(!Welcome::NeedsMacro(Welcome::ETarget::Tree) && !Welcome::NeedsMacro(Welcome::ETarget::None));
}

static void TestVersions()
{
    using namespace ReleaseNotes;
    Version a, b;
    CHECK(ParseVersion("2026.11.21.1412", a));
    CHECK(a.Part[0] == 2026 && a.Part[1] == 11 && a.Part[2] == 21 && a.Part[3] == 1412);
    CHECK(ParseVersion("2026.11.21.1412.Dev", b) && a == b);
    CHECK(ParseVersion("2026.12.1.5", b) && a < b && !(b < a));
    CHECK(!ParseVersion("", b));
    CHECK(!ParseVersion("Unreleased", b));
    CHECK(!ParseVersion("2026.11.21", b));
    CHECK(!ParseVersion("2026.11.21.1412x", b));
    CHECK(!ParseVersion("20260.11.21.1412", b));
    CHECK(!ParseVersion("2026..21.1412", b));
}

static const char* kLog =
    "# Changelog\n"
    "\n"
    "Intro text.\n"
    "\n"
    "## [Unreleased]\n"
    "\n"
    "### Added\n"
    "- Not released yet.\n"
    "\n"
    "## [2026.12.02.0900] - 2026-12-02\n"
    "\n"
    "### Added\n"
    "- **A welcome** for new players, with a\n"
    "  [tour](Home) of the `library`.\n"
    "  - A nested point.\n"
    "\r\n"
    "### Fixed\n"
    "- Something.\n"
    "\n"
    "## [2026.11.30.1200]\n"
    "### Changed\n"
    "- Older change.\n"
    "\n"
    "## [2026.11.21.1412]\n"
    "- First release.\n";

static void TestParseAndSince()
{
    using namespace ReleaseNotes;
    const auto sections = Parse(kLog);
    CHECK(sections.size() == 3);
    if (sections.size() != 3) return;
    CHECK(sections[0].Version == "2026.12.02.0900");
    CHECK(sections[0].Body.find("Not released") == std::string::npos);
    CHECK(sections[0].Body.compare(0, 9, "### Added") == 0); // leading blank line dropped
    CHECK(sections[0].Body.back() != '\n');
    CHECK(sections[2].Body == "- First release.");

    // Only the current version's, for an update from before this feature.
    auto s = Since(sections, "", "2026.12.02.0900");
    CHECK(s.size() == 1 && s[0].Version == "2026.12.02.0900");
    s = Since(sections, "garbage", "2026.12.02.0900.Dev");
    CHECK(s.size() == 1);
    // Every skipped version, newest first.
    s = Since(sections, "2026.11.21.1412", "2026.12.02.0900");
    CHECK(s.size() == 2 && s[0].Version == "2026.12.02.0900" && s[1].Version == "2026.11.30.1200");
    // Never a version newer than the running DLL.
    s = Since(sections, "2026.11.21.1412", "2026.11.30.1200");
    CHECK(s.size() == 1 && s[0].Version == "2026.11.30.1200");
    // Nothing new.
    CHECK(Since(sections, "2026.12.02.0900", "2026.12.02.0900").empty());
    // A version with no section of its own (a Dev build).
    CHECK(Since(sections, "", "2026.12.05.0000").empty());
    CHECK(Parse("").empty());
    CHECK(Parse("## [Unreleased]\n- x\n").empty());
}

static void TestDecideNotes()
{
    using namespace ReleaseNotes;
    // An update with notes, shown.
    Start s = Decide(true, false, "2026.11.21.1412", "2026.12.02.0900", true);
    CHECK(s.Show && !s.Remember);
    s = Decide(true, false, "", "2026.12.02.0900", true);
    CHECK(s.Show);
    // Turned off, a first run, or nothing to show: just remembered.
    s = Decide(false, false, "2026.11.21.1412", "2026.12.02.0900", true);
    CHECK(!s.Show && s.Remember);
    s = Decide(true, true, "", "2026.12.02.0900", true);
    CHECK(!s.Show && s.Remember);
    s = Decide(true, false, "", "2026.12.02.0900", false);
    CHECK(!s.Show && s.Remember);
    // Same version, or an older DLL: nothing at all.
    s = Decide(true, false, "2026.12.02.0900", "2026.12.02.0900.Dev", true);
    CHECK(!s.Show && !s.Remember);
    s = Decide(true, false, "2026.12.02.0900", "2026.11.30.1200", true);
    CHECK(!s.Show && !s.Remember);
    s = Decide(true, false, "", "not a version", true);
    CHECK(!s.Show && !s.Remember);

    CHECK(ShowFromSettings(nlohmann::json::object()));
    CHECK(ShowFromSettings(nlohmann::json::parse(R"({"show_update_notes":"no"})")));
    CHECK(!ShowFromSettings(nlohmann::json::parse(R"({"show_update_notes":false})")));
    CHECK(ShowFromSettings(nlohmann::json()));
}

static void TestBlocks()
{
    using namespace ReleaseNotes;
    const auto sections = Parse(kLog);
    if (sections.empty()) return;
    const auto b = ToBlocks(sections[0].Body);
    CHECK(b.size() == 5);
    if (b.size() != 5) return;
    CHECK(b[0].Kind == EKind::Heading && b[0].Text == "Added");
    CHECK(b[1].Kind == EKind::Bullet && b[1].Indent == 0 && b[1].Text == "A welcome for new players, with a tour of the library.");
    CHECK(b[2].Kind == EKind::Bullet && b[2].Indent == 1 && b[2].Text == "A nested point.");
    CHECK(b[3].Kind == EKind::Heading && b[3].Text == "Fixed");
    CHECK(b[4].Kind == EKind::Bullet && b[4].Text == "Something.");

    const auto t = ToBlocks("Plain text\nthat wraps.\n\nNew [para(x).\n[a](b) and **c**");
    CHECK(t.size() == 2);
    if (t.size() == 2)
    {
        CHECK(t[0].Kind == EKind::Text && t[0].Text == "Plain text that wraps.");
        CHECK(t[1].Text == "New [para(x). a and c"); // an unclosed link stays as typed
    }
    CHECK(ToBlocks("").empty());
}

void RunWelcomeTests()
{
    TestDecideWelcome();
    TestTourSteps();
    TestVersions();
    TestParseAndSince();
    TestDecideNotes();
    TestBlocks();
}
