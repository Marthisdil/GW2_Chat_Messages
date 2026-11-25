#include "SendPlan.h"

#include <cstdint>

#include "ChatLine.h"
#include "Localization.h"

namespace SendPlan
{
    using MacroTree::EChannel;
    using MacroTree::Line;

    namespace
    {
        void Add(Plan& p, EStep t, size_t line, std::string text = {}, int ms = 0)
        {
            Step s;
            s.Type = t;
            s.Line = line;
            s.Text = std::move(text);
            s.Ms   = ms;
            p.Steps.push_back(std::move(s));
        }

        bool HasControlChar(const std::string& s) { return ChatLine::ControlsToSpaces(s) != s; }

        // The UTF-8 code point at s[i] and its length in bytes. A byte that doesn't start a valid
        // sequence counts as one character U+FFFD (what the game gets), never as a space or "/".
        size_t CodePointAt(const std::string& s, size_t i, uint32_t& aCp)
        {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            size_t len = 0;
            if (c < 0x80)                { aCp = c;        len = 1; }
            else if ((c & 0xE0) == 0xC0) { aCp = c & 0x1F; len = 2; }
            else if ((c & 0xF0) == 0xE0) { aCp = c & 0x0F; len = 3; }
            else if ((c & 0xF8) == 0xF0) { aCp = c & 0x07; len = 4; }
            else { aCp = 0xFFFD; return 1; }
            if (i + len > s.size()) { aCp = 0xFFFD; return 1; }
            for (size_t k = 1; k < len; k++)
            {
                const unsigned char cc = static_cast<unsigned char>(s[i + k]);
                if ((cc & 0xC0) != 0x80) { aCp = 0xFFFD; return 1; }
                aCp = (aCp << 6) | (cc & 0x3F);
            }
            return len;
        }

        // Spaces the game might skip before a command: ASCII and Unicode white space and the
        // invisible spaces and joiners (on purpose more than Unicode's White_Space list).
        bool IsSpaceLike(uint32_t cp)
        {
            return cp == ' ' || (cp >= 0x09 && cp <= 0x0D) || cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
                   cp == 0x180E || (cp >= 0x2000 && cp <= 0x200D) || cp == 0x2028 || cp == 0x2029 ||
                   cp == 0x202F || cp == 0x205F || cp == 0x2060 || cp == 0x3000 || cp == 0xFEFF;
        }

        // Whether a "/" chat could read as a command came from a placeholder value. aText is what's
        // typed into one box (from its start); aFromValue marks the bytes that came from a value
        // (shorter or empty = the rest was written by the user). Every "/" at the start, or right
        // after a command word and its spaces, must be the user's: a channel command empties the
        // box, so the next "/" starts a new command ("/g /w Name" whispers).
        bool ValueStartsCommand(const std::string& aText, const std::vector<bool>& aFromValue)
        {
            auto fromValue = [&](size_t k) { return k < aFromValue.size() && aFromValue[k]; };
            size_t i = 0;
            for (;;)
            {
                uint32_t cp = 0;
                while (i < aText.size())
                {
                    const size_t len = CodePointAt(aText, i, cp);
                    if (!IsSpaceLike(cp)) break;
                    i += len;
                }
                if (i >= aText.size() || aText[i] != '/') return false;
                if (fromValue(i)) return true;
                while (i < aText.size()) // the command word, up to the next space
                {
                    const size_t len = CodePointAt(aText, i, cp);
                    if (IsSpaceLike(cp)) break;
                    i += len;
                }
            }
        }

        std::vector<bool> Prefixed(size_t aOwn, const std::vector<bool>& aFromValue)
        {
            std::vector<bool> out(aOwn, false);
            out.insert(out.end(), aFromValue.begin(), aFromValue.end());
            return out;
        }
    }

