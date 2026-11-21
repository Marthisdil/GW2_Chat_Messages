#pragma once

// Chat lines as text: the raw edit format, what gets typed into chat, and length checks.
// No Nexus or ImGui dependency (tested by tests/ChatLineTests.cpp).
//
// Raw format, one line per row, as you'd type it in game:
//   hello                 -> current channel
//   /s hello              -> Say            (also /say, and the German, French and Spanish forms)
//   /w [Some Name] hi     -> Whisper to "Some Name"
//   /d ! Stack on tag     -> Squad broadcast (sent as a broadcast only when you're commander)
//   /g3 hi                -> Guild 3
// Any other "/..." text (e.g. "/wave") stays a current-channel message, so emotes and game
// commands still work.

#include <cstdint>
#include <string>
#include <vector>

#include "MacroTree.h"

namespace ChatLine
{
    // GW2's chat box limit, counted like the game counts: UTF-16 code units of the typed text.
    constexpr size_t kMaxChatLength = 199;

    using MacroTree::EChannel;
    using MacroTree::Line;

    // Short English command for a channel ("" for Current): "/s", "/g3"...
    const char* ShortCommand(EChannel aChannel);

    // Channel heading colour from the game's chat (as bhm-chat-macros uses), 0xRRGGBB.
    uint32_t ChannelColor(EChannel aChannel);

    Line              ParseLine(const std::string& aText);
    std::string       SerializeLine(const Line& aLine);
    // Splits on newlines; blank rows are dropped.
    std::vector<Line> ParseRaw(const std::string& aText);
    std::string       SerializeRaw(const std::vector<Line>& aLines);

    // The text typed into the chat box for this line: "/s hello", or just "hello" for Current.
    // For whispers the recipient goes in a separate box, so it isn't included.
    std::string TypedText(const Line& aLine);

    // Length as the game counts it (UTF-16 code units of UTF-8 text).
    size_t Utf16Length(const std::string& aUtf8);

    // The character counter's colour steps (docs/design.md, "Character counter").
    constexpr size_t kLengthWarnAt  = 150; // yellow from here
    constexpr size_t kLengthAlertAt = 180; // bright red from here
    enum class ELengthLevel { Normal, Near, Close, Over };
    ELengthLevel LengthLevel(size_t aLength);
}
