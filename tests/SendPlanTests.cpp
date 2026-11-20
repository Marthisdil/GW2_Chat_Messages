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

void RunSendPlanTests()
{
    TestSimpleLine();
    TestTwoLinesPause();
    TestWhisper();
    TestBroadcast();
    TestEmptyAndTooLong();
}
