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
    TestTypedAndLength();
    TestColors();
}
