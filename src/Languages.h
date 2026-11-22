#pragma once

// Which language the addon's own text is shown in (docs/design.md, "Languages"): the built-in
// files (localization/*.json, embedded as resources) plus any the user puts in
// <addon>\localization\, following the Nexus language unless the user picks one.
// Render thread only. The checks on every file are in Localization.*.

#include <filesystem>
#include <string>
#include <vector>

namespace Languages
{
    constexpr const char* kFollowNexus = "nexus"; // the "language" setting's default

    struct Entry
    {
        std::string Id;           // "de"
        std::string DisplayName;  // "Deutsch"
        bool        FromFolder = false; // a file in <addon>\localization\ (not shipped with the addon)
        std::string File;         // its file name, when FromFolder
        std::string AddonVersion; // the version the file was made for
        int         Percent = 100;
        size_t      Skipped = 0;  // texts left out by the checks
    };

    // Registers a text Nexus shows for us (a keybind's name, the Quick Access tooltip) so it's
    // translated in Nexus too. Call before Init.
    void AddNexusText(const char* aIdentifier, const char* aEnglish);

    void Init();     // loads the files, reads the setting, tells Nexus our texts
    void Shutdown(); // back to English, frees the files
    // Once per frame, before anything is drawn: applies a choice or reload made last frame and,
    // about once a second, follows a change of the Nexus language.
    void Update();

    // English first, then the others by name.
    const std::vector<Entry>& List();
    const std::string& Choice();      // kFollowNexus or a language id
    void Choose(const std::string& aId); // saved; applied next frame
    void RequestReload();             // reads the folder again next frame
    std::string NexusLanguage();      // what Nexus uses now ("en" if unknown)
    std::string ActiveId();           // what's shown now
    std::filesystem::path Folder();   // <addon>\localization, created on first call

    // Problems found while loading (a file that was refused, texts skipped), in plain words, for
    // the error window. Taken once.
    std::vector<std::string> TakeProblems();
}
