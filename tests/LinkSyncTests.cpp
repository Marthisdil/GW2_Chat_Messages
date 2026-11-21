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

        const fs::path addon = fs::u8path("C:\\GW2\\addons\\Chat Messages");
        CHECK(Resolve(addon, "callouts.txt") == fs::u8path("C:\\GW2\\addons\\Chat Messages\\links\\callouts.txt"));
        CHECK(Resolve(addon, " raids/x.txt ") == fs::u8path("C:\\GW2\\addons\\Chat Messages\\links\\raids\\x.txt"));
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
}

void RunLinkSyncTests()
{
    TestPaths();
    TestFileText();
    TestFirstSync();
    TestSteadyState();
    TestBothChanged();
    TestMissingAndErrors();
    TestTooManyLines();
}
