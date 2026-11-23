// Tests for Examples (the starter library).

#include "Examples.h"
#include "ChatLine.h"
#include "Exchange.h"
#include "Placeholders.h"
#include "TestCheck.h"

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

using namespace MacroTree;

namespace
{
    void Walk(const Node& aNode, const std::function<void(const Node&)>& aFn)
    {
        aFn(aNode);
        for (const auto& c : aNode.Children) Walk(*c, aFn);
    }

    const Node* Child(const Node* aFolder, const std::string& aName)
    {
        if (!aFolder) return nullptr;
        for (const auto& c : aFolder->Children)
            if (c->Name == aName) return c.get();
        return nullptr;
    }
}

static void TestShape()
{
    Tree t;
    Node* ex = Examples::Add(t);
    CHECK(ex != nullptr && ex->IsFolder() && ex->Parent == &t.Root() && ex->Name == "Examples");
    // Nested sub-folders: Examples > Filters > In WvW / In Lion's Arch.
    const Node* filters = Child(ex, "Filters");
    const Node* wvw = Child(filters, "In WvW");
    const Node* la = Child(filters, "In Lion's Arch");
    CHECK(Child(ex, "Basics") && Child(ex, "Placeholders") && wvw && la);
    CHECK(wvw && Tree::FolderLevel(*wvw) == 3);

    const Node* stack = Child(wvw, "Stack on tag");
    CHECK(stack && stack->GameModes == GM_WvW && stack->Lines.size() == 1 && stack->Lines[0].SquadBroadcast);
    const Node* bank = Child(la, "Meet at the bank");
    CHECK(bank && bank->MapIds.size() == 1 && bank->MapIds[0] == Examples::kLionsArchMapId);
    const Node* whisper = Child(Child(ex, "Basics"), "Whisper a friend");
    CHECK(whisper && whisper->Lines.size() == 1 && whisper->Lines[0].Channel == EChannel::Whisper &&
          !whisper->Lines[0].WhisperTo.empty());
    CHECK(Child(Child(ex, "Basics"), "Two channels") && Child(Child(ex, "Basics"), "Two channels")->Lines.size() == 2);

    // Every macro has lines that can be sent: within the length limit, no removed placeholder,
    // no hotkey and no cooldown.
    int macros = 0;
    Walk(*ex, [&](const Node& n) {
        if (!n.IsMacro()) return;
        ++macros;
        CHECK(!n.Lines.empty());
        CHECK(n.Hotkey.Empty() && n.CooldownSec == 0);
        for (const Line& l : n.Lines)
        {
            CHECK(!l.Message.empty());
            CHECK(ChatLine::Utf16Length(ChatLine::TypedText(l)) <= ChatLine::kMaxChatLength);
            CHECK(!Placeholders::UsesRemoved(l.Message));
        }
    });
    CHECK(macros == 9);

    // It survives a save and load unchanged.
    Tree u;
    std::vector<std::string> w;
    CHECK(u.FromJson(t.ToJson(), &w) && w.empty() && u.ToJson() == t.ToJson());
}

static void TestAddedAgain()
{
    Tree t;
    Node* first = Examples::Add(t);
    CHECK(first && first->Id == Examples::kFolderId);
    Node* second = Examples::Add(t);
    CHECK(second && second->Name == "Examples (2)" && second->Id != Examples::kFolderId);
    Node* third = Examples::Add(t);
    CHECK(third && third->Name == "Examples (3)" && third->Id != Examples::kFolderId);
    CHECK(t.Root().Children.size() == 3);
    CHECK(t.Find(Examples::kFolderId) == first);
}

static nlohmann::json Library(const std::string& aChildren)
{
    return nlohmann::json::parse(R"({"format":1,"root":{"children":[)" + aChildren + "]}}");
}

