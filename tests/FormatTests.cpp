// Standalone tests for the saved-file rules (docs/design.md, "Saved files"): every format's sample
// file still reads, fields from a newer version are kept and written back, a newer library is
// read-only, an unknown channel is never sent, and the trash bin's times are UTC.

#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Exchange.h"
#include "MacroTree.h"
#include "SendPlan.h"
#include "TestCheck.h"

using namespace MacroTree;
using nlohmann::json;

namespace
{
    // Run from the repository root (tools\run_tests.ps1).
    std::string ReadSample(const char* aName)
    {
        std::ifstream f(std::string("tests\\samples\\") + aName, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    const Node* Child(const Node& aParent, const char* aName)
    {
        for (const auto& c : aParent.Children)
            if (c->Name == aName) return c.get();
        return nullptr;
    }

    // Format 1 (the first release's): everything in it reads as it should.
    void TestFormat1Sample()
    {
        const std::string text = ReadSample("macros_format1.json");
        CHECK(!text.empty());
        if (text.empty()) return;
        Tree t;
        std::vector<std::string> w;
        CHECK(t.FromJson(json::parse(text), &w));
        CHECK(w.empty());
        CHECK(t.FormatRead() == 1 && !t.FromNewerVersion() && t.SavedBy() == "2026.11.26.0000.Dev");

        const Node* raids = Child(t.Root(), "Raids");
        CHECK(raids && raids->IsFolder() && !raids->Expanded && raids->GameModes == GM_PvE);
        CHECK(raids && raids->MapIds == std::vector<int>({1062}));
        const Node* ready = raids ? Child(*raids, "Ready check") : nullptr;
        CHECK(ready && ready->IsMacro() && ready->Favourite && ready->CooldownSec == 30);
        CHECK(ready && ready->Lines.size() == 3);
        if (ready && ready->Lines.size() == 3)
        {
            CHECK(ready->Lines[0].Channel == EChannel::Squad && ready->Lines[0].SquadBroadcast);
            CHECK(ready->Lines[1].Channel == EChannel::Whisper && ready->Lines[1].WhisperTo == "Some Name.1234");
            CHECK(ready->Lines[2].Channel == EChannel::Party && ready->Lines[2].Message == "{time}");
            CHECK(!ready->Hotkey.Empty());
        }
        const Node* hello = Child(t.Root(), "Hello");
        CHECK(hello && hello->GameModes == GM_All && hello->MapIds.empty());

        // Written back: the current format, who saved it, what it is, and no empty filters.
        const json out = t.ToJson();
        CHECK(out.value("format", 0) == kFormatVersion);
        CHECK(out.value("saved_by", std::string()).size() > 0 && out.value("kind", std::string()) == "library");
        const std::string dumped = out.dump();
        CHECK(dumped.find("\"cooldown_sec\":30") != std::string::npos);
        CHECK(dumped.find("\"cooldown\":") == std::string::npos);
        // Only the Raids folder has filters.
        size_t modes = 0, maps = 0;
        for (size_t at = dumped.find("\"game_modes\""); at != std::string::npos; at = dumped.find("\"game_modes\"", at + 1)) modes++;
        for (size_t at = dumped.find("\"map_ids\""); at != std::string::npos; at = dumped.find("\"map_ids\"", at + 1)) maps++;
        CHECK(modes == 1 && maps == 1);

        // And reads back the same.
        Tree again;
        CHECK(again.FromJson(out) && again.ToJson()["root"] == out["root"]);
    }

    // What a newer version wrote: fields this build doesn't know, at every level, and a channel.
    const char* kNewer = R"({
        "format": 1, "saved_by": "2099.01.01.0000.Rel", "kind": "library", "future_top": {"a": [1, 2]},
        "root": {"type": "folder", "id": "0000000000000001", "name": "", "children": [
            {"type": "folder", "id": "0000000000000002", "name": "F", "colour": "red", "children": [
                {"type": "macro", "id": "0000000000000003", "name": "M", "pinned_at": 5, "lines": [
                    {"channel": "say", "message": "hi", "delay_ms": 250},
                    {"channel": "guild_hall", "message": "secret", "whisper_to": "X", "squad_broadcast": true}
                ]}
            ]}
        ]}
    })";

