# GW2 Chat Messages

Chat macros for Guild Wars 2, as a [Nexus](https://raidcore.gg/Nexus) addon.

> **Status:** early development. All the features below are built, but **none of them has been
> tested in game yet**, and there is no release to download. Import and export are still to come.
> See [docs/design.md](docs/design.md) for the plan.

## What it does

GW2 Chat Messages keeps your chat macros in a library you organise yourself, and types them into
chat for you, from a hotkey, a menu or a button.

- **Folders:** keep macros in folders nested up to 6 levels deep (for example
  expansion › map › event), or at the top level. Drag and drop to rearrange, or sort A–Z.
- **Multi-line macros:** each macro sends one or more lines, and each line goes to its own
  channel: say, map, party, squad (with squad broadcast when you're commander), team, guild 1–6,
  whisper, emote, reply, or whichever channel is open. Edit them line by line, or as raw text
  typed the way you would in game (`/s hi`, `/w [Name] hi`). German, French and Spanish chat
  commands work too.
- **Three ways to send:**
  - **Hotkeys:** give any macro a key, with Ctrl, Alt or Shift if you like. Mouse 3, 4 and 5
    work too.
  - **Quick menu:** right-click the scroll icon in the Nexus bar, or press a key you choose, to
    pick a macro from nested menus.
  - **Send now** in the library.
- **Filters:** limit a macro to PvE, WvW or PvP, and to certain maps. Hotkeys and the quick menu
  only use macros that are active where you are, so one key can do different things in WvW and
  in PvE. Map names come from the official GW2 API.
- **Placeholders:** `{time}`, `{today}`, `{random 1 6}`, `{map}`, a random line from a text file
  (`{txt jokes.txt}`), a value from a JSON web page, and more, filled in when the message is sent.
- **Safe input:** it never interferes with normal game controls. Keys go only to the game window,
  chat opens with your own Chat key, your Tab, Shift and Ctrl are never pressed or released for
  you, and a macro won't fire while you're typing.
- **Coming next:** import and export of whole folder trees with a preview, `.txt` import and
  export, and syncing a macro with a text file.

## Quick start

1. Install [Nexus](https://raidcore.gg/Nexus), then copy `GW2_Chat_Messages.dll` into
   `<GW2>\addons\`.
2. In Nexus > Options > Game Keybinds, make sure your **Chat** key is set (and **Squad
   Broadcast** if you're a commander). The addon uses them to open chat.
3. Left-click the scroll icon in the Nexus bar to open the macro library. Click **New macro**,
   add a line, pick a channel and type a message.
4. Press **Send now**, set a hotkey in the macro editor, or right-click the scroll icon for the
   quick menu.

Your macros are saved in `<GW2>\addons\Chat Messages\macros.json`.

## Documentation

Full usage documentation is being written for the project wiki. Until the wiki is online, the
pages are in [docs/wiki/](docs/wiki/Home.md).

## Building from source

```
git clone --recurse-submodules https://github.com/Marthisdil/GW2_Chat_Messages.git
msbuild GW2_Chat_Messages.sln /p:Configuration=Release /p:Platform=x64
```

The DLL is written to `build\Release\GW2_Chat_Messages.dll`. Copy it into `<GW2>\addons\`.

## AI Disclosure

This addon was built with the help of AI. Most of the code, design notes and documentation were
written by Claude, an AI model made by Anthropic, working with me in Claude Code. I set the
direction, decide what goes in, and review every change before it's merged.

I know some players aren't comfortable with AI being used to make software, and I respect that.
If that's how you feel, no hard feelings. There are other great chat macro tools out there, and
I'd rather you use one you're happy with.

I'm just a player who enjoys Guild Wars 2. I made this to send the messages I want while I play,
and I'm sharing it in case it's useful to you too.
