// Standalone tests for src/ChatLine.cpp (raw edit format, typed text, lengths).

#include <string>

#include "ChatLine.h"
#include "TestCheck.h"

using namespace ChatLine;
using MacroTree::EChannel;
using MacroTree::Line;

static void TestParseBasic()
{
    Line l = ParseLine("hello there");
    CHECK(l.Channel == EChannel::Current && l.Message == "hello there");

    l = ParseLine("/s hello");
    CHECK(l.Channel == EChannel::Say && l.Message == "hello");

    l = ParseLine("/SAY  two spaces"); // only one space is the separator
    CHECK(l.Channel == EChannel::Say && l.Message == " two spaces");

    l = ParseLine("/s");
    CHECK(l.Channel == EChannel::Say && l.Message.empty());

    l = ParseLine("/wave");   // a game command, not a channel
    CHECK(l.Channel == EChannel::Current && l.Message == "/wave");

    l = ParseLine("/sit down"); // "/sit" isn't "/s"
    CHECK(l.Channel == EChannel::Current && l.Message == "/sit down");

    l = ParseLine("trailing spaces   \r");
    CHECK(l.Message == "trailing spaces");
}

static void TestParseChannels()
{
    CHECK(ParseLine("/m x").Channel == EChannel::Map);
    CHECK(ParseLine("/p x").Channel == EChannel::Party);
    CHECK(ParseLine("/t x").Channel == EChannel::Team);
    CHECK(ParseLine("/r x").Channel == EChannel::Reply);
    CHECK(ParseLine("/e x").Channel == EChannel::Emote);
    CHECK(ParseLine("/g x").Channel == EChannel::Guild);
    CHECK(ParseLine("/g1 x").Channel == EChannel::Guild1);
    CHECK(ParseLine("/g6 x").Channel == EChannel::Guild6);
    CHECK(ParseLine("/guild4 x").Channel == EChannel::Guild4);
    CHECK(ParseLine("/g7 x").Channel == EChannel::Current); // no guild 7
    // German, French, Spanish
    CHECK(ParseLine("/k x").Channel == EChannel::Map);
    CHECK(ParseLine("/gruppe x").Channel == EChannel::Party);
    CHECK(ParseLine("/escouade x").Channel == EChannel::Squad);
    CHECK(ParseLine("/escuadr\xC3\xB3n x").Channel == EChannel::Squad);
    CHECK(ParseLine("/\xC3\xA9quipe x").Channel == EChannel::Team);
    CHECK(ParseLine("/clan2 x").Channel == EChannel::Guild2);
    CHECK(ParseLine("/gilde3 x").Channel == EChannel::Guild3);
    CHECK(ParseLine("/fl\xC3\xBCstern [A] x").Channel == EChannel::Whisper);
}

static void TestWhisperAndSquad()
{
    Line l = ParseLine("/w [Some Name.1234] hi there");
    CHECK(l.Channel == EChannel::Whisper && l.WhisperTo == "Some Name.1234" && l.Message == "hi there");

    l = ParseLine("/w[Name] hi");
    CHECK(l.WhisperTo == "Name" && l.Message == "hi");

    l = ParseLine("/w [Name] see [this]"); // only the first ] closes the name
    CHECK(l.WhisperTo == "Name" && l.Message == "see [this]");

    l = ParseLine("/w no recipient");
    CHECK(l.Channel == EChannel::Whisper && l.WhisperTo.empty() && l.Message == "no recipient");

    l = ParseLine("/d ! Stack on tag");
    CHECK(l.Channel == EChannel::Squad && l.SquadBroadcast && l.Message == "Stack on tag");

    l = ParseLine("/d! go");
    CHECK(l.SquadBroadcast && l.Message == "go");

    l = ParseLine("/d hello!");
    CHECK(!l.SquadBroadcast && l.Message == "hello!");

    l = ParseLine("/s ! not a broadcast");
    CHECK(!l.SquadBroadcast && l.Message == "! not a broadcast");
}

static void TestRoundTrip()
{
    const char* rows[] = {
        "hello",
        "/s hello",
        "/w [A B] hi",
        "/w hi",
        "/d ! go go",
        "/d plain",
        "/g3 guild three",
        "/e waves",
        "/wave",
        "caf\xC3\xA9 \xE2\x9C\x93",
    };
    for (const char* r : rows)
    {
        const Line l = ParseLine(r);
        CHECK(SerializeLine(l) == r);
        const Line again = ParseLine(SerializeLine(l));
        CHECK(again.Channel == l.Channel && again.Message == l.Message &&
              again.WhisperTo == l.WhisperTo && again.SquadBroadcast == l.SquadBroadcast);
    }

    const std::string raw = "/s one\n\n   \n/p two\r\nthree\n";
    const auto lines = ParseRaw(raw);
    CHECK(lines.size() == 3);
    CHECK(lines.size() == 3 && lines[0].Channel == EChannel::Say && lines[1].Channel == EChannel::Party && lines[2].Message == "three");
    CHECK(SerializeRaw(lines) == "/s one\n/p two\nthree");
    CHECK(ParseRaw("").empty());

    // Long forms are written back as short forms.
    CHECK(SerializeLine(ParseLine("/squad ! x")) == "/d ! x");
}

