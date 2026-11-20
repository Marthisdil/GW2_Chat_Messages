#pragma once

// Listens for macro hotkeys in the game window's messages (docs/design.md, "Hotkeys").
//  - Reads key and mouse-button presses in a Nexus WndProc hook. It never blocks or changes a
//    message: every key still reaches the game exactly as before.
//  - Ignores auto-repeat, and ignores everything while a game text box is focused (chat), while
//    an ImGui text box is being typed in (any addon), and outside gameplay.
//  - Only reports presses. The render thread matches them against the macro tree and starts the
//    sender, so the tree is never touched from the window thread.
#include "Hotkey.h"

namespace Hotkeys
{
    void Init();     // in AddonLoad
    void Shutdown(); // in AddonUnload: removes the WndProc hook

    // Render thread, every frame: whether ImGui is taking text input (ImGui::GetIO().WantTextInput).
    void SetUiTyping(bool aTyping);

    // Render thread: the next hotkey press to handle. False when there's none.
    bool PopPress(Hotkey::Combo& aOut);

    // Capturing a new hotkey: the next key or mouse-button press (with its modifiers) is recorded
    // instead of triggering anything. Escape on its own cancels.
    void StartCapture();
    void CancelCapture();
    bool IsCapturing();
    // Render thread: true once the capture ended; aCancelled says if Escape cancelled it.
    bool PopCapture(Hotkey::Combo& aOut, bool& aCancelled);
}
