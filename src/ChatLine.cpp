#include "ChatLine.h"

#include <cctype>
#include <utility>

namespace ChatLine
{
    namespace
    {
        // Every command the game accepts for a channel in the English, German, French and Spanish
        // clients (from bhm-chat-macros' localized resources). The first entry is what we write.
        // "{n}" in a guild entry means a digit 1-6 follows directly.
        struct ChannelCommands { EChannel Channel; std::vector<std::string> Commands; };

        const std::vector<ChannelCommands>& Table()
        {
            static const std::vector<ChannelCommands> table = {
                {EChannel::Emote,   {"/e", "/emote"}},
                {EChannel::Say,     {"/s", "/say", "/sagen", "/l", "/dire", "/local"}},
                {EChannel::Map,     {"/m", "/map", "/k", "/karte", "/ca", "/carte", "/mapa"}},
                {EChannel::Party,   {"/p", "/party", "/gr", "/gruppe", "/groupe", "/grupo"}},
                {EChannel::Squad,   {"/d", "/squad", "/einsatztrupp", "/es", "/escouade", "/escuadr\xC3\xB3n"}},
                {EChannel::Team,    {"/t", "/team", "/eq", "/\xC3\xA9quipe", "/equipo"}},
                {EChannel::Reply,   {"/r", "/reply", "/a", "/antworten", "/r\xC3\xA9pondre", "/responder"}},
                {EChannel::Whisper, {"/w", "/whisper", "/f", "/fl\xC3\xBCstern", "/ch", "/chuchoter", "/su", "/susurrar"}},
                {EChannel::Guild,   {"/g", "/guild", "/gilde", "/guilde", "/c", "/clan"}},
            };
            return table;
        }

