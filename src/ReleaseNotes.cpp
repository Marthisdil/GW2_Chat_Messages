#include "ReleaseNotes.h"

namespace
{
    std::string TrimEnd(std::string s)
    {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
        return s;
    }

    size_t LeadingSpaces(const std::string& s)
    {
        size_t n = 0;
        while (n < s.size() && s[n] == ' ') n++;
        return n;
    }

    // **bold** and `code` lose their marks; [text](url) keeps the text.
    std::string Clean(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); i++)
        {
            if (s.compare(i, 2, "**") == 0) { i++; continue; }
            if (s[i] == '`') continue;
            if (s[i] == '[')
            {
                const size_t close = s.find(']', i + 1);
                const size_t again = s.find('[', i + 1); // "[x [y](z)": only the inner one is a link
                if (close != std::string::npos && again > close && close + 1 < s.size() && s[close + 1] == '(')
                {
                    const size_t end = s.find(')', close + 2);
                    if (end != std::string::npos)
                    {
                        out += Clean(s.substr(i + 1, close - i - 1));
                        i = end;
                        continue;
                    }
                }
            }
            out += s[i];
        }
        return out;
    }
}

bool ReleaseNotes::ShowFromSettings(const nlohmann::json& aSettings)
{
    if (!aSettings.is_object()) return true;
    const auto it = aSettings.find(kShowSetting);
    return it == aSettings.end() || !it->is_boolean() || it->get<bool>();
}

ReleaseNotes::EChannel ReleaseNotes::ParseChannel(const std::string& aText)
{
    std::string t;
    for (char c : aText) t += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    if (t == "dev") return EChannel::Dev;
    if (t == "beta") return EChannel::Beta;
    if (t == "rel") return EChannel::Rel;
    return EChannel::None;
}

bool ReleaseNotes::ChannelShows(EChannel aRelease, EChannel aRunning)
{
    if (aRelease == EChannel::None || aRunning == EChannel::None) return true;
    return static_cast<int>(aRelease) >= static_cast<int>(aRunning); // Dev < Beta < Rel
}

bool ReleaseNotes::Version::operator<(const Version& o) const
{
    for (int i = 0; i < 4; i++)
        if (Part[i] != o.Part[i]) return Part[i] < o.Part[i];
    return false;
}

bool ReleaseNotes::Version::operator==(const Version& o) const
{
    for (int i = 0; i < 4; i++)
        if (Part[i] != o.Part[i]) return false;
    return true;
}

bool ReleaseNotes::ParseVersion(const std::string& aText, Version& aOut)
{
    Version v;
    size_t pos = 0;
    for (int i = 0; i < 4; i++)
    {
        size_t digits = 0;
        int value = 0;
        while (pos < aText.size() && aText[pos] >= '0' && aText[pos] <= '9' && digits < 4)
        {
            value = value * 10 + (aText[pos] - '0');
            pos++;
            digits++;
        }
        if (digits == 0) return false;
        if (pos < aText.size() && aText[pos] >= '0' && aText[pos] <= '9') return false; // more than 4 digits
        v.Part[i] = value;
        if (i < 3)
        {
            if (pos >= aText.size() || aText[pos] != '.') return false;
            pos++;
        }
    }
    if (pos < aText.size() && aText[pos] != '.') return false; // a suffix starts with "."
    aOut = v;
    return true;
}

std::vector<ReleaseNotes::Section> ReleaseNotes::Parse(const std::string& aChangelog)
{
    std::vector<Section> out;
    Section* cur = nullptr;
    size_t start = 0;
    while (start <= aChangelog.size())
    {
        size_t end = aChangelog.find('\n', start);
        if (end == std::string::npos) end = aChangelog.size();
        const std::string line = TrimEnd(aChangelog.substr(start, end - start));
        start = end + 1;
        if (line.compare(0, 4, "## [") == 0)
        {
            const size_t close = line.find(']', 4);
            const std::string ver = close == std::string::npos ? std::string() : line.substr(4, close - 4);
            Version v;
            if (ParseVersion(ver, v))
            {
                // The channel, if the heading ends with "(Dev)", "(Beta)" or "(Rel)".
                EChannel channel = EChannel::None;
                const size_t open = line.rfind('(');
                if (open != std::string::npos && open > close && line.back() == ')')
                    channel = ParseChannel(line.substr(open + 1, line.size() - open - 2));
                out.push_back({ver, channel, std::string()});
                cur = &out.back();
            }
            else cur = nullptr; // [Unreleased], or anything else
            continue;
        }
        if (line.compare(0, 3, "## ") == 0 || line.compare(0, 2, "# ") == 0) { cur = nullptr; continue; }
        if (cur)
        {
            if (!cur->Body.empty() || !line.empty()) cur->Body += line + "\n";
        }
        if (end == aChangelog.size()) break;
    }
    for (Section& s : out)
        while (!s.Body.empty() && (s.Body.back() == '\n')) s.Body.pop_back();
    return out;
}