static void TestControlCharacters()
{
    // Every control character becomes one space; nothing else changes.
    CHECK(ControlsToSpaces("a\tb\rc\nd\x1B") == "a b c d ");
    CHECK(ControlsToSpaces(std::string("a\0b\x01\x1F\x7F", 6)) == "a b   ");
    CHECK(ControlsToSpaces("x\xC2\x85y\xC2\x80\xC2\x9F") == "x y  ");             // C1 (NEL, U+0080, U+009F)
    CHECK(ControlsToSpaces("\xC2\xA0 caf\xC3\xA9 \xC2\xA9") == "\xC2\xA0 caf\xC3\xA9 \xC2\xA9"); // NBSP, e acute, (c) kept
    CHECK(ControlsToSpaces("\xE2\x80\x99 \xF0\x9F\x98\x80") == "\xE2\x80\x99 \xF0\x9F\x98\x80"); // 0x80-0x9F inside other characters
    CHECK(ControlsToSpaces("end\xC2") == "end\xC2");                               // a lone lead byte isn't a C1 control
    CHECK(ControlsToSpaces("plain /s text [x] !") == "plain /s text [x] !");

    // Inside a row they're typed as spaces, so they can't add an Enter, Tab or Escape.
    Line l = ParseLine("gg\r/m text");
    CHECK(l.Channel == EChannel::Current && l.Message == "gg /m text");
    l = ParseLine("/s a\tb\x1B" "c\xC2\x85" "d\x1B");
    CHECK(l.Channel == EChannel::Say && l.Message == "a b c d");
    l = ParseLine("/w [Na\tme\r] hi");
    CHECK(l.Channel == EChannel::Whisper && l.WhisperTo == "Na me " && l.Message == "hi");
    CHECK(TypedText(ParseLine("/s x\ry")) == "/s x y");

    // A tab as the separator parses as before.
    l = ParseLine("/s\thello");
    CHECK(l.Channel == EChannel::Say && l.Message == "hello");
    l = ParseLine("/w\t[Name]\thi");
    CHECK(l.Channel == EChannel::Whisper && l.WhisperTo == "Name" && l.Message == "hi");
    l = ParseLine("/d\t!\tgo\t");
    CHECK(l.Channel == EChannel::Squad && l.SquadBroadcast && l.Message == "go");

    // Rows still split on "\n" (and "\r\n"); a lone CR stays inside its row, as a space. A row of
    // only control characters is blank.
    const auto lines = ParseRaw("one\r\ntwo\rthree\n\x1B\x07\r\n/s a\tb");
    CHECK(lines.size() == 3);
    CHECK(lines.size() == 3 && lines[0].Message == "one" && lines[1].Message == "two three" &&
          lines[2].Channel == EChannel::Say && lines[2].Message == "a b");
    CHECK(SerializeRaw(lines) == "one\ntwo three\n/s a b");
}

static void TestTypedAndLength()
{
    Line l;
    l.Channel = EChannel::Say;
    l.Message = "hi";
    CHECK(TypedText(l) == "/s hi");
    l.Channel = EChannel::Whisper;
    l.WhisperTo = "Name";
    CHECK(TypedText(l) == "/w hi"); // recipient goes in its own box
    l.Channel = EChannel::Current;
    CHECK(TypedText(l) == "hi");

    CHECK(Utf16Length("abc") == 3);
    CHECK(Utf16Length("caf\xC3\xA9") == 4);        // é: 2 bytes, 1 unit
    CHECK(Utf16Length("\xE2\x9C\x93") == 1);        // check mark: 3 bytes, 1 unit
    CHECK(Utf16Length("\xF0\x9F\x98\x80") == 2);    // emoji: surrogate pair
}

