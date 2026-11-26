#include "Welcome.h"

#include "Localization.h"

Welcome::Start Welcome::Decide(bool aExamplesAdded, bool aWelcomeShown, bool aNoLibrary, bool aTreeEmpty)
{
    Start s;
    if (aWelcomeShown) return s; // answered before ("Start empty" leaves "examples_added" unset)
    if (!aExamplesAdded && aNoLibrary && aTreeEmpty)
    {
        s.ShowWelcome = true; // both settings are saved when the user answers
        return s;
    }
    s.MarkShown         = true;
    s.MarkExamplesAdded = !aExamplesAdded;
    return s;
}

bool Welcome::NeedsMacro(ETarget aTarget)
{
    return aTarget == ETarget::Header || aTarget == ETarget::Lines || aTarget == ETarget::Sections;
}

const std::vector<Welcome::Step>& Welcome::Steps()
{
    static const std::vector<Step> steps = {
        {ETarget::Tree, TX("Your library"),
         TX("Your folders and macros. Folders can hold folders, up to 6 levels. Drag a macro or folder to move it, "
            "and right-click one for more: rename, duplicate, delete, favourite.")},
        {ETarget::Toolbar, TX("New folder and New macro"),
         TX("Make a folder or a macro here. It goes into the folder that's selected. Import and Export share "
            "macros with other players.")},
        {ETarget::Search, TX("Search"),
         TX("Finds macros by name, text, whisper recipient, hotkey or note. Tick \"Only active...\" next to it to see "
            "just the macros that can be sent on this map.")},
        {ETarget::Header, TX("Send now and Preview"),
         TX("The selected macro. Send now types its lines into chat, one after the other. Preview shows what would "
            "be typed, with placeholders filled in, and sends nothing. \"...\" renames, duplicates or deletes it.")},
        {ETarget::Lines, TX("The lines"),
         TX("Each line goes to its own channel: say, party, squad, a whisper and more. The count on the right "
            "shows how close a line is to chat's limit of 199 characters. Placeholders (?) lists what you can "
            "fill in, like the time or a random number.")},
        {ETarget::Sections, TX("Where it's active, hotkey and note"),
         TX("Click a heading to open or close it. Where it's active limits a macro to some maps or game modes. "
            "A hotkey sends it with one key press. A note says what it's for and is never sent.")},
        {ETarget::None, TX("The quick menu"),
         TX("Click the speech bubble with the lightning bolt in the Nexus bar to open this library. Right-click it "
            "for the quick menu: your favourites and recent macros, ready to send. You can set keys for both in "
            "Nexus > Options > Keybinds.")},
        {ETarget::None, TX("Options"),
         TX("The addon's options are in Nexus > Options, under Chat Messages: text size, language, the trash bin, "
            "backups and more. Features for experienced players are behind the Advanced options box there. You "
            "can run this tour again from that page too.")},
    };
    return steps;
}
