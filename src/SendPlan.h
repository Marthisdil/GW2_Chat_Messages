#pragma once

// Turns a macro's lines into the exact steps the sender performs in game.
// No Nexus, ImGui or Windows dependency, so the sequence is unit-tested (tests/SendPlanTests.cpp).
// The rules behind each step are in docs/design.md, "Input safety".

#include <string>
#include <vector>

#include "MacroTree.h"

namespace SendPlan
{
    enum class EStep
    {
        OpenChat,            // press the user's "Chat focus" game bind
        OpenSquadBroadcast,  // press the user's "Squad broadcast chat focus" game bind
        WaitChatOpen,        // wait until the game reports a focused text box
        Type,                // type Text into the focused box
        KeyTab,              // whisper: move from the recipient box to the message box
        KeyEnter,            // send
        WaitChatClosed,      // wait until the text box loses focus (the message went out)
        Pause,               // wait Ms milliseconds
    };

    struct Step
    {
        EStep       Type = EStep::Pause;
        std::string Text;      // Type only
        int         Ms   = 0;  // Pause only
        size_t      Line = 0;  // index of the macro line this step belongs to
    };

    struct Plan
    {
        std::vector<Step> Steps;
        size_t            LinesToSend = 0;
        std::string       Error; // non-empty: nothing may be sent
    };

    constexpr int kPauseBetweenLinesMs = 50;

    // Which bytes of a line's Message and WhisperTo came from placeholder values
    // (Placeholders::Result::FromValue). Empty vectors: all written by the user.
    struct Origin
    {
        std::vector<bool> Message, WhisperTo;
    };

    // Checks every line first, so a macro is either sent whole or not at all.
    // Lines with an empty message are skipped (chat ignores them anyway).
    // Refused (SEC-013):
    //  - typed text with a control character (C0, DEL, C1), so our own Enter and Tab are the only
    //    ones the game ever gets;
    //  - a "/" that chat would read as a command but that came from a placeholder value. The game
    //    reads commands as the text arrives and a channel command ("/g ") empties the box, so that's
    //    a "/" at the start of a box we type into, or right after a command word and its spaces
    //    ("/g /w Name"), on any channel, after any kind of space.
    // aOrigins: empty, or one per line.
    Plan Build(const std::vector<MacroTree::Line>& aLines, bool aIsCommander,
               const std::vector<Origin>& aOrigins = {});
}
