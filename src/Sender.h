#pragma once

// Sends a macro's lines into Guild Wars 2's chat, following docs/design.md "Input safety":
//  - Chat is opened with the user's own game binds (Nexus GameBinds), never a guessed key.
//  - Text and keys go only to the game window, on the game's window thread, through
//    WndProc_SendToGameOnly. Nothing goes through SendInput, so the OS keyboard state (and
//    the user's real Tab, Shift, Ctrl...) is never touched.
//  - Every step is checked against Mumble's "text box focused" flag, with short time limits.
//    On any failure chat is closed with one Escape (only if it's still open) and the rest of
//    the macro is dropped.
//  - One macro at a time, on a dedicated thread. Triggers while busy are refused.
#include <string>
#include <vector>

#include "MacroTree.h"

namespace Sender
{
    enum class EState { Idle, Sending, Done, Failed };

    struct Status
    {
        EState      State = EState::Idle;
        size_t      Line  = 0;  // 1-based line being sent (Sending)
        size_t      Lines = 0;  // lines to send in this macro
        std::string Message;    // what happened (Done / Failed)
        uint64_t    MacroId = 0;
    };

    void Init();     // in AddonLoad, after g_Api is set
    void Shutdown(); // in AddonUnload: stops and joins the thread, removes the WndProc hook

    // Starts sending (returns at once). False with aError set if it can't start.
    // Safe to call from any thread (render thread, keybind threads).
    bool Start(uint64_t aMacroId, const std::vector<MacroTree::Line>& aLines, std::string& aError);

    Status GetStatus();
    bool   IsBusy();
}
