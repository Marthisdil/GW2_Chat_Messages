# GW2 Chat Messages

Chat macros for Guild Wars 2, as a [Nexus](https://raidcore.gg/Nexus) addon.

> **Status:** early development. All the features below are built, but **none of them has been
> tested in game yet**, and there is no release to download.
> See [docs/design.md](docs/design.md) for the plan.

## What it does

GW2 Chat Messages keeps your chat macros in a library you organise yourself, and types them into
chat for you, from a hotkey, a menu or a button.

<!-- screenshot: library-window.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the macro library window.*

- **Folders:** keep macros in folders nested up to 6 levels deep (for example
  expansion › map › event), or at the top level. Drag and drop to rearrange, or sort A–Z.
  A search box finds macros by name, line text or hotkey.
- **Multi-line macros:** each macro sends one or more lines, and each line goes to its own
  channel: say, map, party, squad (with squad broadcast when you're commander), team, guild 1–6,
  whisper, emote, reply, or whichever channel is open. Edit them line by line, or as raw text
  typed the way you would in game (`/s hi`, `/w [Name] hi`). German, French and Spanish chat
  commands work too.
- **Three ways to send:**
  - **Hotkeys:** give any macro a key, with Ctrl, Alt or Shift if you like. Mouse 3, 4 and 5
    work too, with Ctrl, Alt or Shift. The left and right mouse buttons can never be hotkeys,
    not even with Ctrl, Alt or Shift: you click them all the time, and in GW2 Ctrl+click and
    Shift+click already do things (chat links, targeting), so a macro would go off by accident.
    The editor warns when a key is one of GW2's default keys (like F1 or Ctrl+T).
  - **Quick menu:** right-click the chat bubble icon in the Nexus bar, or press a key you choose, to
    pick a macro from nested menus, with your favourites and the macros you sent last at the top.
  - **Send now** in the library. **Preview**, next to it, shows exactly what would be typed
    (placeholders filled in, channel commands added) without sending anything.
- **Filters:** limit a macro, or a whole folder, to PvE, WvW or PvP, and to certain maps. A
  folder's filter applies to everything inside it, on top of each macro's own. Hotkeys and the
  quick menu only use macros that are active where you are, so one key can do different things
  in WvW and in PvE. Find maps by name (or pick one you've been to); names come from the official GW2 API.
- **Placeholders:** `{time}`, `{today}`, `{random 1 6}`, `{map}` and more, filled in when the
  message is sent.
- **Safe input:** it never interferes with normal game controls. Keys go only to the game window,
  chat opens with your own Chat key, your Tab, Shift and Ctrl are never pressed or released for
  you, and a macro won't fire while you're typing.
- **Import and export:** share or back up the whole library, a folder or one macro, as `.json`
  or `.txt` files or through the clipboard. Imports show a preview first (new, merged, renamed,
  replaced), and the last import can be undone. A friend's whole library (their `macros.json`)
  can be imported into a folder of its own.
- **Text file links:** keep a macro in step with a `.txt` file, both ways: edit it in Notepad
  and the macro follows, edit the macro and the file follows.
- **Trash bin:** deleted folders and macros can be restored, until you empty the bin (can be
  turned off).
- **Backups:** copies of your library every few hours, days, weeks or months, as many as you
  choose, named with the date and time in UTC; a weekly safety copy if you leave them off. The
  Restore tab puts one back (with undo).
- **Text size:** make the addon's text bigger (or smaller) with the fonts Nexus provides; the
  windows resize to match.
- **Languages:** English, German, French and Spanish, following Nexus' language or your own
  choice. Every message and key name is translated, and map names come in your language where
  the GW2 API has them. More languages can be added with a file (see [Contributing](#contributing)).

<!-- screenshot: chat-result.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: a multi-line macro sent to several chat channels.*

<!-- screenshot: quick-menu.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the quick menu.*

## Quick start

1. Install [Nexus](https://raidcore.gg/Nexus), then copy `GW2_Chat_Messages.dll` into
   `<GW2>\addons\`.
2. In Nexus > Options > Game Keybinds, make sure your **Chat** key is set (and **Squad
   Broadcast** if you're a commander). The addon uses them to open chat.
3. Left-click the chat bubble icon in the Nexus bar to open the macro library. The **Examples**
   folder shows what a macro can do. Click **New macro**, add a line, pick a channel and type a
   message.
4. Press **Send now**, set a hotkey in the macro editor, or right-click the chat bubble icon for the
   quick menu.

Your macros are saved in `<GW2>\addons\GW2_Chat_Messages\macros.json`.

## Macro use and ArenaNet's policy

This addon is for **your convenience**: it types chat messages that you start yourself (a
hotkey, the quick menu or **Send now**) into the channels you chose. One press sends one macro
once. It never sends on a timer or while you're away, and it doesn't use skills, move your
character or press any other game key.

**Don't try to use it to automate gameplay**, to play or stay "active" while you're away, or to
spam other players. ArenaNet allows macros only while you're at the keyboard, and only when they
give no unfair advantage: read their
[Policy: Macros and Macro Use](https://help.guildwars2.com/hc/en-us/articles/360013762153-Policy-Macros-and-Macro-Use) and
[Policy: Third-Party Programs](https://help.guildwars2.com/hc/en-us/articles/360013625034-Policy-Third-Party-Programs).
The addon's options page in Nexus links to the macro policy too.

GW2 Chat Messages is unofficial and isn't made, reviewed or endorsed by ArenaNet. You use it at
your own risk and are responsible for following ArenaNet's rules. More in the wiki's
[ArenaNet's policies](https://github.com/Marthisdil/GW2_Chat_Messages/wiki/ArenaNet-Policies) page.

## Documentation

Full usage documentation is in the [wiki](https://github.com/Marthisdil/GW2_Chat_Messages/wiki): installation, the macro
library, editing and sending macros, filters, placeholders, import and export, languages, files, backups,
troubleshooting and ArenaNet's policies.

## Building from source

```
git clone --recurse-submodules https://github.com/Marthisdil/GW2_Chat_Messages.git
msbuild GW2_Chat_Messages.sln /p:Configuration=Release /p:Platform=x64
```

The DLL is written to `build\Release\GW2_Chat_Messages.dll`. Copy it into `<GW2>\addons\`.

For editors that use clangd, each build also writes `build\compile_commands.json` (or run
`tools\compile_commands.ps1` yourself). It's never committed.

## Versioning

Versions look like `2026.11.21.1412.Dev`: the year, month, day and time (24-hour, `HHMM`) the
version was set, then the kind of build (`Dev` until the first full release, then `Beta` or
`Rel`). **The date and the time are both UTC**, not local time: `1412` is 14:12 UTC (8:12 in US
Central time), and an evening release in the Americas can carry the next day's date. The version
is set once per release, so test builds made in between keep the same number. It's shown in the
Nexus log and on the DLL's Properties > Details tab.

Each build also carries a **build stamp**: the git commit it was built from (`+changes` if it had
uncommitted edits) and the build time, also UTC, for example
`build db84a1e, 2026-11-21 19:20 UTC`. It's in the Nexus log line, under the version on the
addon's options page, in copied error reports and in the DLL's Properties (Comments), so two
builds with the same version can be told apart.

## Contributing

Translations, bug reports, ideas and code are all welcome. [CONTRIBUTING.md](CONTRIBUTING.md)
explains how, including how to translate the addon into your language with the files in
[localization/](localization/). The German, French and Spanish translations were made by AI, so
corrections from native speakers would help a lot.

## AI Disclosure

This addon was built with the help of AI. Most of the code, design notes and documentation were
written by Claude, an AI model made by Anthropic, working with me in Claude Code. I set the
direction, decide what goes in, and review every change before it's merged.

I know some players aren't comfortable with AI being used to make software, and I respect that.
If that's how you feel, no hard feelings. There are other great chat macro tools out there, and
I'd rather you use one you're happy with.

I'm just a player who enjoys Guild Wars 2. I made this to send the messages I want while I play,
and I'm sharing it in case it's useful to you too.
