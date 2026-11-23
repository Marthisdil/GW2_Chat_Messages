// Standalone tests for src/LinkSync.cpp (macros kept in step with a .txt file).

#include <string>
#include <vector>

#include "ChatLine.h"
#include "LinkSync.h"
#include "TestCheck.h"

using namespace LinkSync;
namespace fs = std::filesystem;

namespace
{
    // A fake file: its text and last-write time, and how often it was read.
    struct FakeFile
    {
        bool        Exists = true;
        int64_t     Mtime  = 100;
        std::string Text;
        bool        Fail   = false;
        int         Reads  = 0;

        File View()
        {
            File f;
            f.Exists = Exists;
            f.Mtime  = Mtime;
            f.Read = [this](std::string& aText, std::string& aError) {
                Reads++;
                if (Fail) { aError = "it's locked."; return false; }
                aText = Text;
                return true;
            };
            return f;
        }
    };

    std::vector<MacroTree::Line> Lines(const std::string& raw) { return ChatLine::ParseRaw(raw); }

    void TestPaths()
    {
        CHECK(CheckPath("callouts.txt").empty());
        CHECK(CheckPath("raids\\wing 3.TXT").empty());
        CHECK(CheckPath("C:\\Users\\me\\Documents\\gw2.txt").empty());
        CHECK(!CheckPath("").empty());
        CHECK(!CheckPath("   ").empty());
        CHECK(!CheckPath("notes.md").empty());
        CHECK(!CheckPath(".txt").empty());
        CHECK(!CheckPath("folder\\.txt").empty());
        CHECK(!CheckPath("bad?.txt").empty());
        CHECK(!CheckPath("a\"b.txt").empty());

        const fs::path addon = fs::u8path("C:\\GW2\\addons\\GW2_Chat_Messages");
        CHECK(Resolve(addon, "callouts.txt") == fs::u8path("C:\\GW2\\addons\\GW2_Chat_Messages\\links\\callouts.txt"));
        CHECK(Resolve(addon, " raids/x.txt ") == fs::u8path("C:\\GW2\\addons\\GW2_Chat_Messages\\links\\raids\\x.txt"));
        CHECK(Resolve(addon, "D:\\notes\\x.txt") == fs::u8path("D:\\notes\\x.txt"));
        CHECK(SamePath(Resolve(addon, "Callouts.TXT"), Resolve(addon, "callouts.txt")));
        CHECK(!SamePath(Resolve(addon, "a.txt"), Resolve(addon, "b.txt")));
    }

    void TestFileText()
    {
        CHECK(FileText(Lines("/s one\n/p two")) == "/s one\r\n/p two\r\n");
        CHECK(FileText({}).empty());
    }

    void TestFirstSync()
    {
        // At load or when linked, the file wins.
        FakeFile f;
        f.Text = "/s from file\r\n";
        State st;
        Outcome o = Sync(st, Lines("/s from macro"), f.View());
        CHECK(o.LoadMacro && !o.WriteFile);
        CHECK(o.Lines.size() == 1 && o.Lines[0].Message == "from file");
        CHECK(st.Known && st.Mtime == 100 && st.Text == "/s from file");

        // Already the same: nothing to do.
        State same;
        Outcome o2 = Sync(same, Lines("/s from file"), f.View());
        CHECK(!o2.LoadMacro && !o2.WriteFile && same.Known);
    }

