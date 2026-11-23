#include "LinkSync.h"

#include <cctype>
#include <cwctype>

#include "ChatLine.h"
#include "PathText.h"

namespace fs = std::filesystem;

namespace
{
    std::string Lower(std::string s)
    {
        for (char& c : s)
            if (static_cast<unsigned char>(c) < 0x80) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::wstring LowerW(std::wstring s)
    {
        for (wchar_t& c : s)
            if (c < 0x80) c = static_cast<wchar_t>(std::towlower(c));
        return s;
    }
}

namespace LinkSync
{
    std::string CheckPath(const std::string& aPath)
    {
        const size_t first = aPath.find_first_not_of(" \t");
        if (first == std::string::npos) return "Type a file name, like \"callouts.txt\".";
        const std::string p = aPath.substr(first, aPath.find_last_not_of(" \t") - first + 1);
        for (char c : p)
            if (static_cast<unsigned char>(c) < 0x20 || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                return "The file name has a character Windows doesn't allow.";
        if (p.size() < 5 || Lower(p.substr(p.size() - 4)) != ".txt") return "The file name must end in \".txt\".";
        const char last = p[p.size() - 5];
        if (last == '\\' || last == '/') return "Type a file name, like \"callouts.txt\".";
        return {};
    }

    fs::path Resolve(const fs::path& aAddonDir, const std::string& aPath)
    {
        const size_t first = aPath.find_first_not_of(" \t");
        const std::string p = first == std::string::npos ? std::string() : aPath.substr(first, aPath.find_last_not_of(" \t") - first + 1);
        const fs::path path = PathText::FromUtf8(p);
        if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) return path.lexically_normal();
        return (aAddonDir / "links" / path).lexically_normal();
    }

    bool SamePath(const fs::path& aA, const fs::path& aB)
    {
        return LowerW(aA.lexically_normal().wstring()) == LowerW(aB.lexically_normal().wstring());
    }

    std::string FileText(const std::vector<Line>& aLines)
    {
        const std::string raw = ChatLine::SerializeRaw(aLines);
        std::string out;
        out.reserve(raw.size() + aLines.size() + 2);
        for (char c : raw)
        {
            if (c == '\n') out += "\r\n";
            else out += c;
        }
        if (!out.empty()) out += "\r\n";
        return out;
    }

    Outcome Sync(State& aState, const std::vector<Line>& aMacroLines, const File& aFile)
    {
        Outcome out;
        if (!aFile.Exists)
        {
            out.Missing = true;
            return out;
        }

        const std::string macroText = ChatLine::SerializeRaw(aMacroLines);

        // The file changed (or was never read): read it, and if its lines differ, it wins.
        if (!aState.Known || aFile.Mtime != aState.Mtime)
        {
            std::string text;
            if (!aFile.Read || !aFile.Read(text, out.Error))
            {
                if (out.Error.empty()) out.Error = "the file couldn't be read.";
                return out;
            }
            // Not UTF-8 (saved as Windows-1252, say): those bytes are replaced, the same way on
            // every read, so the file isn't written back for it (only when the macro is edited).
            if (ChatLine::RepairUtf8(text))
            {
                // Each replaced byte takes three: written back, it would be past the read limit and
                // the link would stop syncing (SEC-023).
                if (text.size() > kMaxFileBytes)
                {
                    out.Error = "it isn't saved as UTF-8, and with its characters replaced it's over 256 KB.";
                    return out;
                }
                out.Warning = "The file isn't saved as UTF-8, so some of its characters couldn't be read and were replaced. "
                              "Save it as UTF-8 to keep them.";
            }
            std::vector<Line> lines = ChatLine::ParseRaw(text);
            if (lines.size() > MacroTree::kMaxLines)
            {
                lines.resize(MacroTree::kMaxLines);
                if (!out.Warning.empty()) out.Warning += " ";
                out.Warning += "The file has more than " + std::to_string(MacroTree::kMaxLines) + " lines. Only the first " +
                               std::to_string(MacroTree::kMaxLines) + " are used.";
            }
            const std::string fileText = ChatLine::SerializeRaw(lines);
            const bool firstSync = !aState.Known;
            const bool fileChanged = firstSync || fileText != aState.Text;
            aState.Known = true;
            aState.Mtime = aFile.Mtime;
            if (fileChanged)
            {
                aState.Text = fileText;
                if (fileText != macroText)
                {
                    out.LoadMacro = true;
                    out.Lines = std::move(lines);
                }
                return out;
            }
        }

        // The file is as last agreed: if the macro was edited in the addon, write it out.
        if (macroText != aState.Text)
        {
            std::string text = FileText(aMacroLines);
            if (text.size() > kMaxFileBytes) // it couldn't be read back (SEC-023): tried again after each edit
            {
                out.Warning = "The macro is too big for its link file (over 256 KB), so the file wasn't updated.";
                return out;
            }
            out.WriteFile = true;
            out.Text = std::move(text);
            aState.Text = macroText;
        }
        return out;
    }
}
