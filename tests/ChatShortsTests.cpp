// Tests for src/ChatShorts.* and Exchange::FromChatShorts: macros from Blish HUD's Chat Shorts
// module (docs/design.md, "Import / export", "From Blish HUD").

#include "TestCheck.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "ChatLine.h"
#include "ChatShorts.h"
#include "Exchange.h"
#include "Placeholders.h"

using nlohmann::json;
using namespace MacroTree;
namespace fs = std::filesystem;

namespace
{
    json Oid(const std::string& aHex) { return json{{"$oid", aHex}}; }
    json Ref(const std::string& aHex) { return json{{"$id", Oid(aHex)}, {"$ref", "chat_lines"}}; }
    std::string Id(int n)
    {
        std::string s = std::to_string(n);
        return std::string(24 - s.size(), '0') + s;
    }

    void TestFields()
    {
        EChannel c = EChannel::Current;
        CHECK(ChatShorts::ChannelOf("Say", c) && c == EChannel::Say);
        CHECK(ChatShorts::ChannelOf("guild3", c) && c == EChannel::Guild3);
        CHECK(ChatShorts::ChannelOf("Current", c) && c == EChannel::Current);
        CHECK(ChatShorts::ChannelOf(15, c) && c == EChannel::Guild6);
        CHECK(ChatShorts::ChannelOf(8, c) && c == EChannel::Whisper);
        CHECK(!ChatShorts::ChannelOf(16, c));
        CHECK(!ChatShorts::ChannelOf(-1, c));
        CHECK(!ChatShorts::ChannelOf("Shout", c));
        CHECK(!ChatShorts::ChannelOf(json(), c));

        CHECK(ChatShorts::GameModesOf("PvE, WvW, PvP") == GM_All);
        CHECK(ChatShorts::GameModesOf("WvW") == GM_WvW);
        CHECK(ChatShorts::GameModesOf("PvE, WvW") == (GM_PvE | GM_WvW));
        CHECK(ChatShorts::GameModesOf(" pvp ") == GM_PvP);
        CHECK(ChatShorts::GameModesOf("None") == GM_All); // none = every mode in Chat Shorts
        CHECK(ChatShorts::GameModesOf(0) == GM_All);
        CHECK(ChatShorts::GameModesOf(6) == (GM_WvW | GM_PvP));
        CHECK(ChatShorts::GameModesOf(json()) == GM_All);

        CHECK(ChatShorts::HotkeyOf("{}").Empty());
        CHECK(ChatShorts::HotkeyOf(json()).Empty());
        CHECK(ChatShorts::HotkeyOf("not json").Empty());
        const Hotkey::Combo k = ChatShorts::HotkeyOf(R"({"PrimaryKey":112,"ModifierKeys":5})"); // Ctrl+Shift+F1
        CHECK(k.Key == 0x70 && k.Ctrl && !k.Alt && k.Shift);
        CHECK(ChatShorts::HotkeyOf(R"({"PrimaryKey":65})").Key == 'A');
        CHECK(ChatShorts::HotkeyOf(R"({"PrimaryKey":999})").Empty());
        CHECK(ChatShorts::HotkeyOf(R"({"PrimaryKey":"F1"})").Empty());

        std::vector<std::string> missing;
        CHECK(ChatShorts::FixPlaceholders("Blish {blish} at {time}", missing) == "Blish {nexus} at {time}");
        CHECK(missing.empty());
        CHECK(ChatShorts::FixPlaceholders("{wp} {WP} {poi} {area} {random 1 6}", missing) == "{wp} {WP} {poi} {area} {random 1 6}");
        CHECK(missing.size() == 3);
        missing.clear();
        CHECK(ChatShorts::FixPlaceholders("{ {blish}} {x", missing) == "{ {nexus}} {x");
        CHECK(ChatShorts::FixPlaceholders("}{blish", missing) == "}{blish");

        CHECK(ChatShorts::IsDbFileName("macros.db"));
        CHECK(ChatShorts::IsDbFileName("Alex.DB"));
        CHECK(!ChatShorts::IsDbFileName("macros-log.db"));
        CHECK(!ChatShorts::IsDbFileName(".db"));
        CHECK(!ChatShorts::IsDbFileName("macros.json"));
        CHECK(ChatShorts::LogFileOf("C:\\x\\macros.db") == fs::path("C:\\x\\macros-log.db"));
        CHECK(ChatShorts::DefaultFile("C:\\Users\\A\\Documents") ==
              fs::path("C:\\Users\\A\\Documents\\Guild Wars 2\\addons\\blishhud\\chat_shorts\\macros.db"));
        CHECK(ChatShorts::DefaultFile("").empty());
    }

