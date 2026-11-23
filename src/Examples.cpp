#include "Examples.h"

#include "Localization.h"

#include <string>

namespace
{
    using MacroTree::EChannel;
    using MacroTree::Line;
    using MacroTree::Node;
    using MacroTree::Tree;

    Node* Folder(Tree& aTree, Node* aParent, const std::string& aName)
    {
        return aTree.AddFolder(aParent, aName).Target;
    }

    Node* Macro(Tree& aTree, Node* aParent, const std::string& aName, std::initializer_list<Line> aLines)
    {
        Node* m = aTree.AddMacro(aParent, aName).Target;
        if (m) m->Lines.assign(aLines);
        return m;
    }

    Line Say(EChannel aChannel, const char* aText) { return Line{aChannel, aText, "", false}; }
}

namespace Examples
{
    Node* Add(Tree& aTree)
    {
        // A free top-level name: "Examples", then "Examples (2)", ...
        const std::string base = TR("Examples");
        Node* root = nullptr;
        for (int i = 1; i <= 100 && !root; ++i)
            root = Folder(aTree, nullptr, i == 1 ? base : base + " (" + std::to_string(i) + ")");
        if (!root) return nullptr;

        Macro(aTree, root, TR("Hello"), {Say(EChannel::Say, TR("Hello everyone!"))});

        Node* basics = Folder(aTree, root, TR("Basics"));
        Macro(aTree, basics, TR("Two channels"),
              {Say(EChannel::Say, TR("Heading to the next event, come along!")),
               Say(EChannel::Party, TR("I'll wait for you at the waypoint."))});
        Macro(aTree, basics, TR("Whisper a friend"),
              {Line{EChannel::Whisper, TR("Hi! Want to run some fractals later?"), TR("Friend Name"), false}});
        Macro(aTree, basics, TR("Wave"), {Say(EChannel::Emote, TR("waves hello."))});

        Node* placeholders = Folder(aTree, root, TR("Placeholders"));
        Macro(aTree, placeholders, TR("Roll a die"), {Say(EChannel::Say, TR("I rolled a {random 1 6}!"))});
        Macro(aTree, placeholders, TR("Time check"), {Say(EChannel::Party, TR("It's {time} for me, I can stay for an hour."))});
        Macro(aTree, placeholders, TR("Where am I"), {Say(EChannel::Party, TR("I'm in {map}, come and join me."))});

        Node* filters = Folder(aTree, root, TR("Filters"));
        Node* wvw = Folder(aTree, filters, TR("In WvW"));
        if (Node* m = Macro(aTree, wvw, TR("Stack on tag"), {Line{EChannel::Squad, TR("Stack on the commander!"), "", true}}))
            m->GameModes = MacroTree::GM_WvW;
        Node* la = Folder(aTree, filters, TR("In Lion's Arch"));
        if (Node* m = Macro(aTree, la, TR("Meet at the bank"), {Say(EChannel::Map, TR("Meet me at the bank in Lion's Arch."))}))
            m->MapIds = {kLionsArchMapId};
        return root;
    }
}
