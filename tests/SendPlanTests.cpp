// Standalone tests for src/SendPlan.cpp: the exact step sequence the sender performs.

#include <string>
#include <vector>

#include "SendPlan.h"
#include "TestCheck.h"

using namespace SendPlan;
using MacroTree::EChannel;
using MacroTree::Line;

static Line L(EChannel c, const std::string& msg, const std::string& to = "", bool bc = false)
{
    Line l;
    l.Channel = c;
    l.Message = msg;
    l.WhisperTo = to;
    l.SquadBroadcast = bc;
    return l;
}

static std::vector<EStep> Types(const Plan& p)
{
    std::vector<EStep> t;
    for (const Step& s : p.Steps) t.push_back(s.Type);
    return t;
}

static void TestSimpleLine()
{
    Plan p = Build({L(EChannel::Say, "hello")}, false);
    CHECK(p.Error.empty());
    CHECK(p.LinesToSend == 1);
    CHECK(Types(p) == (std::vector<EStep>{EStep::OpenChat, EStep::WaitChatOpen, EStep::Type, EStep::KeyEnter, EStep::WaitChatClosed}));
    CHECK(p.Steps[2].Text == "/s hello");

    p = Build({L(EChannel::Current, "/wave")}, false);
    CHECK(p.Steps[2].Text == "/wave");
}

static void TestTwoLinesPause()
{
    Plan p = Build({L(EChannel::Map, "a"), L(EChannel::Party, "b")}, false);
    CHECK(p.LinesToSend == 2);
    CHECK(p.Steps.size() == 11);
    CHECK(p.Steps[5].Type == EStep::Pause && p.Steps[5].Ms == kPauseBetweenLinesMs);
    CHECK(p.Steps[8].Text == "/p b" && p.Steps[8].Line == 1);
}

static void TestWhisper()
{
    Plan p = Build({L(EChannel::Whisper, "hi there", "Some Name")}, false);
    CHECK(p.Error.empty());
    CHECK(Types(p) == (std::vector<EStep>{EStep::OpenChat, EStep::WaitChatOpen, EStep::Type, EStep::Type,
                                          EStep::KeyTab, EStep::Type, EStep::KeyEnter, EStep::WaitChatClosed}));
    CHECK(p.Steps[2].Text == "/w ");
    CHECK(p.Steps[3].Text == "Some Name");
    CHECK(p.Steps[5].Text == "hi there");

    p = Build({L(EChannel::Say, "ok"), L(EChannel::Whisper, "hi")}, false);
    CHECK(!p.Error.empty() && p.Steps.empty()); // nothing is sent if any line is invalid
    CHECK(p.Error.find("Line 2") == 0);
}

static void TestBroadcast()
{
    Plan p = Build({L(EChannel::Squad, "Stack on tag", "", true)}, true);
    CHECK(p.Steps[0].Type == EStep::OpenSquadBroadcast);
    CHECK(p.Steps[2].Text == "Stack on tag");

    // Not commander: a normal squad message.
    p = Build({L(EChannel::Squad, "Stack on tag", "", true)}, false);
    CHECK(p.Steps[0].Type == EStep::OpenChat);
    CHECK(p.Steps[2].Text == "/d Stack on tag");
}

static void TestEmptyAndTooLong()
{
    Plan p = Build({L(EChannel::Say, ""), L(EChannel::Say, "x")}, false);
    CHECK(p.Error.empty() && p.LinesToSend == 1);
    CHECK(p.Steps[0].Type == EStep::OpenChat); // no pause before the first sent line
    CHECK(p.Steps[2].Line == 1);

    p = Build({L(EChannel::Say, "")}, false);
    CHECK(!p.Error.empty() && p.Steps.empty());
    p = Build({}, false);
    CHECK(!p.Error.empty());

    p = Build({L(EChannel::Say, std::string(196, 'a'))}, false); // "/s " + 196 = 199
    CHECK(p.Error.empty());
    p = Build({L(EChannel::Say, std::string(197, 'a'))}, false);
    CHECK(!p.Error.empty() && p.Steps.empty());
}

// SEC-013: only our own Enter and Tab may reach the chat box.
static void TestControlCharacters()
{
    const char* bad[] = {"a\tb", "a\rb", "a\nb", "a\x08" "b", "\x1B" "a", "a\x7F", "a\xC2\x85" "b", "a\xC2\x9F"};
    for (const char* msg : bad)
    {
        Plan p = Build({L(EChannel::Say, "ok"), L(EChannel::Current, msg)}, false);
        CHECK(!p.Error.empty() && p.Steps.empty());
        CHECK(p.Error.find("Line 2") == 0);
        p = Build({L(EChannel::Squad, msg, "", true)}, true); // the broadcast box too
        CHECK(!p.Error.empty() && p.Steps.empty());
    }
    // A whisper's recipient is typed too; one left on a non-whisper line isn't.
    CHECK(!Build({L(EChannel::Whisper, "hi", "Some\tName")}, false).Error.empty());
    CHECK(Build({L(EChannel::Say, "hi", "Some\tName")}, false).Error.empty());
    // Accents, a no-break space (0xC2 0xA0) and other UTF-8 text are fine.
    Plan p = Build({L(EChannel::Say, "caf\xC3\xA9 \xC2\xA0 \xE2\x82\xAC")}, false);
    CHECK(p.Error.empty() && p.Steps[2].Text == "/s caf\xC3\xA9 \xC2\xA0 \xE2\x82\xAC");
}

