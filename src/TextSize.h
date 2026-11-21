#pragma once

// The text size setting for the addon's own windows (docs/design.md, "Text size"). Nexus offers a
// few ready-made fonts; the user picks one in a dropdown, and our windows, fixed widths and window
// sizes follow its size. No Nexus or ImGui dependency (tested by tests/TextSizeTests.cpp); the
// UI side is src/UiFont.*.

#include <string>
#include <vector>

namespace TextSize
{
    // Default = the font ImGui draws with when we push nothing (as Nexus set it up). The others are
    // NexusLinkData_t::Font, FontUI and FontBig.
    enum class EFont { Default, Nexus, NexusUi, NexusBig };

    // The name stored in settings.json: "default", "nexus", "nexus_ui", "nexus_big".
    const char* Key(EFont aFont);
    // False (aFont unchanged) for an unknown name.
    bool FromKey(const std::string& aKey, EFont& aFont);

    // One font as Nexus has it right now. Id is the font's address, used only to tell fonts apart
    // within one frame (never kept). Id null or Size <= 0: not available.
    struct Font
    {
        EFont       Kind = EFont::Default;
        const void* Id   = nullptr;
        float       Size = 0.0f;
    };
    bool Available(const Font& aFont);

    // What the dropdown lists: the available fonts, smallest first (ties in EFont order). A font
    // that's the same one as another kind (same Id, e.g. Nexus' font is also the default) is listed
    // once, as the kind that comes first in EFont order.
    std::vector<Font> Choices(const std::vector<Font>& aFonts);

    // The font to draw with for the saved choice: that font if available, else the default.
    // Returns an unavailable Font if not even the default is there.
    Font Pick(const std::vector<Font>& aFonts, EFont aWanted);

    // Whether the dropdown entry aChoice stands for the saved choice aWanted (same kind, or the
    // same font listed under another kind).
    bool Shows(const Font& aChoice, const std::vector<Font>& aFonts, EFont aWanted);

    // "Default (13 px)", "Nexus large font (24 px)"...
    std::string Label(const Font& aFont);

    // How much bigger the chosen font is than the default one: sizes designed for the default
    // font are multiplied by this. 1 when either size is unknown; kept within 0.5-4.
    float Scale(float aDefaultSize, float aChosenSize);

    // A window's place on screen, in pixels.
    struct Box
    {
        float X = 0.0f, Y = 0.0f, W = 0.0f, H = 0.0f;
    };
    // A window resized by aRatio (the new scale over the old one), keeping its top-left corner,
    // at most the screen's size, and moved left/up just enough to stay on screen. A screen size
    // of 0 means unknown (no clamping).
    Box Rescale(const Box& aWindow, float aRatio, float aScreenW, float aScreenH);
}
