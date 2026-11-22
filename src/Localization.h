#pragma once

// The addon's own text in other languages (docs/design.md, "Languages"). No Nexus or ImGui
// dependency, so it can be unit-tested (tests/LocalizationTests.cpp).
//
// The English text in the code is the key: T("Open macro library") returns the active
// language's text for it, or the English when there's none. A language file is
// localization/<id>.json:
//   { "Identifier": "de", "DisplayName": "Deutsch", "AddonVersion": "...",
//     "Texts": { "Open macro library": "Makrobibliothek öffnen", ... } }
// localization/en.json lists every English text (written by tools/localization.ps1); only its
// keys are accepted from other files.
//
// Language files can come from anyone, so every file is checked the same way, ours included
// (docs/security-review.md): size and nesting limits, valid UTF-8, no control or bidi
// characters, and a translation's %-codes must be exactly the English text's, in the same order
// (it may be used as a format string). A text that fails is skipped (English is used); a file
// that fails as a whole isn't loaded.

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Localization
{
    constexpr size_t kMaxFileBytes  = 512 * 1024; // a full translation is about 60 KB
    constexpr size_t kMaxTexts      = 4000;
    constexpr size_t kMaxTextBytes  = 2048;
    constexpr size_t kMaxNameBytes  = 64;
    constexpr size_t kMaxFiles      = 32;         // files read from the localization folder
    constexpr int    kMaxDepth      = 4;          // JSON nesting
    constexpr size_t kMaxSkipNotes  = 5;          // reasons kept per file, for the log

    struct Catalog
    {
        std::string Id;           // "de", "pt-br"
        std::string DisplayName;  // "Deutsch"
        std::string AddonVersion; // the version it was made for ("" if not given)
        std::unordered_map<std::string, std::string> Texts; // English -> translation
        size_t Skipped = 0;                    // texts left out (English is used for them)
        std::vector<std::string> SkipNotes;    // the first few reasons, for the log
    };

    // "en", "de", "pt-br": 2-3 lower-case letters, optionally "-" and 2-8 letters or digits.
    bool ValidId(const std::string& aId);

    // Text that's safe to show: valid UTF-8 (no overlong forms or surrogates), no control
    // characters (except '\n' when aAllowNewline), no bidi or invisible direction marks, no "##"
    // (ImGui's ID separator). aWhy gets the reason when it isn't.
    bool SafeText(const std::string& aText, bool aAllowNewline, std::string* aWhy = nullptr);

    // The %-codes that take an argument, in order ("%s", "%zu", "%02d", "%.1f"); "%%" is left out.
    // Returns false for anything else after a '%' (%n, %*d, %1$s, a '%' at the end...).
    bool Placeholders(const std::string& aText, std::vector<std::string>& aOut);
    // Whether aTranslation may stand in for aEnglish: both parse and the codes are the same.
    bool SamePlaceholders(const std::string& aEnglish, const std::string& aTranslation);

    // Parses and checks a language file. aKnown: the English texts that exist (en.json); other
    // keys are skipped. nullptr accepts every key (only for en.json itself). Returns false, with
    // aError in plain words, when the file as a whole is refused.
    bool ParseCatalog(const std::string& aJson, const std::unordered_set<std::string>* aKnown,
                      Catalog& aOut, std::string& aError);

    // How much of aKnown a catalog translates, 0-100 (rounded down; 100 only when complete).
    int PercentDone(const Catalog& aCatalog, const std::unordered_set<std::string>& aKnown);

    // "Label##id" -> "Label" and "##id"; "###id" too. No "##": aVisible is the whole text.
    void SplitId(const std::string& aText, std::string& aVisible, std::string& aSuffix);

    // --- The active language (render thread only) ------------------------------------------
    // aCatalog stays owned by the caller and must outlive its use; nullptr = English.
    void SetActive(const Catalog* aCatalog);
    const Catalog* Active();
    // The text for an English literal. The result is valid until the next SetActive: use it
    // straight away, never keep it. Results are cached per literal address.
    const char* Lookup(const char* aEnglishLiteral);
    // For text that isn't a literal (not cached).
    std::string LookupText(const std::string& aEnglish);

    // printf into a std::string (for translated format strings built into labels).
    std::string Fmt(const char* aFormat, ...);
}

// TX("English text"): marks a literal for tools/localization.ps1 without translating it where it
// stands (tables in the core modules; Ui.cpp translates them with Localization::Lookup).
#define TX(aEnglish) aEnglish

// T("English text"): only string literals (their address is the cache key).
template <size_t N>
inline const char* T(const char (&aEnglish)[N]) { return Localization::Lookup(aEnglish); }
// A writable char buffer isn't a literal (its address would cache the wrong text): refused.
template <size_t N>
const char* T(char (&aBuffer)[N]) = delete;