        std::string LowerAscii(std::string s)
        {
            for (char& c : s)
                if (static_cast<unsigned char>(c) < 0x80) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        // If aWord is a channel command, returns true and sets aChannel. Handles "/g3", "/guild3"...
        bool MatchCommand(const std::string& aWord, EChannel& aChannel)
        {
            const std::string w = LowerAscii(aWord);
            for (const ChannelCommands& cc : Table())
            {
                for (const std::string& cmd : cc.Commands)
                {
                    if (w == cmd) { aChannel = cc.Channel; return true; }
                    if (cc.Channel == EChannel::Guild && w.size() == cmd.size() + 1 && w.compare(0, cmd.size(), cmd) == 0)
                    {
                        const char d = w.back();
                        if (d >= '1' && d <= '6')
                        {
                            aChannel = static_cast<EChannel>(static_cast<int>(EChannel::Guild1) + (d - '1'));
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        std::string TrimRight(const std::string& s)
        {
            size_t end = s.size();
            while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) end--;
            return s.substr(0, end);
        }

        // Removes one space (or tab) at the front, like typing "/s hello".
        std::string DropOneSpace(const std::string& s)
        {
            return !s.empty() && (s[0] == ' ' || s[0] == '\t') ? s.substr(1) : s;
        }
    }

    const char* ShortCommand(EChannel c)
    {
        switch (c)
        {
        case EChannel::Emote:   return "/e";
        case EChannel::Say:     return "/s";
        case EChannel::Map:     return "/m";
        case EChannel::Party:   return "/p";
        case EChannel::Squad:   return "/d";
        case EChannel::Team:    return "/t";
        case EChannel::Reply:   return "/r";
        case EChannel::Whisper: return "/w";
        case EChannel::Guild:   return "/g";
        case EChannel::Guild1:  return "/g1";
        case EChannel::Guild2:  return "/g2";
        case EChannel::Guild3:  return "/g3";
        case EChannel::Guild4:  return "/g4";
        case EChannel::Guild5:  return "/g5";
        case EChannel::Guild6:  return "/g6";
        default:                return "";
        }
    }

    uint32_t ChannelColor(EChannel c)
    {
        // The game's chat heading colours, brightened by 1.25 like bhm-chat-macros does.
        auto bright = [](int r, int g, int b) -> uint32_t
        {
            auto up = [](int v) { v = v * 5 / 4; return static_cast<uint32_t>(v > 255 ? 255 : v); };
            return (up(r) << 16) | (up(g) << 8) | up(b);
        };
        switch (c)
        {
        case EChannel::Emote:   return bright(136, 136, 136);
        case EChannel::Say:     return bright(118, 217, 140);
        case EChannel::Map:     return bright(170, 79, 68);
        case EChannel::Party:   return bright(63, 155, 229);
        case EChannel::Squad:   return bright(173, 218, 91);
        case EChannel::Team:    return bright(221, 48, 49);
        case EChannel::Reply:
        case EChannel::Whisper: return bright(177, 78, 169);
        case EChannel::Guild:   return bright(204, 150, 42);
        case EChannel::Guild1:
        case EChannel::Guild2:
        case EChannel::Guild3:
        case EChannel::Guild4:
        case EChannel::Guild5:
        case EChannel::Guild6:  return bright(141, 129, 86);
        default:                return 0xFFFFFF;
        }
    }

    std::string ControlsToSpaces(const std::string& aText)
    {
        std::string out;
        out.reserve(aText.size());
        for (size_t i = 0; i < aText.size(); i++)
        {
            const unsigned char c = static_cast<unsigned char>(aText[i]);
            if (c < 0x20 || c == 0x7F) { out += ' '; continue; }
            // C1 is only 0xC2 then 0x80-0x9F; other 0x80-0x9F bytes belong to ordinary characters.
            if (c == 0xC2 && i + 1 < aText.size())
            {
                const unsigned char next = static_cast<unsigned char>(aText[i + 1]);
                if (next >= 0x80 && next <= 0x9F) { out += ' '; i++; continue; }
            }
            out += static_cast<char>(c);
        }
        return out;
    }

    Line ParseLine(const std::string& aText)
    {
        Line line;
        // A tab already counted as a space below, so it parses the same; CR, Esc etc. can't be typed.
        const std::string text = TrimRight(ControlsToSpaces(aText));

        // The command is the first word, cut early at '[' for whispers and '!' for squad.
        size_t end = 0;
        while (end < text.size() && text[end] != ' ' && text[end] != '\t' && text[end] != '[' && text[end] != '!') end++;
        EChannel channel;
        if (text.empty() || text[0] != '/' || !MatchCommand(text.substr(0, end), channel))
        {
            line.Message = text; // current channel; keeps "/wave" and other game commands as typed
            return line;
        }

        line.Channel = channel;
        std::string rest = text.substr(end);
        // Optional parameter, before or after one space: "/w [Name] hi", "/w[Name] hi", "/d ! hi", "/d! hi".
        const std::string afterSpace = DropOneSpace(rest);
        if (channel == EChannel::Whisper && !afterSpace.empty() && afterSpace[0] == '[')
        {
            const size_t close = afterSpace.find(']');
            if (close != std::string::npos)
            {
                line.WhisperTo = afterSpace.substr(1, close - 1);
                rest = afterSpace.substr(close + 1);
            }
        }
        else if (channel == EChannel::Squad && !afterSpace.empty() && afterSpace[0] == '!')
        {
            line.SquadBroadcast = true;
            rest = afterSpace.substr(1);
        }
        line.Message = DropOneSpace(rest);
        return line;
    }

    std::string SerializeLine(const Line& aLine)
    {
        const std::string cmd = ShortCommand(aLine.Channel);
        if (cmd.empty()) return aLine.Message;
        std::string out = cmd;
        if (aLine.Channel == EChannel::Whisper && !aLine.WhisperTo.empty()) out += " [" + aLine.WhisperTo + "]";
        if (aLine.Channel == EChannel::Squad && aLine.SquadBroadcast) out += " !";
        out += " " + aLine.Message;
        return out;
    }

    std::vector<Line> ParseRaw(const std::string& aText)
    {
        std::vector<Line> lines;
        size_t start = 0;
        while (start <= aText.size())
        {
            size_t nl = aText.find('\n', start);
            if (nl == std::string::npos) nl = aText.size();
            const std::string row = TrimRight(ControlsToSpaces(aText.substr(start, nl - start)));
            if (row.find_first_not_of(" \t") != std::string::npos) lines.push_back(ParseLine(row));
            start = nl + 1;
        }
        return lines;
    }

    std::string SerializeRaw(const std::vector<Line>& aLines)
    {
        std::string out;
        for (size_t i = 0; i < aLines.size(); i++)
        {
            if (i) out += '\n';
            out += SerializeLine(aLines[i]);
        }
        return out;
    }

    std::string TypedText(const Line& aLine)
    {
        const std::string cmd = ShortCommand(aLine.Channel);
        return cmd.empty() ? aLine.Message : cmd + " " + aLine.Message;
    }

    ELengthLevel LengthLevel(size_t aLength)
    {
        if (aLength > kMaxChatLength) return ELengthLevel::Over;
        if (aLength >= kLengthAlertAt) return ELengthLevel::Close;
        if (aLength >= kLengthWarnAt) return ELengthLevel::Near;
        return ELengthLevel::Normal;
    }

    size_t Utf16Length(const std::string& s)
    {
        size_t units = 0;
        for (size_t i = 0; i < s.size(); i++)
        {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if ((c & 0xC0) == 0x80) continue;  // continuation byte
            units += (c >= 0xF0) ? 2 : 1;       // 4-byte sequences are surrogate pairs in UTF-16
        }
        return units;
    }

    bool RepairUtf8(std::string& aText)
    {
        const auto at = [&aText](size_t k) { return static_cast<unsigned char>(aText[k]); };
        std::string out;
        bool repaired = false;
        size_t i = 0, kept = 0; // aText[kept, i) is valid and not yet copied to out
        while (i < aText.size())
        {
            const unsigned char c = at(i);
            if (c < 0x80) { i++; continue; }
            // Well-formed sequences (Unicode Table 3-7): the length, and the second byte's range.
            size_t len = 0;
            unsigned char lo = 0x80, hi = 0xBF;
            if (c >= 0xC2 && c <= 0xDF) len = 2;
            else if (c == 0xE0) { len = 3; lo = 0xA0; } // no overlong forms
            else if (c == 0xED) { len = 3; hi = 0x9F; } // no surrogates
            else if (c >= 0xE1 && c <= 0xEF) len = 3;
            else if (c == 0xF0) { len = 4; lo = 0x90; } // no overlong forms
            else if (c == 0xF4) { len = 4; hi = 0x8F; } // nothing past U+10FFFF
            else if (c >= 0xF1 && c <= 0xF3) len = 4;
            size_t n = len ? 1 : 0; // bytes that start a well-formed sequence
            while (n && n < len && i + n < aText.size())
            {
                const unsigned char b = at(i + n);
                if (b < (n == 1 ? lo : 0x80) || b > (n == 1 ? hi : 0xBF)) break;
                n++;
            }
            if (len && n == len) { i += len; continue; }
            // Not UTF-8: that start (or the one stray byte) becomes one U+FFFD.
            out.append(aText, kept, i - kept);
            out += "\xEF\xBF\xBD";
            i += n ? n : 1;
            kept = i;
            repaired = true;
        }
        if (!repaired) return false;
        out.append(aText, kept, std::string::npos);
        aText = std::move(out);
        return true;
    }
}
