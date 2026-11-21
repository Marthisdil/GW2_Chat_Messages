#pragma once

// The text size of the addon's own windows (docs/design.md, "Text size"): the font chosen in the
// "Text size" dropdown, sizes that grow with it, and helpers so nothing runs off a window's edge.
// Render thread only. The choice rules are in src/TextSize.* (unit tested).
//
// Fonts are read from Nexus' NexusLink every time they're used and never kept: Nexus sends null
// pointers while it rebuilds its fonts, and the default font is used for those frames.

#include "imgui.h"

namespace UiFont
{
    void Init();     // reads the choice from settings.json
    void NewFrame(); // start of Ui::Render: works out the default and chosen sizes for this frame

    // Around our own windows, popups and menus (not Nexus' options page). Every Push needs a Pop.
    void Push();
    void Pop();

    // A size laid out for the default font, grown or shrunk to the chosen one.
    float Px(float aDesignPx);
    ImVec2 Px(float aDesignW, float aDesignH);

    // The "Text size" dropdown. Saves the choice when it changes.
    void DrawCombo(const char* aLabel);
    float ComboWidth(const char* aLabel); // the dropdown with its label, for a Row

    // Keeps a window's size in step with the text size: give it the size it was designed with, and
    // when the text size changes its current size is scaled by the same amount (kept on screen).
    struct WindowFit
    {
        bool  Seen  = false; // drawn at least once (Pos, Size and Scale are known)
        float Scale = 1.0f;  // the scale it was last drawn at
        ImVec2 Pos, Size;
    };
    void BeforeBegin(WindowFit& aFit, ImVec2 aDesignSize, ImVec2 aDesignMinSize);
    void AfterBegin(WindowFit& aFit); // right after ImGui::Begin, whatever it returned

    // A row of buttons (or other items) that continues on the next line when the next one doesn't
    // fit. Make it at the start of the row, draw the first item, then call Next(width) before each
    // following item instead of ImGui::SameLine().
    class Row
    {
    public:
        Row();
        void Next(float aItemWidth);

    private:
        float m_Right;
    };
    float ButtonWidth(const char* aLabel);   // Button or SmallButton ("##" ids are ignored)
    float CheckboxWidth(const char* aLabel); // Checkbox or RadioButton
    float TextWidth(const char* aText);

    // Wraps text in a tooltip (or another window that sizes itself to its contents) at a
    // comfortable width. Pair with ImGui::PopTextWrapPos().
    void PushTooltipWrap();
}