// SEC-035: CutToUtf16 keeps whole characters, and ParseLine cuts a recipient to kMaxRecipientLength.
static void TestCutRecipient()
{
    CHECK(CutToUtf16("abc", 5) == "abc");
    CHECK(CutToUtf16("abc", 2) == "ab");
    CHECK(CutToUtf16("abc", 0).empty());
    CHECK(CutToUtf16("caf\xC3\xA9!", 4) == "caf\xC3\xA9");             // keeps the whole e-acute
    CHECK(CutToUtf16("a\xF0\x9F\x98\x80", 2) == "a");                   // an emoji is 2 units: not cut in half
    CHECK(CutToUtf16("a\xF0\x9F\x98\x80", 3) == "a\xF0\x9F\x98\x80");
    CHECK(CutToUtf16("\xE2\x9C\x93\xE2\x9C\x93", 1) == "\xE2\x9C\x93"); // a 3-byte character is 1 unit

    const size_t max = kMaxRecipientLength;
    Line l = ParseLine("/w [" + std::string(max, 'n') + "] hi");
    CHECK(l.WhisperTo == std::string(max, 'n') && l.Message == "hi");
    l = ParseLine("/w [Bob" + std::string(300000, 'x') + "] hi");
    CHECK(l.Channel == EChannel::Whisper && Utf16Length(l.WhisperTo) == max && l.WhisperTo.compare(0, 3, "Bob") == 0);
    CHECK(l.Message == "hi");
}

static void TestLengthLevels()
{
    CHECK(LengthLevel(0) == ELengthLevel::Normal);
    CHECK(LengthLevel(149) == ELengthLevel::Normal);
    CHECK(LengthLevel(150) == ELengthLevel::Near);
    CHECK(LengthLevel(179) == ELengthLevel::Near);
    CHECK(LengthLevel(180) == ELengthLevel::Close);
    CHECK(LengthLevel(199) == ELengthLevel::Close);
    CHECK(LengthLevel(200) == ELengthLevel::Over);
}

// RepairUtf8 on a copy: the result, and whether anything was replaced.
static std::string Repaired(std::string s, bool* aChanged = nullptr)
{
    const bool changed = RepairUtf8(s);
    if (aChanged) *aChanged = changed;
    return s;
}

static void TestRepairUtf8()
{
    const std::string bad = "\xEF\xBF\xBD"; // U+FFFD
    bool changed = true;

    // Valid UTF-8 is left exactly as it is, including the edges of each range.
    for (const char* ok : {"", "abc", "a\r\nb\tc", "caf\xC3\xA9", "\xC2\x80", "\xDF\xBF", "\xE0\xA0\x80",
                           "\xED\x9F\xBF", "\xEE\x80\x80", "\xEF\xBF\xBD", "\xF0\x90\x80\x80", "\xF0\x9F\x98\x80",
                           "\xF4\x8F\xBF\xBF"})
    {
        CHECK(Repaired(ok, &changed) == ok && !changed);
    }

    // A Windows-1252 file: each accented letter is one byte, and one U+FFFD.
    CHECK(Repaired("/s K\xE4se f\xFCr dich", &changed) == "/s K" + bad + "se f" + bad + "r dich" && changed);
    // Stray bytes.
    CHECK(Repaired("\xFF") == bad);
    CHECK(Repaired("\x80") == bad);
    CHECK(Repaired("a\xFE\xFF" "b") == "a" + bad + bad + "b");
    // Overlong forms, surrogates and past U+10FFFF: one U+FFFD per byte.
    CHECK(Repaired("\xC0\xAF") == bad + bad);
    CHECK(Repaired("\xE0\x80\xAF") == bad + bad + bad);
    CHECK(Repaired("\xF0\x80\x80\x80") == bad + bad + bad + bad);
    CHECK(Repaired("\xED\xA0\x80") == bad + bad + bad);
    CHECK(Repaired("\xF4\x90\x80\x80") == bad + bad + bad + bad);
    CHECK(Repaired("\xF5\x80") == bad + bad);
    // A cut-off character is one U+FFFD; what follows it is kept.
    CHECK(Repaired("\xE2\x9C") == bad);
    CHECK(Repaired("\xE2\x9C" "x\xE2\x9C\x93") == bad + "x\xE2\x9C\x93");
    CHECK(Repaired("ok \xF0\x9F\x98") == "ok " + bad);

    // The result is valid: repairing it again changes nothing.
    Repaired(Repaired("\xC3(\xE0\xA0\xF8\x88\x80\x80\x80 \xFF"), &changed);
    CHECK(!changed);
}

static void TestColors()
{
    CHECK(ChannelColor(EChannel::Current) == 0xFFFFFF);
    CHECK(ChannelColor(EChannel::Team) != ChannelColor(EChannel::Say));
    CHECK(ChannelColor(EChannel::Whisper) == ChannelColor(EChannel::Reply));
}

void RunChatLineTests()
{
    TestParseBasic();
    TestParseChannels();
    TestWhisperAndSquad();
    TestRoundTrip();
    TestControlCharacters();
    TestTypedAndLength();
    TestColors();
    TestLengthLevels();
    TestCutRecipient();
    TestRepairUtf8();
}