    void TestBuild()
    {
        std::map<std::string, std::vector<json>> cols;
        auto& lines = cols["chat_lines"];
        lines.push_back({{"_id", Oid(Id(1))}, {"channel", "Say"}, {"whisper_to", nullptr}, {"squad_broadcast", false}, {"message", "hello {blish}"}});
        lines.push_back({{"_id", Oid(Id(2))}, {"channel", "Whisper"}, {"whisper_to", "Some Name"}, {"squad_broadcast", true}, {"message", "psst"}});
        lines.push_back({{"_id", Oid(Id(3))}, {"channel", "Squad"}, {"whisper_to", "ignored"}, {"squad_broadcast", true}, {"message", "{wp} here"}});
        lines.push_back({{"_id", Oid(Id(4))}, {"channel", "Shout"}, {"message", "unknown channel"}});
        lines.push_back({{"_id", Oid(Id(5))}, {"channel", "Map"}, {"message", "an orphan, in no macro"}});
        lines.push_back({{"_id", Oid(Id(6))}, {"channel", "Guild2"}, {"message", "bad \xFF byte"}});
        auto& macros = cols["chat_macros"];
        macros.push_back({{"_id", Oid(Id(100))}, {"title", "zeta"}, {"lines", json::array({Ref(Id(1)), Ref(Id(2))})},
                          {"game_mode", "WvW"}, {"map_ids", json::array()}, {"key_binding", R"({"PrimaryKey":112,"ModifierKeys":1})"},
                          {"voice_commands", json::array({"hello"})}, {"link_file", "C:\\x.txt"}});
        macros.push_back({{"_id", Oid(Id(101))}, {"title", "Alpha"}, {"lines", json::array({Ref(Id(3)), Ref(Id(4)), Ref(Id(77))})},
                          {"game_mode", "PvE"}, {"map_ids", json::array({15, 15, 50, -3, "x"})}, {"key_binding", "{}"}});
        macros.push_back({{"_id", Oid(Id(102))}, {"title", ""}, {"lines", json::array({Ref(Id(6))})}});
        macros.push_back({{"_id", Oid(Id(103))}, {"title", "Empty"}, {"lines", json::array()}});
        macros.push_back({{"_id", Oid(Id(104))}, {"title", "alpha"}, {"lines", json::array({Ref(Id(1))})}});
        macros.push_back("not an object");

        Tree t;
        std::vector<std::string> warnings;
        CHECK(ChatShorts::Build(cols, t, warnings) == 4);
        const auto& top = t.Root().Children;
        CHECK(top.size() == 4);
        if (top.size() != 4) return;
        // A-Z, ignoring case; the second "alpha" becomes "alpha (2)".
        CHECK(top[0]->Name == "Alpha");
        CHECK(top[1]->Name == "alpha (2)");
        const Node* alphaNode = top[0].get();
        CHECK(top[2]->Name == "Imported macro");
        CHECK(top[3]->Name == "zeta");

        const Node& zeta = *top[3];
        CHECK(zeta.Lines.size() == 2);
        CHECK(zeta.Lines[0].Channel == EChannel::Say && zeta.Lines[0].Message == "hello {nexus}");
        CHECK(zeta.Lines[1].Channel == EChannel::Whisper && zeta.Lines[1].WhisperTo == "Some Name" && !zeta.Lines[1].SquadBroadcast);
        CHECK(zeta.GameModes == GM_WvW && zeta.MapIds.empty());
        CHECK(zeta.Hotkey.Key == 0x70 && zeta.Hotkey.Ctrl);
        CHECK(zeta.LinkFile.empty());

        const Node& alpha = *alphaNode;
        CHECK(alpha.Lines.size() == 1); // the Shout line and the missing one are left out
        CHECK(alpha.Lines[0].Channel == EChannel::Squad && alpha.Lines[0].SquadBroadcast && alpha.Lines[0].WhisperTo.empty());
        CHECK(alpha.MapIds == std::vector<int>({15, 50}));
        CHECK(alpha.GameModes == GM_All); // maps win over game modes, as in Chat Shorts
        CHECK(alpha.Hotkey.Empty());

        const Node& unnamed = *top[2];
        CHECK(unnamed.Lines[0].Message.find('\xFF') == std::string::npos); // repaired

        auto has = [&](const char* aPart) {
            for (const std::string& w : warnings)
                if (w.find(aPart) != std::string::npos) return true;
            return false;
        };
        CHECK(has("Empty"));          // no lines, left out
        CHECK(has("{wp}"));           // not in this addon
        CHECK(has("aren't in the file")); // the missing line
        CHECK(has("doesn't know"));   // the Shout line
        CHECK(has("text file"));      // a link file
        CHECK(has("voice commands"));
        CHECK(has("UTF-8"));

        // Too many lines.
        std::map<std::string, std::vector<json>> many;
        json refs = json::array();
        for (int i = 0; i < 60; i++)
        {
            many["chat_lines"].push_back({{"_id", Oid(Id(i))}, {"channel", "Say"}, {"message", "x"}});
            refs.push_back(Ref(Id(i)));
        }
        many["chat_macros"].push_back({{"_id", Oid(Id(500))}, {"title", "Long"}, {"lines", refs}});
        Tree t2;
        std::vector<std::string> w2;
        CHECK(ChatShorts::Build(many, t2, w2) == 1);
        CHECK(!t2.Root().Children.empty() && t2.Root().Children[0]->Lines.size() == kMaxLines);
        CHECK(!w2.empty());

        // Cleaned as any file is (Tree::FromJson): control characters and long recipients.
        std::map<std::string, std::vector<json>> odd;
        odd["chat_lines"].push_back({{"_id", Oid(Id(1))}, {"channel", "Say"}, {"message", "a\tb\nc"}});
        odd["chat_lines"].push_back({{"_id", Oid(Id(2))}, {"channel", "Whisper"}, {"whisper_to", std::string(40, 'n') + "\r"}, {"message", "hi"}});
        odd["chat_lines"].push_back({{"_id", Oid(Id(3))}, {"channel", "Shout"}, {"message", "x"}});
        odd["chat_macros"].push_back({{"_id", Oid(Id(10))}, {"title", "Odd"}, {"lines", json::array({Ref(Id(1)), Ref(Id(2))})}});
        odd["chat_macros"].push_back({{"_id", Oid(Id(11))}, {"title", "Unknown only"}, {"lines", json::array({Ref(Id(3)), Ref(Id(99))})}});
        // A long title used twice: shortened a whole character at a time, keeping the "é".
        const std::string longTitle = std::string(94, 'a') + "\xC3\xA9" "bb"; // 98 bytes
        odd["chat_macros"].push_back({{"_id", Oid(Id(12))}, {"title", longTitle}, {"lines", json::array({Ref(Id(1))})}});
        odd["chat_macros"].push_back({{"_id", Oid(Id(13))}, {"title", longTitle}, {"lines", json::array({Ref(Id(1))})}});
        Tree t4;
        std::vector<std::string> w4;
        CHECK(ChatShorts::Build(odd, t4, w4) == 3);
        const Node* oddNode = nullptr;
        bool shortened = false;
        for (const auto& c : t4.Root().Children)
        {
            if (c->Name == "Odd") oddNode = c.get();
            if (c->Name == std::string(94, 'a') + "\xC3\xA9 (2)") shortened = true;
        }
        CHECK(shortened);
        CHECK(oddNode && oddNode->Lines.size() == 2);
        if (oddNode && oddNode->Lines.size() == 2)
        {
            CHECK(oddNode->Lines[0].Message == "a b c");
            CHECK(oddNode->Lines[1].WhisperTo.size() == ChatLine::kMaxRecipientLength);
            CHECK(oddNode->Lines[1].WhisperTo.find('\r') == std::string::npos);
        }
        auto has4 = [&](const char* aPart) {
            for (const std::string& w : w4)
                if (w.find(aPart) != std::string::npos) return true;
            return false;
        };
        CHECK(has4("was shortened"));
        CHECK(has4("none of its lines could be read (1 aren't in the file, 1 are for a channel"));

        // Nothing at all.
        Tree t3;
        std::vector<std::string> w3;
        CHECK(ChatShorts::Build({}, t3, w3) == 0);
    }