std::vector<ReleaseNotes::Section> ReleaseNotes::Since(const std::vector<Section>& aSections, const std::string& aSeen,
                                                       const std::string& aCurrent, const std::string& aChannel)
{
    std::vector<Section> out;
    Version current, seen;
    if (!ParseVersion(aCurrent, current)) return out;
    const bool hasSeen = ParseVersion(aSeen, seen);
    const EChannel running = ParseChannel(aChannel);
    for (const Section& s : aSections)
    {
        Version v;
        if (!ParseVersion(s.Version, v)) continue;
        if (s.Body.find_first_not_of(" \t\r\n") == std::string::npos) continue; // nothing to show
        if (!ChannelShows(s.Channel, running)) continue;                          // another channel's
        if (current < v) continue;                           // newer than this DLL
        if (hasSeen ? !(seen < v) : !(v == current)) continue; // already seen, or only the current one
        out.push_back(s);
    }
    // Newest first, whatever the file's order.
    for (size_t i = 1; i < out.size(); i++)
        for (size_t j = i; j > 0; j--)
        {
            Version a, b;
            ParseVersion(out[j - 1].Version, a);
            ParseVersion(out[j].Version, b);
            if (a < b) std::swap(out[j - 1], out[j]);
            else break;
        }
    return out;
}

ReleaseNotes::Start ReleaseNotes::Decide(bool aShowSetting, bool aFirstRun, const std::string& aSeen,
                                         const std::string& aCurrent, bool aHasSections)
{
    Start s;
    Version current, seen;
    if (!ParseVersion(aCurrent, current)) return s;
    const bool hasSeen = ParseVersion(aSeen, seen);
    if (hasSeen && !(seen < current))
    {
        // Nothing new. An older DLL is remembered, so the next update shows what's newer than it.
        s.Remember = current < seen;
        return s;
    }
    if (aFirstRun || !aShowSetting || !aHasSections)
    {
        s.Remember = true;
        return s;
    }
    s.Show = true;
    return s;
}

std::vector<ReleaseNotes::Block> ReleaseNotes::ToBlocks(const std::string& aBody)
{
    std::vector<Block> out;
    bool open = false; // the last block takes continuation lines
    size_t start = 0;
    while (start < aBody.size())
    {
        size_t end = aBody.find('\n', start);
        if (end == std::string::npos) end = aBody.size();
        const std::string line = TrimEnd(aBody.substr(start, end - start));
        start = end + 1;
        const size_t lead = LeadingSpaces(line);
        const std::string rest = line.substr(lead);
        if (rest.empty()) { open = false; continue; }
        if (rest.compare(0, 2, "# ") == 0 || rest.compare(0, 3, "## ") == 0 || rest.compare(0, 4, "### ") == 0 ||
            rest.compare(0, 5, "#### ") == 0)
        {
            const size_t hashes = rest.find(' ');
            out.push_back({EKind::Heading, 0, Clean(rest.substr(hashes + 1))});
            open = false;
            continue;
        }
        if (rest.compare(0, 2, "- ") == 0 || rest.compare(0, 2, "* ") == 0)
        {
            out.push_back({EKind::Bullet, static_cast<int>(lead / 2), Clean(rest.substr(2))});
            open = true;
            continue;
        }
        if (open && !out.empty())
        {
            out.back().Text += " " + Clean(rest);
            continue;
        }
        out.push_back({EKind::Text, 0, Clean(rest)});
        open = true;
    }
    return out;
}
