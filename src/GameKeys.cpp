#include "GameKeys.h"

#include <cstddef>

#include "Localization.h" // TX

namespace
{
    using Hotkey::Combo;

    constexpr Combo K(uint16_t aKey, bool aCtrl = false, bool aAlt = false, bool aShift = false)
    {
        return Combo{aKey, aCtrl, aAlt, aShift};
    }
    constexpr Combo Ctrl(uint16_t aKey) { return K(aKey, true); }
    constexpr Combo Alt(uint16_t aKey) { return K(aKey, false, true); }
    constexpr Combo Shift(uint16_t aKey) { return K(aKey, false, false, true); }
    constexpr Combo AltShift(uint16_t aKey) { return K(aKey, false, true, true); }

    // Virtual-key codes (windows.h values) for the keys below that aren't letters or digits.
    constexpr uint16_t kBackquote = 0xC0, kMinus = 0xBD, kEquals = 0xBB, kBackslash = 0xDC, kSlash = 0xBF;
    constexpr uint16_t kEnter = 0x0D, kTab = 0x09, kPageUp = 0x21, kPageDown = 0x22, kPrintScreen = 0x2C;
    constexpr uint16_t kLeft = 0x25, kUp = 0x26, kRight = 0x27, kDown = 0x28, kNumLock = 0x90;
    constexpr uint16_t kNumPlus = 0x6B, kNumMinus = 0x6D;
    constexpr uint16_t kF1 = 0x70;

    // From the GW2 wiki's Options page, "Control Options" (checked 2026-11-23). Plain Tab, Enter,
    // Space and Backspace are left out: they can't be hotkeys anyway (Hotkey::Refusal). Where a key
    // has two default uses (Dodge and Mount ability 1 on V), both are named.
    const GameKeys::Bind kBinds[] = {
        // Movement
        {K('W'), TX("Move forward")}, {K(kUp), TX("Move forward")},
        {K('S'), TX("Move backward")}, {K(kDown), TX("Move backward")},
        {K('A'), TX("Strafe left")}, {K(kLeft), TX("Strafe left")},
        {K('D'), TX("Strafe right")}, {K(kRight), TX("Strafe right")},
        {K('Q'), TX("Turn left")}, {K('E'), TX("Turn right")},
        {K('V'), TX("Dodge, and mount ability 1")},
        {K('R'), TX("Autorun")}, {K(kNumLock), TX("Autorun")},
        // Skills
        {K(kBackquote), TX("Swap weapons")},
        {K('1'), TX("Weapon skill 1")}, {K('2'), TX("Weapon skill 2")}, {K('3'), TX("Weapon skill 3")},
        {K('4'), TX("Weapon skill 4")}, {K('5'), TX("Weapon skill 5")},
        {K('6'), TX("Healing skill")},
        {K('7'), TX("Utility skill 1")}, {K('8'), TX("Utility skill 2")}, {K('9'), TX("Utility skill 3")},
        {K('0'), TX("Elite skill")},
        {K(kF1), TX("Profession skill 1")}, {K(kF1 + 1), TX("Profession skill 2")},
        {K(kF1 + 2), TX("Profession skill 3")}, {K(kF1 + 3), TX("Profession skill 4")},
        {K(kF1 + 4), TX("Profession skill 5")}, {K(kF1 + 5), TX("Profession skill 6")},
        {K(kF1 + 6), TX("Profession skill 7")},
        {K('N'), TX("Special action")},
        // Targeting
        {Shift('T'), TX("Alert target")}, {Ctrl('T'), TX("Call target")}, {K('T'), TX("Take target")},
        {Shift(kTab), TX("Previous enemy")},
        // User interface
        {K('O'), TX("Black Lion Trading")}, {K('Y'), TX("Contacts")}, {K('G'), TX("Guild")},
        {K('H'), TX("Hero panel")}, {K('I'), TX("Inventory")}, {K('K'), TX("Pet panel (ranger)")},
        {K(kF1 + 11), TX("Log out")}, {K(kF1 + 10), TX("Options")}, {K('P'), TX("Party")},
        {K('B'), TX("Scoreboard")}, {Shift('H'), TX("Wizard's Vault")},
        {K(kMinus), TX("Information panel, and zoom the map out")},
        {K(kBackslash), TX("Show or hide chat")}, {K(kSlash), TX("Chat command")},
        {K('H', true, false, true), TX("Show or hide the whole UI")},
        {Shift(kBackslash), TX("Show or hide squad broadcast")},
        {Shift(kSlash), TX("Squad broadcast command")}, {Shift(kEnter), TX("Squad broadcast message")},
        // Camera, screenshot, map
        {K(kPageUp), TX("Zoom in (on the map: floor up)")}, {K(kPageDown), TX("Zoom out (on the map: floor down)")},
        {K(kPrintScreen), TX("Screenshot")},
        {K('M'), TX("Map")}, {K(kEquals), TX("Zoom the map in")}, {K(kNumPlus), TX("Zoom the map in")},
        {K(kNumMinus), TX("Zoom the map out")},
        // Mounts
        {K('X'), TX("Mount or dismount")}, {K('C'), TX("Mount ability 2")},
        // Squad markers
        {Alt('1'), TX("Squad location marker")}, {Alt('2'), TX("Squad location marker")},
        {Alt('3'), TX("Squad location marker")}, {Alt('4'), TX("Squad location marker")},
        {Alt('5'), TX("Squad location marker")}, {Alt('6'), TX("Squad location marker")},
        {Alt('7'), TX("Squad location marker")}, {Alt('8'), TX("Squad location marker")},
        {Alt('9'), TX("Clear all squad location markers")},
        {AltShift('1'), TX("Squad object marker")}, {AltShift('2'), TX("Squad object marker")},
        {AltShift('3'), TX("Squad object marker")}, {AltShift('4'), TX("Squad object marker")},
        {AltShift('5'), TX("Squad object marker")}, {AltShift('6'), TX("Squad object marker")},
        {AltShift('7'), TX("Squad object marker")}, {AltShift('8'), TX("Squad object marker")},
        {AltShift('9'), TX("Clear all squad object markers")},
        // Mastery skills, miscellaneous, spectators
        {K('J'), TX("Mastery skill")}, {K('F'), TX("Interact")}, {K('U'), TX("Novelty")},
        {K('L'), TX("Decorate mode")}, {Alt(kEnter), TX("Full screen on or off")},
        {K('F', true, false, true), TX("Free camera (spectators)")},
    };
}

const GameKeys::Bind* GameKeys::All(size_t& aCount)
{
    aCount = sizeof(kBinds) / sizeof(kBinds[0]);
    return kBinds;
}

const char* GameKeys::DefaultUse(const Hotkey::Combo& aCombo)
{
    size_t n = 0;
    const Bind* all = All(n);
    for (size_t i = 0; i < n; i++)
        if (all[i].Combo == aCombo) return all[i].Use;
    return nullptr;
}