    void TestUnknownFieldsKept()
    {
        Tree t;
        std::vector<std::string> w;
        CHECK(t.FromJson(json::parse(kNewer), &w));
        CHECK(w.empty());
        CHECK(t.SavedBy() == "2099.01.01.0000.Rel");
        const Node* f = Child(t.Root(), "F");
        const Node* m = f ? Child(*f, "M") : nullptr;
        CHECK(f && f->Extra && f->Extra->value("colour", std::string()) == "red");
        CHECK(m && m->Extra && m->Extra->value("pinned_at", 0) == 5);
        CHECK(m && m->Lines.size() == 2);
        if (!m || m->Lines.size() != 2) return;
        CHECK(m->Lines[0].Extra && m->Lines[0].Extra->value("delay_ms", 0) == 250 && m->Lines[0].UnknownChannel.empty());
        CHECK(m->Lines[1].UnknownChannel == "guild_hall" && m->Lines[1].Message == "secret");
        CHECK(HasUnknownChannel(*m) && HasNewerData(*m) && !HasUnknownChannel(*f));

        // Written back as they were.
        const json out = t.ToJson();
        CHECK(out["future_top"] == json::parse(R"({"a":[1,2]})"));
        const json& jf = out["root"]["children"][0];
        CHECK(jf.value("colour", std::string()) == "red");
        const json& jm = jf["children"][0];
        CHECK(jm.value("pinned_at", 0) == 5);
        CHECK(jm["lines"][0].value("delay_ms", 0) == 250 && jm["lines"][0].value("channel", std::string()) == "say");
        CHECK(jm["lines"][1].value("channel", std::string()) == "guild_hall");
        CHECK(jm["lines"][1].value("whisper_to", std::string()) == "X" && jm["lines"][1].value("squad_broadcast", false));
        // A known field is never written twice or overwritten by a kept one.
        Tree again;
        CHECK(again.FromJson(out) && again.ToJson() == out);
    }

    void TestUnknownChannelNeverSent()
    {
        Tree t;
        CHECK(t.FromJson(json::parse(kNewer)));
        const Node* f = Child(t.Root(), "F");
        const Node* m = f ? Child(*f, "M") : nullptr;
        if (!m) { CHECK(false); return; }
        const SendPlan::Plan p = SendPlan::Build(m->Lines, false);
        CHECK(!p.Error.empty() && p.Steps.empty());
        CHECK(p.Error.find("guild_hall") != std::string::npos);
        const SendPlan::Plan q = SendPlan::Prepare(m->Lines, false, Placeholders::Context{});
        CHECK(!q.Error.empty() && q.Steps.empty());
        // Even with an empty message (normally skipped), it's refused.
        std::vector<Line> lines(1);
        lines[0].UnknownChannel = "later";
        CHECK(!SendPlan::Build(lines, false).Error.empty());
        // A known line on its own still sends.
        std::vector<Line> ok(1, m->Lines[0]);
        CHECK(SendPlan::Build(ok, false).Error.empty());
    }

