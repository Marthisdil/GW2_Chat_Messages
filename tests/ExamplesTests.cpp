// Tests for Examples (the starter library).

#include "Examples.h"
#include "ChatLine.h"
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
    CHECK(Examples::Add(t) != nullptr);
    Node* second = Examples::Add(t);
    CHECK(second && second->Name == "Examples (2)");
    Node* third = Examples::Add(t);
    CHECK(third && third->Name == "Examples (3)");
    CHECK(t.Root().Children.size() == 3);
}

void RunExamplesTests()
{
    TestShape();
    TestAddedAgain();
}
