#pragma once

// Paths <-> UTF-8 text that never throws (SEC-020, SEC-024). std::filesystem's u8string() and
// u8path() throw std::system_error on a file name with an unpaired surrogate or on invalid UTF-8
// (a text box can hand us one), and on the render thread that would leave the Nexus callback.
// Here a bad sequence becomes U+FFFD instead. No Nexus, ImGui or windows.h dependency.
// Tested by tests/PathTextTests.cpp.

#include <filesystem>
#include <string>

namespace PathText
{
    // UTF-16 (Windows wide text) to UTF-8; an unpaired surrogate becomes U+FFFD.
    std::string FromWide(const std::wstring& aText);

    // UTF-8 to UTF-16; an invalid or overlong sequence, or an encoded surrogate, becomes U+FFFD.
    std::wstring ToWide(const std::string& aText);

    // A path's text as UTF-8: use it instead of path::u8string().
    inline std::string Utf8(const std::filesystem::path& aPath) { return FromWide(aPath.wstring()); }

    // A path from UTF-8 text: use it instead of std::filesystem::u8path().
    inline std::filesystem::path FromUtf8(const std::string& aText) { return std::filesystem::path(ToWide(aText)); }
}