    void TestChannelKeyCleaned()
    {
        // A long key is cut, and control characters can't reach the screen or the file.
        const std::string longKey(100, 'k');
        const json j = json::parse(std::string(R"({"format":1,"root":{"children":[{"type":"macro","id":"00000000000000aa","name":"M","lines":[{"channel":")") +
                                   longKey + R"(","message":"a"},{"channel":"a\u0001b","message":"b"},{"channel":5,"message":"c"}]}]}})");
        Tree t;
        CHECK(t.FromJson(j));
        const Node* m = Child(t.Root(), "M");
        CHECK(m && m->Lines.size() == 3);
        if (!m || m->Lines.size() != 3) return;
        CHECK(m->Lines[0].UnknownChannel.size() == kMaxChannelKeyLen);
        CHECK(m->Lines[1].UnknownChannel == "a b");
        // Not a string: there's no channel to keep (as before: Current).
        CHECK(m->Lines[2].UnknownChannel.empty() && m->Lines[2].Channel == EChannel::Current);
    }

    void TestExtraLimits()
    {
        // Too many unknown keys: the rest are dropped with a warning.
        json j = json::parse(R"({"format":1,"root":{"children":[{"type":"macro","id":"00000000000000aa","name":"M"}]}})");
        json& m = j["root"]["children"][0];
        for (size_t i = 0; i < kMaxExtraKeys + 10; i++) m["x" + std::to_string(i)] = 1;
        // Too deep and too big values are dropped.
        json deep = 1;
        for (int i = 0; i < kMaxExtraDepth + 4; i++) deep = json::array({deep});
        j["deep"] = deep;
        json big = json::array();
        for (size_t i = 0; i < kMaxExtraValues + 10; i++) big.push_back(i);
        j["big"] = big;
        j["fine"] = "ok";
        Tree t;
        std::vector<std::string> w;
        CHECK(t.FromJson(j, &w));
        CHECK(!w.empty());
        const Node* n = Child(t.Root(), "M");
        CHECK(n && n->Extra && n->Extra->size() == kMaxExtraKeys);
        CHECK(t.Root().Extra && t.Root().Extra->contains("fine"));
        CHECK(t.Root().Extra && !t.Root().Extra->contains("deep") && !t.Root().Extra->contains("big"));
    }

    void TestNewerFormat()
    {
        json j = json::parse(kNewer);
        j["format"] = 3;
        Tree t;
        std::vector<std::string> w;
        CHECK(t.FromJson(j, &w));
        CHECK(w.empty() && t.FromNewerVersion() && t.Format() == 3 && t.FormatRead() == 3);
        // Written (an export, a backup copy) with its own number, never ours.
        CHECK(t.ToJson().value("format", 0) == 3);

        // An older or missing format reads as 1.
        for (const char* f : {"", R"("format":0,)", R"("format":-4,)"})
        {
            Tree o;
            CHECK(o.FromJson(json::parse(std::string("{") + f + R"("root":{"children":[]}})")));
            CHECK(o.FormatRead() == 1 && !o.FromNewerVersion());
        }

        // saved_by: cut, with control characters made spaces.
        json s = json::parse(R"({"format":1,"root":{"children":[]}})");
        s["saved_by"] = std::string("v\n1") + std::string(200, 'x');
        Tree st;
        CHECK(st.FromJson(s));
        CHECK(st.SavedBy().size() == 64 && st.SavedBy().substr(0, 3) == "v 1");
    }

    void TestUtcStamp()
    {
        const std::time_t when = 1795269780; // 2026-11-21T14:03:00Z
        CHECK(UtcStamp(when) == "2026-11-21T14:03:00Z");
        std::time_t back = 0;
        CHECK(ParseUtcStamp("2026-11-21T14:03:00Z", back) && back == when);
        for (const char* bad : {"", "2026-11-21 14:03", "2026-11-21T14:03:00", "2026-13-21T14:03:00Z",
                                "2026-11-21T14:03:00Zx", "2026-11-21T25:03:00Z", "abcd-11-21T14:03:00Z"})
            CHECK(!ParseUtcStamp(bad, back));

        // The trash bin keeps the time it's given, and saves and reads it as it was.
        Tree t;
        CHECK(t.FromJson(json::parse(R"({"format":1,"root":{"children":[{"type":"macro","id":"00000000000000aa","name":"M"}]}})")));
        Node* m = t.Root().Children[0].get();
        CHECK(t.MoveToTrash(*m, UtcStamp(when)).Ok);
        Tree again;
        CHECK(again.FromJson(t.ToJson()));
        CHECK(again.Trash().size() == 1 && again.Trash()[0].DeletedAt == "2026-11-21T14:03:00Z");
    }

    void TestExports()
    {
        Tree t;
        CHECK(t.FromJson(json::parse(kNewer)));
        const Node* f = Child(t.Root(), "F");
        if (!f) { CHECK(false); return; }
        const json e = Exchange::ExportJson({f});
        CHECK(e.value("kind", std::string()) == "export" && e.value("format", 0) == kFormatVersion);
        CHECK(!e.value("saved_by", std::string()).empty());
        // Kept fields go along.
        CHECK(e.dump().find("guild_hall") != std::string::npos && e.dump().find("pinned_at") != std::string::npos);

        // An export from a newer version still imports, with a warning, and is marked in the preview.
        json newer = e;
        newer["format"] = 2;
        newer["saved_by"] = "2099.01.01.0000.Rel";
        const Exchange::Package p = Exchange::FromJsonText(newer.dump());
        CHECK(p.Ok() && p.Items.FromNewerVersion());
        bool warned = false;
        for (const std::string& s : p.Warnings)
            if (s.find("2099.01.01.0000.Rel") != std::string::npos) warned = true;
        CHECK(warned);

        Tree mine;
        CHECK(mine.FromJson(json::parse(R"({"format":1,"root":{"children":[]}})")));
        const Exchange::Options o;
        const Exchange::Plan plan = Exchange::BuildPlan(mine, mine.Root(), p.Items, o);
        // The folder (its own "colour") and the macro (unknown fields and channel).
        CHECK(plan.FromNewer == 2);
        CHECK(plan.Items.size() == 1 && plan.Items[0].FromNewer && plan.Items[0].Children.size() == 1 &&
              plan.Items[0].Children[0].FromNewer);
        Exchange::Apply(mine, mine.Root(), plan, o);
        const Node* nf = Child(mine.Root(), "F");
        const Node* nm = nf ? Child(*nf, "M") : nullptr;
        CHECK(nf && nf->Extra && nm && nm->Extra && nm->Lines.size() == 2 && nm->Lines[1].UnknownChannel == "guild_hall");

        // A current export with nothing newer isn't marked.
        const Exchange::Package plain = Exchange::FromJsonText(ReadSample("macros_format1.json"));
        CHECK(plain.Ok() && !plain.Items.FromNewerVersion());
        Tree mine2;
        CHECK(mine2.FromJson(json::parse(R"({"format":1,"root":{"children":[]}})")));
        CHECK(Exchange::BuildPlan(mine2, mine2.Root(), plain.Items, o).FromNewer == 0);
    }
}

void RunFormatTests()
{
    TestFormat1Sample();
    TestUnknownFieldsKept();
    TestUnknownChannelNeverSent();
    TestChannelKeyCleaned();
    TestExtraLimits();
    TestNewerFormat();
    TestUtcStamp();
    TestExports();
}