// The mask for a text whose bytes from aFrom on came from a placeholder value.
static std::vector<bool> V(const std::string& aText, size_t aFrom)
{
    std::vector<bool> m(aText.size(), false);
    for (size_t k = aFrom; k < aText.size(); k++) m[k] = true;
    return m;
}

static Origin O(std::vector<bool> aMessage, std::vector<bool> aTo = {})
{
    Origin o;
    o.Message   = std::move(aMessage);
    o.WhisperTo = std::move(aTo);
    return o;
}

// SEC-013: a "/" chat reads as a command must be one the user wrote.
static void TestPlaceholderCommand()
{
    auto refused = [](const Plan& p) { return !p.Error.empty() && p.Steps.empty(); };
    auto line1 = [&](EChannel c, const std::string& msg, size_t from, bool commander = false, bool bc = false) {
        return Build({L(c, msg, "", bc)}, commander, {O(V(msg, from))});
    };

    // A whole value at the start, on any channel (the game reads commands as the text arrives, so
    // even after our own "/s " a "/" may begin one).
    CHECK(refused(line1(EChannel::Current, "/g hello", 0)));
    CHECK(line1(EChannel::Current, "/g hello", 0).Error.find("Line 1") == 0);
    CHECK(refused(line1(EChannel::Say, "/g hello", 0)));
    CHECK(refused(line1(EChannel::Guild, "/w Victim", 0)));
    CHECK(refused(line1(EChannel::Guild3, "/g hello", 0)));
    CHECK(refused(line1(EChannel::Squad, "/g hello", 0, true, true)));  // the broadcast box
    CHECK(refused(line1(EChannel::Squad, "/g hello", 0, false, true))); // not commander: "/d /g hello"

    // The gap the declined draft left: after a channel command the user wrote, the box is empty
    // again, so a value's "/" right after it starts another command.
    CHECK(refused(line1(EChannel::Current, "/g /w Victim", 3)));
    CHECK(refused(line1(EChannel::Current, "/g  \xC2\xA0/w Victim", 3)));  // after more spaces of any kind
    CHECK(refused(line1(EChannel::Current, "/g /wave /w Victim", 9)));    // after two of the user's commands
    CHECK(refused(line1(EChannel::Current, "/g /w Victim", 2)));           // the value adds the space too
    CHECK(refused(line1(EChannel::Current, "/wave /g hi", 1)));            // the user's "/", the value's word and "/g"
    CHECK(refused(line1(EChannel::Say, "/g hi", 0)));                      // after our own "/s "

    // Any kind of space first.
    CHECK(refused(line1(EChannel::Current, "   /g hello", 0)));
    CHECK(refused(line1(EChannel::Current, "\xC2\xA0/g hello", 0)));                   // U+00A0
    CHECK(refused(line1(EChannel::Say, "\xE3\x80\x80/g hello", 0)));                   // U+3000
    CHECK(refused(line1(EChannel::Map, "\xE2\x80\x8B \xEF\xBB\xBF/g hello", 0)));      // U+200B, U+FEFF
    CHECK(refused(line1(EChannel::Current, "\xE1\x9A\x80\xE2\x80\xAF\xE2\x81\x9F/g", 0))); // U+1680, U+202F, U+205F

    // Whispers: the recipient box (after our "/w ") and the message box (after Tab) both start fresh.
    Plan p = Build({L(EChannel::Whisper, "hello", "/g")}, false, {O(V("hello", 5), V("/g", 0))});
    CHECK(refused(p));
    p = Build({L(EChannel::Whisper, "/g hello", "Some Name")}, false, {O(V("/g hello", 0), V("Some Name", 9))});
    CHECK(refused(p));
    p = Build({L(EChannel::Whisper, "hello", "Some Name")}, false, {O(V("hello", 0), V("Some Name", 0))});
    CHECK(p.Error.empty()); // values without "/" are fine

    // Allowed: the user's own commands, values after them that don't start with "/", and a "/"
    // later in the text.
    CHECK(line1(EChannel::Current, "/wave", 5).Error.empty());              // no value at all
    CHECK(line1(EChannel::Current, "/wave", 1).Error.empty());              // "/{map}": the user's "/"
    CHECK(line1(EChannel::Current, " /wave", 6).Error.empty());
    CHECK(line1(EChannel::Current, "/g 09:05", 3).Error.empty());           // "/g {time}"
    CHECK(line1(EChannel::Current, "/g /wave", 8).Error.empty());           // two of the user's commands
    p = line1(EChannel::Say, "/shrug 09:05", 7);
    CHECK(p.Error.empty() && p.Steps[2].Text == "/s /shrug 09:05");
    CHECK(line1(EChannel::Current, "at 09:05 /g", 3).Error.empty());        // "at {time} {map}"
    CHECK(line1(EChannel::Current, "/g hi /w x", 6).Error.empty());         // after the message started
    CHECK(line1(EChannel::Current, "/g hi /w x", 3).Error.empty());
    CHECK(line1(EChannel::Current, "  ", 0).Error.empty());                 // no "/" at all
    p = Build({L(EChannel::Say, "hi", "/g")}, false, {O(V("hi", 2), V("/g", 0))});
    CHECK(p.Error.empty()); // a recipient on a non-whisper line isn't typed

    // Every line is checked, and nothing is sent if one fails.
    p = Build({L(EChannel::Say, "ok"), L(EChannel::Current, "/g hello")}, false, {O(V("ok", 2)), O(V("/g hello", 0))});
    CHECK(refused(p) && p.Error.find("Line 2") == 0);
    // The origins must match the lines.
    CHECK(refused(Build({L(EChannel::Say, "a"), L(EChannel::Say, "b")}, false, {O(V("a", 1))})));
}