    void TestSteadyState()
    {
        FakeFile f;
        f.Text = "/s hi\n";
        State st;
        std::vector<MacroTree::Line> macro = Lines("/s hi");
        Sync(st, macro, f.View());
        CHECK(f.Reads == 1);

        // Nothing changed: the file isn't read again.
        Outcome o = Sync(st, macro, f.View());
        CHECK(!o.LoadMacro && !o.WriteFile && f.Reads == 1);

        // Edited in the addon: written to the file.
        macro = Lines("/s hi\n/p party");
        o = Sync(st, macro, f.View());
        CHECK(o.WriteFile && o.Text == "/s hi\r\n/p party\r\n" && !o.LoadMacro);
        CHECK(f.Reads == 1);
        // The caller writes it and records the new time.
        f.Text  = o.Text;
        f.Mtime = 200;
        st.Mtime = 200;
        o = Sync(st, macro, f.View());
        CHECK(!o.WriteFile && !o.LoadMacro && f.Reads == 1);

        // Edited in Notepad: loaded into the macro.
        f.Text  = "/m map line\r\n";
        f.Mtime = 300;
        o = Sync(st, macro, f.View());
        CHECK(o.LoadMacro && o.Lines.size() == 1 && o.Lines[0].Channel == MacroTree::EChannel::Map);
        macro = o.Lines;
        o = Sync(st, macro, f.View());
        CHECK(!o.LoadMacro && !o.WriteFile);

        // Touched but the same lines (saved again, other line ends, "/say"): no change either way.
        f.Text  = "\n/map map line\n\n";
        f.Mtime = 400;
        o = Sync(st, macro, f.View());
        CHECK(!o.LoadMacro && !o.WriteFile);
        CHECK(st.Mtime == 400);
    }

    void TestBothChanged()
    {
        // The file changed since the last sync, and so did the macro: the file (newer) wins.
        FakeFile f;
        f.Text = "/s a\n";
        State st;
        Sync(st, Lines("/s a"), f.View());
        f.Text  = "/s from notepad\n";
        f.Mtime = 150;
        Outcome o = Sync(st, Lines("/s from addon"), f.View());
        CHECK(o.LoadMacro && !o.WriteFile && o.Lines[0].Message == "from notepad");
    }

    // SEC-023: what couldn't be read back stays out of the link both ways.
    void TestTooBig()
    {
        // 100 KB of Windows-1252 bytes: under the read limit, but 300 KB once each byte is replaced.
        FakeFile f;
        f.Text = "/s " + std::string(100 * 1024, '\xE9') + "\r\n";
        State st;
        Outcome o = Sync(st, Lines("/s a"), f.View());
        CHECK(!o.Error.empty() && !o.LoadMacro && !o.WriteFile && !st.Known);

        // A little of it is fine: replaced, with a warning.
        f.Text = "/s caf\xE9\r\n";
        o = Sync(st, Lines("/s a"), f.View());
        CHECK(o.Error.empty() && o.LoadMacro && !o.Warning.empty());

        // A macro bigger than the limit isn't written (it couldn't be read back), and is tried
        // again after the next edit.
        FakeFile g;
        g.Text = "/s a\n";
        State gs;
        Sync(gs, Lines("/s a"), g.View());
        const std::vector<MacroTree::Line> big = Lines("/s " + std::string(kMaxFileBytes, 'x'));
        o = Sync(gs, big, g.View());
        CHECK(!o.WriteFile && !o.Warning.empty() && gs.Text == "/s a");
        o = Sync(gs, Lines("/s b"), g.View());
        CHECK(o.WriteFile && o.Warning.empty() && gs.Text == "/s b");
    }

    void TestMissingAndErrors()
    {
        FakeFile f;
        f.Text = "/s a\n";
        State st;
        Sync(st, Lines("/s a"), f.View());

        // Deleted: nothing is written back (it isn't recreated behind the user's back).
        f.Exists = false;
        Outcome o = Sync(st, Lines("/s changed"), f.View());
        CHECK(o.Missing && !o.WriteFile && !o.LoadMacro);
        CHECK(st.Text == "/s a");

        // Back again with new text: it's read.
        f.Exists = true;
        f.Text   = "/s back\n";
        f.Mtime  = 500;
        o = Sync(st, Lines("/s changed"), f.View());
        CHECK(o.LoadMacro && o.Lines[0].Message == "back");

        // Can't be read: nothing changes, the next try reads again.
        f.Fail  = true;
        f.Mtime = 600;
        const int reads = f.Reads;
        o = Sync(st, Lines("/s back"), f.View());
        CHECK(!o.Error.empty() && !o.LoadMacro && !o.WriteFile && st.Mtime == 500);
        f.Fail = false;
        o = Sync(st, Lines("/s back"), f.View());
        CHECK(f.Reads == reads + 2 && o.Error.empty() && st.Mtime == 600);

        // An empty file empties the macro (the user cleared it on purpose).
        f.Text  = "";
        f.Mtime = 700;
        o = Sync(st, Lines("/s back"), f.View());
        CHECK(o.LoadMacro && o.Lines.empty());
    }