    // The sample: a real Chat Shorts macros.db, its text replaced by filler (tests/samples).
    void TestSampleFile()
    {
        const fs::path sample = fs::path("tests") / "samples" / "chatshorts_macros.db";
        Exchange::Package p = Exchange::FromChatShorts(sample);
        CHECK(p.Ok());
        CHECK(p.Warnings.empty());
        CHECK(p.Source == "chatshorts_macros.db");
        const auto& top = p.Items.Root().Children;
        CHECK(top.size() == 11);
        if (!p.Ok()) std::printf("  sample error: %s\n", p.Error.c_str());
        if (top.size() != 11) return;
        size_t lines = 0;
        for (const auto& m : top) lines += m->Lines.size();
        CHECK(lines == 27); // 21 more lines in the file belong to no macro
        // Titles are "Macro 01 ..." to "Macro 11 ...", in the file's order, so A-Z keeps it.
        CHECK(top[0]->Name.rfind("Macro 01", 0) == 0);
        CHECK(top[10]->Name.rfind("Macro 11", 0) == 0);
        CHECK(top[0]->Lines.size() == 1 && top[0]->Lines[0].Channel == EChannel::Guild2 && top[0]->GameModes == GM_All);
        CHECK(top[1]->Lines[0].Channel == EChannel::Squad && top[1]->GameModes == GM_WvW);
        CHECK(top[2]->Lines[0].Channel == EChannel::Map);
        CHECK(top[3]->Lines.size() == 5 && top[3]->GameModes == (GM_PvE | GM_WvW));
        const EChannel guilds[] = {EChannel::Guild1, EChannel::Guild3, EChannel::Guild4, EChannel::Guild5, EChannel::Guild6};
        for (size_t i = 0; i < 5; i++) CHECK(top[3]->Lines[i].Channel == guilds[i]);
        CHECK(top[10]->Lines[0].Channel == EChannel::Guild6);
        for (const auto& m : top) CHECK(m->Hotkey.Empty());

        // Put into one "Imported" folder.
        Node* folder = Exchange::WrapInFolder(p, Exchange::ChatShortsFolderName());
        CHECK(folder && folder->Name == "Imported" && folder->Children.size() == 11);

        // The chat_shorts folder itself, a log file that isn't empty, a missing file, not a database.
        const fs::path dir = fs::temp_directory_path() / "chatmsg_chatshorts_tests";
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        fs::copy_file(sample, dir / "macros.db", ec);
        CHECK(!ec);
        Exchange::Package fromDir = Exchange::FromChatShorts(dir);
        CHECK(fromDir.Ok() && fromDir.Items.Root().Children.size() == 11 && fromDir.Warnings.empty());
        { std::ofstream(dir / "macros-log.db", std::ios::binary) << "x"; }
        Exchange::Package withLog = Exchange::FromChatShorts(dir / "macros.db");
        CHECK(withLog.Ok() && withLog.Warnings.size() == 1);
        CHECK(!Exchange::FromChatShorts(dir / "nothing.db").Ok());
        { std::ofstream(dir / "text.db", std::ios::binary) << "hello"; }
        CHECK(!Exchange::FromChatShorts(dir / "text.db").Ok());
        fs::remove_all(dir, ec);
    }
}

void RunChatShortsTests()
{
    TestFields();
    TestBuild();
    TestSampleFile();
}
