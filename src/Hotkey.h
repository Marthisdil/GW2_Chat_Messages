#pragma once

// A macro's hotkey: one key or mouse button plus Ctrl/Alt/Shift (docs/design.md, "Hotkeys").
// No Nexus, ImGui or windows.h dependency, so it can be unit-tested (tests/HotkeyTests.cpp).
// Keys are Windows virtual-key codes; the mouse buttons use their own codes (Mouse 3/4/5).

#include <cstdint>
#include <string>

namespace Hotkey
{
    // Virtual-key codes used here (same values as windows.h).
    constexpr uint16_t kVkMButton  = 0x04; // Mouse 3 (middle)
    constexpr uint16_t kVkXButton1 = 0x05; // Mouse 4
    constexpr uint16_t kVkXButton2 = 0x06; // Mouse 5
    constexpr uint16_t kVkEscape   = 0x1B;

    struct Combo
    {
        uint16_t Key   = 0; // 0 = no hotkey
        bool     Ctrl  = false;
        bool     Alt   = false;
        bool     Shift = false;

        bool     Empty() const { return Key == 0; }
        bool     HasModifier() const { return Ctrl || Alt || Shift; }
        uint32_t Pack() const;
        static Combo Unpack(uint32_t aPacked);

        bool operator==(const Combo& o) const { return Pack() == o.Pack(); }
        bool operator!=(const Combo& o) const { return !(*this == o); }
    };

    // "Ctrl+Shift+F1", "Mouse 4", "Num 5". Empty combo -> "".
    std::string ToString(const Combo& aCombo);
    // Inverse of ToString (case-insensitive, spaces around '+' allowed). Unknown -> empty combo.
    Combo FromString(const std::string& aText);
    // Display name of one key, e.g. "F1", "Mouse 4", "Key 0xE2" for unnamed keys.
    std::string KeyName(uint16_t aKey);

    // Shift, Ctrl, Alt and Windows keys: never a hotkey's main key (they're its modifiers).
    bool IsModifierKey(uint16_t aKey);
    // Mouse 3, 4 or 5.
    bool IsMouseKey(uint16_t aKey);

    // Why aCombo can't be a hotkey, or "" if it can.
    // Refused: left/right mouse, Escape, modifier keys alone, Tab/Enter/Space/Backspace without
    // a modifier (they'd fire every time the key is used normally in game), and Mouse 3/4/5
    // without a modifier (often dodge or a skill, so easy to press by accident).
    std::string Refusal(const Combo& aCombo);

    // A plain Mouse 3/4/5 hotkey saved before mouse buttons needed a modifier. It's kept in
    // macros.json (so the user sees it and can change it) but never fires.
    bool NeedsModifier(const Combo& aCombo);

    // Whether a hotkey read from macros.json or an import is kept: everything Refusal allows,
    // plus the old plain mouse hotkeys (NeedsModifier).
    bool KeepOnLoad(const Combo& aCombo);

    // Whether a press of aCombo may send a macro (Refusal allows it).
    bool Fires(const Combo& aCombo);
}
