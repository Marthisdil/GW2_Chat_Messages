// Standalone tests for src/TextSize.cpp (the text size setting).

#include <string>
#include <vector>

#include "TestCheck.h"
#include "TextSize.h"

using namespace TextSize;

namespace
{
    // Stand-ins for ImFont addresses.
    int g_A, g_B, g_C, g_D;

    std::vector<Font> Fonts(float aDefault, float aNexus, float aUi, float aBig, bool aNexusIsDefault = false)
    {
        return {
            {EFont::Default,  &g_A, aDefault},
            {EFont::Nexus,    aNexusIsDefault ? static_cast<const void*>(&g_A) : &g_B, aNexus},
            {EFont::NexusUi,  &g_C, aUi},
            {EFont::NexusBig, &g_D, aBig},
        };
    }

    void TestKeys()
    {
        const EFont all[] = {EFont::Default, EFont::Nexus, EFont::NexusUi, EFont::NexusBig};
        for (EFont f : all)
        {
            EFont back = EFont::Default;
            CHECK(FromKey(Key(f), back) && back == f);
        }
        EFont f = EFont::NexusBig;
        CHECK(!FromKey("huge", f) && f == EFont::NexusBig);
        CHECK(!FromKey("", f));
        CHECK(std::string(Key(EFont::NexusBig)) == "nexus_big");
    }

    void TestChoices()
    {
        // Sorted by size, smallest first.
        std::vector<Font> c = Choices(Fonts(13, 16, 15, 24));
        CHECK(c.size() == 4);
        CHECK(c[0].Kind == EFont::Default && c[1].Kind == EFont::NexusUi && c[2].Kind == EFont::Nexus && c[3].Kind == EFont::NexusBig);

        // The same font under two kinds is listed once, as the earlier kind.
        c = Choices(Fonts(16, 16, 15, 24, true));
        CHECK(c.size() == 3);
        CHECK(c[0].Kind == EFont::NexusUi && c[1].Kind == EFont::Default && c[2].Kind == EFont::NexusBig);

        // Different fonts of the same size are both listed, in EFont order.
        c = Choices(Fonts(16, 16, 16, 24));
        CHECK(c.size() == 4 && c[0].Kind == EFont::Default && c[1].Kind == EFont::Nexus && c[2].Kind == EFont::NexusUi);

        // Unavailable fonts (null, or no size: Nexus is rebuilding its fonts) are left out.
        std::vector<Font> f = Fonts(13, 16, 15, 24);
        f[3].Id = nullptr;
        f[2].Size = 0.0f;
        c = Choices(f);
        CHECK(c.size() == 2 && c[0].Kind == EFont::Default && c[1].Kind == EFont::Nexus);
        CHECK(Choices({}).empty());
    }

    void TestPick()
    {
        std::vector<Font> f = Fonts(13, 16, 15, 24);
        CHECK(Pick(f, EFont::NexusBig).Kind == EFont::NexusBig);
        CHECK(Pick(f, EFont::Default).Kind == EFont::Default);
        // Not available this frame: the default font, until it's back.
        f[3].Id = nullptr;
        CHECK(Pick(f, EFont::NexusBig).Kind == EFont::Default);
        f[0].Id = nullptr;
        CHECK(!Available(Pick(f, EFont::NexusBig)));

        // The dropdown shows the merged entry as chosen.
        f = Fonts(16, 16, 15, 24, true);
        const std::vector<Font> c = Choices(f);
        CHECK(Shows(c[1], f, EFont::Nexus));  // "Default" entry stands for the Nexus font too
        CHECK(Shows(c[1], f, EFont::Default));
        CHECK(!Shows(c[2], f, EFont::Nexus));
    }

    void TestLabels()
    {
        CHECK(Label({EFont::Default, &g_A, 13.0f}) == "Default (13 px)");
        CHECK(Label({EFont::NexusBig, &g_D, 23.6f}) == "Nexus large font (24 px)");
        CHECK(Label({EFont::NexusUi, &g_C, 0.0f}) == "Nexus UI font");
    }

    void TestScale()
    {
        CHECK(Scale(16, 16) == 1.0f);
        CHECK(Scale(16, 24) == 1.5f);
        CHECK(Scale(16, 12) == 0.75f);
        CHECK(Scale(0, 24) == 1.0f && Scale(16, 0) == 1.0f && Scale(-1, 5) == 1.0f);
        CHECK(Scale(10, 100) == 4.0f && Scale(100, 10) == 0.5f);
    }

    void TestRescale()
    {
        // Grows from its top-left corner.
        Box b = Rescale({100, 50, 800, 500}, 1.5f, 1920, 1080);
        CHECK(b.X == 100 && b.Y == 50 && b.W == 1200 && b.H == 750);
        // Shrinks too.
        b = Rescale({100, 50, 800, 500}, 0.5f, 1920, 1080);
        CHECK(b.X == 100 && b.Y == 50 && b.W == 400 && b.H == 250);
        // Would go off the right and bottom edges: moved back on screen.
        b = Rescale({1500, 800, 400, 200}, 2.0f, 1920, 1080);
        CHECK(b.W == 800 && b.H == 400 && b.X == 1120 && b.Y == 680);
        // Bigger than the screen: the screen's size, at the top-left.
        b = Rescale({300, 300, 1500, 900}, 2.0f, 1920, 1080);
        CHECK(b.W == 1920 && b.H == 1080 && b.X == 0 && b.Y == 0);
        // Unknown screen size: no clamping.
        b = Rescale({10, 10, 100, 100}, 3.0f, 0, 0);
        CHECK(b.W == 300 && b.H == 300 && b.X == 10);
        // A bad ratio leaves the size alone.
        b = Rescale({10, 10, 100, 100}, 0.0f, 1920, 1080);
        CHECK(b.W == 100 && b.H == 100);
    }
}

void RunTextSizeTests()
{
    TestKeys();
    TestChoices();
    TestPick();
    TestLabels();
    TestScale();
    TestRescale();
}