static Placeholders::Context PreviewCtx()
{
    Placeholders::Context c;
    c.Year = 2026; c.Month = 11; c.Day = 23; c.Hour = 9; c.Minute = 5;
    c.Random = [](long long aMin, long long) { return aMin; };
    c.MapName = [](std::string& aName, std::string&) { aName = "Lion's Arch"; return true; };
    return c;
}

static void TestPrepare()
{
    // Placeholders are filled in, then the plan is built from what's really typed.
    Plan p = Prepare({L(EChannel::Map, "LF1M at {map}"), L(EChannel::Whisper, "roll {random 3 9}", "Some Name")}, false, PreviewCtx());
    CHECK(p.Error.empty() && p.LinesToSend == 2);
    std::vector<Typed> t = Describe(p, {L(EChannel::Map, "x"), L(EChannel::Whisper, "y", "z")});
    CHECK(t.size() == 2);
    CHECK(t[0].Line == 0 && t[0].Channel == EChannel::Map && t[0].Text == "/m LF1M at Lion's Arch" && !t[0].Tab && t[0].AfterTab.empty());
    CHECK(t[1].Line == 1 && t[1].Channel == EChannel::Whisper && t[1].Text == "/w Some Name" && t[1].Tab && t[1].AfterTab == "roll 3");

    // A placeholder that can't be filled in: nothing to show, and the error names the line.
    Placeholders::Context bad = PreviewCtx();
    bad.MapName = [](std::string&, std::string& aErr) { aErr = "not yet."; return false; };
    p = Prepare({L(EChannel::Say, "ok"), L(EChannel::Say, "at {map}")}, false, bad);
    CHECK(p.Steps.empty() && p.Error.find("Line 2: ") == 0);
    CHECK(Describe(p, {}).empty());

    // A value that would start a command is refused, the same as on a real send (SEC-013).
    Placeholders::Context slash = PreviewCtx();
    slash.MapName = [](std::string& aName, std::string&) { aName = "/g hi"; return true; };
    p = Prepare({L(EChannel::Current, "{map}")}, false, slash);
    CHECK(!p.Error.empty() && p.Steps.empty());

    // Too long once filled in.
    Placeholders::Context longName = PreviewCtx();
    longName.MapName = [](std::string& aName, std::string&) { aName = std::string(200, 'a'); return true; };
    p = Prepare({L(EChannel::Say, "at {map}")}, false, longName);
    CHECK(p.Error.find("Line 1: ") == 0);

    // Stopping.
    p = Prepare({L(EChannel::Say, "hi")}, false, PreviewCtx(), [] { return true; });
    CHECK(!p.Error.empty() && p.Steps.empty());

    // Squad broadcast (as commander), empty lines skipped, and no placeholders at all.
    const std::vector<Line> lines{L(EChannel::Squad, "stack", "", true), L(EChannel::Say, ""), L(EChannel::Current, "/wave")};
    p = Prepare(lines, true, PreviewCtx());
    t = Describe(p, lines);
    CHECK(t.size() == 2);
    CHECK(t[0].Broadcast && t[0].Channel == EChannel::Squad && t[0].Text == "stack");
    CHECK(t[1].Line == 2 && !t[1].Broadcast && t[1].Text == "/wave");
    // Not commander: the same line goes to squad chat.
    t = Describe(Prepare(lines, false, PreviewCtx()), lines);
    CHECK(!t.empty() && !t[0].Broadcast && t[0].Text == "/d stack");
}

void RunSendPlanTests()
{
    TestSimpleLine();
    TestTwoLinesPause();
    TestWhisper();
    TestBroadcast();
    TestEmptyAndTooLong();
    TestControlCharacters();
    TestPlaceholderCommand();
    TestPrepare();
}