    Plan Build(const std::vector<Line>& aLines, bool aIsCommander, const std::vector<Origin>& aOrigins)
    {
        Plan plan;

        if (!aOrigins.empty() && aOrigins.size() != aLines.size())
        {
            plan.Error = TR("Internal error: the lines changed while placeholders were filled in.");
            return plan;
        }

        // Validate everything before planning anything.
        for (size_t i = 0; i < aLines.size(); i++)
        {
            const Line& l = aLines[i];
            if (l.Message.empty()) continue;
            const std::string where = Localization::Fmt(TR("Line %zu: "), static_cast<size_t>(i + 1));
            if (ChatLine::Utf16Length(ChatLine::TypedText(l)) > ChatLine::kMaxChatLength)
            {
                plan.Error = where + Localization::Fmt(TR("too long for chat (over %zu characters)."), static_cast<size_t>(ChatLine::kMaxChatLength));
                return plan;
            }
            if (l.Channel == EChannel::Whisper && l.WhisperTo.empty())
            {
                plan.Error = where + TR("the whisper has no recipient.");
                return plan;
            }
            // A recipient is typed one character at a time on the game's window thread, and no
            // name is longer than this (SEC-035): a file could otherwise freeze the game.
            if (l.Channel == EChannel::Whisper && ChatLine::Utf16Length(l.WhisperTo) > ChatLine::kMaxRecipientLength)
            {
                plan.Error = where + Localization::Fmt(TR("the whisper recipient is too long (over %zu characters)."),
                                                       static_cast<size_t>(ChatLine::kMaxRecipientLength));
                return plan;
            }
            const bool whisper   = l.Channel == EChannel::Whisper;
            const bool broadcast = l.Channel == EChannel::Squad && l.SquadBroadcast && aIsCommander;
            // We type Message, and WhisperTo for whispers; the rest is our own text.
            if (HasControlChar(l.Message) || (whisper && HasControlChar(l.WhisperTo)))
            {
                plan.Error = where + TR("it has a control character (like a tab or a line break) that can't be typed into chat.");
                return plan;
            }
            if (!aOrigins.empty())
            {
                const Origin& o = aOrigins[i];
                bool message = false, to = false;
                if (whisper)
                {
                    to      = ValueStartsCommand(l.WhisperTo, o.WhisperTo); // the recipient box, after our "/w "
                    message = ValueStartsCommand(l.Message, o.Message);     // the message box, after Tab
                }
                else if (broadcast)
                    message = ValueStartsCommand(l.Message, o.Message);
                else
                {
                    // "/s hello": our own "/s " first, then the message, in one box.
                    const std::string typed = ChatLine::TypedText(l);
                    message = ValueStartsCommand(typed, Prefixed(typed.size() - l.Message.size(), o.Message));
                }
                if (message || to)
                {
                    plan.Error = where + (to ? TR("a placeholder would start a chat command in the whisper recipient (a \"/\" "
                                                  "at its start, or right after a command). Only a \"/\" you write yourself may do that.")
                                             : TR("a placeholder would start a chat command in the message (a \"/\" at its "
                                                  "start, or right after a command). Only a \"/\" you write yourself may do that."));
                    return plan;
                }
            }
        }

        for (size_t i = 0; i < aLines.size(); i++)
        {
            const Line& l = aLines[i];
            if (l.Message.empty()) continue;

            if (plan.LinesToSend > 0) Add(plan, EStep::Pause, i, {}, kPauseBetweenLinesMs);
            plan.LinesToSend++;

            const bool broadcast = l.Channel == EChannel::Squad && l.SquadBroadcast && aIsCommander;
            Add(plan, broadcast ? EStep::OpenSquadBroadcast : EStep::OpenChat, i);
            Add(plan, EStep::WaitChatOpen, i);

            if (broadcast)
            {
                // The broadcast box has no channels: just the message.
                Add(plan, EStep::Type, i, l.Message);
            }
            else if (l.Channel == EChannel::Whisper)
            {
                // "/w " moves the cursor to the recipient box; Tab moves on to the message box.
                // Each move takes the game a few frames: pause before typing into the new box.
                Add(plan, EStep::Type, i, "/w ");
                Add(plan, EStep::Pause, i, {}, kPauseAfterWhisperCommandMs);
                Add(plan, EStep::Type, i, l.WhisperTo);
                Add(plan, EStep::KeyTab, i);
                Add(plan, EStep::Pause, i, {}, kPauseAfterTabMs);
                Add(plan, EStep::Type, i, l.Message);
            }
            else
            {
                Add(plan, EStep::Type, i, ChatLine::TypedText(l)); // "/s hello", or "hello"
            }

            Add(plan, EStep::KeyEnter, i);
            Add(plan, EStep::WaitChatClosed, i);
        }

        if (plan.LinesToSend == 0) plan.Error = TR("There's nothing to send: every line is empty.");
        return plan;
    }

    Plan Prepare(const std::vector<Line>& aLines, bool aIsCommander, const Placeholders::Context& aContext,
                 const std::function<bool()>& aStop)
    {
        std::vector<Line> lines = aLines;
        std::vector<Origin> origins(lines.size()); // which bytes came from values
        for (size_t i = 0; i < lines.size(); i++)
        {
            std::pair<std::string*, std::vector<bool>*> fields[] = {{&lines[i].Message, &origins[i].Message},
                                                                     {&lines[i].WhisperTo, &origins[i].WhisperTo}};
            for (auto& [field, fromValue] : fields)
            {
                Plan stopped;
                if (aStop && aStop())
                {
                    stopped.Error = TR("Stopped.");
                    return stopped;
                }
                Placeholders::Result r = Placeholders::Expand(*field, aContext);
                if (!r.Error.empty())
                {
                    stopped.Error = Localization::Fmt(TR("Line %zu: "), static_cast<size_t>(i + 1)) + r.Error;
                    return stopped;
                }
                *field     = std::move(r.Text);
                *fromValue = std::move(r.FromValue);
            }
        }
        // Built from the filled-in text, so the 199-character and control character checks are on
        // what's really typed, and a value can't start a command (SEC-013).
        return Build(lines, aIsCommander, origins);
    }

    std::vector<Typed> Describe(const Plan& aPlan, const std::vector<Line>& aLines)
    {
        std::vector<Typed> out;
        if (!aPlan.Error.empty()) return out;
        for (const Step& s : aPlan.Steps)
        {
            if (s.Type == EStep::OpenChat || s.Type == EStep::OpenSquadBroadcast)
            {
                Typed t;
                t.Line      = s.Line;
                t.Channel   = s.Line < aLines.size() ? aLines[s.Line].Channel : EChannel::Current;
                t.Broadcast = s.Type == EStep::OpenSquadBroadcast;
                out.push_back(std::move(t));
            }
            else if (out.empty())
                continue; // every line starts by opening chat; nothing to attach this to
            else if (s.Type == EStep::KeyTab)
                out.back().Tab = true;
            else if (s.Type == EStep::Type)
                (out.back().Tab ? out.back().AfterTab : out.back().Text) += s.Text;
        }
        return out;
    }
}
