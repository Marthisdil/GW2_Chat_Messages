#include "SendPlan.h"

#include "ChatLine.h"

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
    }

    Plan Build(const std::vector<Line>& aLines, bool aIsCommander)
    {
        Plan plan;

        // Validate everything before planning anything.
        for (size_t i = 0; i < aLines.size(); i++)
        {
            const Line& l = aLines[i];
            if (l.Message.empty()) continue;
            const std::string where = "Line " + std::to_string(i + 1) + ": ";
            if (ChatLine::Utf16Length(ChatLine::TypedText(l)) > ChatLine::kMaxChatLength)
            {
                plan.Error = where + "too long for chat (over " + std::to_string(ChatLine::kMaxChatLength) + " characters).";
                return plan;
            }
            if (l.Channel == EChannel::Whisper && l.WhisperTo.empty())
            {
                plan.Error = where + "the whisper has no recipient.";
                return plan;
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
                Add(plan, EStep::Type, i, "/w ");
                Add(plan, EStep::Type, i, l.WhisperTo);
                Add(plan, EStep::KeyTab, i);
                Add(plan, EStep::Type, i, l.Message);
            }
            else
            {
                Add(plan, EStep::Type, i, ChatLine::TypedText(l)); // "/s hello", or "hello"
            }

            Add(plan, EStep::KeyEnter, i);
            Add(plan, EStep::WaitChatClosed, i);
        }

        if (plan.LinesToSend == 0) plan.Error = "There's nothing to send: every line is empty.";
        return plan;
    }
}