// The reserved id: only the first Examples folder has it, and nothing else can take it.
static void TestReservedId()
{
    const std::string rid = "4558414d504c4531"; // kReservedExamplesId as macros.json writes it
    {
        // It's written and read back like any id.
        Tree t;
        Examples::Add(t);
        CHECK(t.ToJson().dump().find(rid) != std::string::npos);
        Tree u;
        CHECK(u.FromJson(t.ToJson()));
        CHECK(u.Find(Examples::kFolderId) && u.Find(Examples::kFolderId)->Name == "Examples");
    }
    {
        // A macro with the reserved id (a hand-edited file) gets a new one, with a warning.
        Tree t;
        std::vector<std::string> w;
        CHECK(t.FromJson(Library(R"({"type":"macro","id":")" + rid + R"(","name":"M"})"), &w));
        CHECK(t.Root().Children.size() == 1 && t.Root().Children[0]->Id != Examples::kFolderId && t.Root().Children[0]->Id != 0);
        CHECK(w.size() == 1 && IsNewIdsWarning(w[0]));
        CHECK(!t.Find(Examples::kFolderId));
        // Then adding the examples still gets the reserved id.
        Node* ex = Examples::Add(t);
        CHECK(ex && ex->Id == Examples::kFolderId);
    }
    {
        // A folder keeps it, but only the first one: a second folder with it gets a new id.
        Tree t;
        std::vector<std::string> w;
        CHECK(t.FromJson(Library(R"({"type":"folder","id":")" + rid + R"(","name":"Mine"},)"
                                 R"({"type":"folder","id":")" + rid + R"(","name":"Other"})"), &w));
        CHECK(t.Root().Children.size() == 2);
        CHECK(t.Root().Children[0]->Id == Examples::kFolderId && t.Root().Children[1]->Id != Examples::kFolderId);
        // The examples then get an ordinary id: a folder already holds the reserved one.
        Node* ex = Examples::Add(t);
        CHECK(ex && ex->Id != Examples::kFolderId);
    }
    {
        // New folders and macros never get it: thousands of new ids, none reserved.
        Tree t;
        bool clean = true;
        for (int i = 0; i < 2000 && clean; ++i)
        {
            Node* n = i % 2 ? t.AddFolder(nullptr, "F" + std::to_string(i)).Target : t.AddMacro(nullptr, "M" + std::to_string(i)).Target;
            clean = n && n->Id != Examples::kFolderId && n->Id != 0;
        }
        CHECK(clean);
    }
    {
        // ClaimReservedId: only for a folder in the tree, and only once.
        Tree t;
        Node* m = t.AddMacro(nullptr, "M").Target;
        Node* f = t.AddFolder(nullptr, "F").Target;
        Node* g = t.AddFolder(nullptr, "G").Target;
        CHECK(m && !t.ClaimReservedId(*m));
        CHECK(!t.ClaimReservedId(t.Root()));
        CHECK(f && t.ClaimReservedId(*f) && f->Id == Examples::kFolderId);
        CHECK(g && !t.ClaimReservedId(*g) && g->Id != Examples::kFolderId);
    }
    {
        // An import of an exported Examples folder is an ordinary folder: imports get new ids.
        Tree mine;
        Tree theirs;
        const Node* ex = Examples::Add(theirs);
        Exchange::Package p = Exchange::FromJsonText(Exchange::ExportJsonText({ex}));
        CHECK(p.Ok() && p.Items.Find(Examples::kFolderId) != nullptr); // the file still has it
        Exchange::Options o;
        CHECK(Exchange::Apply(mine, mine.Root(), Exchange::BuildPlan(mine, mine.Root(), p.Items, o), o) > 0);
        CHECK(mine.Root().Children.size() == 1 && mine.Root().Children[0]->Name == "Examples");
        CHECK(!mine.Find(Examples::kFolderId));
    }
    {
        // In the trash bin it still counts: a new copy doesn't take the id, and a restore keeps it.
        Tree t;
        Node* ex = Examples::Add(t);
        CHECK(ex && t.MoveToTrash(*ex, "2026-11-23 20:00").Ok);
        Node* copy = Examples::Add(t);
        CHECK(copy && copy->Id != Examples::kFolderId);
        CHECK(t.Restore(0, ERestoreTo::Original).Ok);
        CHECK(t.Find(Examples::kFolderId) && t.Find(Examples::kFolderId) != copy); // the copy took the name
    }
}

void RunExamplesTests()
{
    TestShape();
    TestAddedAgain();
    TestReservedId();
}