    void TestControlCharacters()
    {
        // A tab or a lone CR inside a row of the file becomes a space in the macro, and that alone
        // never writes the file back.
        FakeFile f;
        f.Text = "/s a\tb\rc\r\n";
        State st;
        Outcome o = Sync(st, {}, f.View());
        CHECK(o.LoadMacro && !o.WriteFile && o.Lines.size() == 1 && o.Lines[0].Message == "a b c");
        const std::vector<MacroTree::Line> macro = o.Lines;
        o = Sync(st, macro, f.View());
        CHECK(!o.LoadMacro && !o.WriteFile);
        f.Mtime = 200; // saved again, same text
        o = Sync(st, macro, f.View());
        CHECK(!o.LoadMacro && !o.WriteFile && st.Mtime == 200);

        // At load the macro (from macros.json) already has the spaces: nothing either way.
        State fresh;
        o = Sync(fresh, Lines("/s a b c"), f.View());
        CHECK(!o.LoadMacro && !o.WriteFile && fresh.Known);
    }

    void TestTooManyLines()
    {
        std::string text;
        for (size_t i = 0; i < MacroTree::kMaxLines + 3; i++) text += "/s line " + std::to_string(i) + "\n";
        FakeFile f;
        f.Text = text;
        State st;
        Outcome o = Sync(st, {}, f.View());
        CHECK(o.LoadMacro && o.Lines.size() == MacroTree::kMaxLines && !o.Warning.empty());
        // Unchanged in the addon, so the file's extra lines are left alone.
        o = Sync(st, o.Lines, f.View());
        CHECK(!o.WriteFile);
    }

    void TestNotUtf8()
    {
        const std::string bad = "\xEF\xBF\xBD"; // U+FFFD
        // A file saved as Windows-1252 (ä as one byte): the macro gets U+FFFD in its place, with a warning.
        FakeFile f;
        f.Text = "/s K\xE4se\r\n";
        State st;
        Outcome o = Sync(st, {}, f.View());
        CHECK(o.LoadMacro && !o.WriteFile && o.Lines.size() == 1 && o.Lines[0].Message == "K" + bad + "se");
        CHECK(!o.Warning.empty());
        std::vector<MacroTree::Line> macro = o.Lines;

        // The repair alone never writes the file back: not on the next sync...
        o = Sync(st, macro, f.View());
        CHECK(!o.LoadMacro && !o.WriteFile && f.Reads == 1);
        // ...nor when the file is touched (read again, same result)...
        f.Mtime = 200;
        o = Sync(st, macro, f.View());
        CHECK(!o.LoadMacro && !o.WriteFile && f.Reads == 2);
        // ...nor at the next load, when the macro already has the repaired lines.
        State fresh;
        o = Sync(fresh, macro, f.View());
        CHECK(!o.LoadMacro && !o.WriteFile);

        // Edited in the addon: the file is written as usual, as UTF-8.
        macro = Lines("/s K" + bad + "se\n/p x");
        o = Sync(st, macro, f.View());
        CHECK(o.WriteFile && o.Text == "/s K" + bad + "se\r\n/p x\r\n");

        // Both warnings when the file also has too many lines.
        std::string many = "\xFF\n";
        for (size_t i = 0; i < MacroTree::kMaxLines + 1; i++) many += "/s line\n";
        FakeFile g;
        g.Text = many;
        State st2;
        o = Sync(st2, {}, g.View());
        CHECK(o.Lines.size() == MacroTree::kMaxLines && o.Warning.find("UTF-8") != std::string::npos &&
              o.Warning.find("more than") != std::string::npos);
    }
}

void RunLinkSyncTests()
{
    TestPaths();
    TestFileText();
    TestFirstSync();
    TestSteadyState();
    TestBothChanged();
    TestTooBig();
    TestMissingAndErrors();
    TestControlCharacters();
    TestTooManyLines();
    TestNotUtf8();
}
