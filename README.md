# GW2 Chat Messages

Chat macros for Guild Wars 2, as a [Nexus](https://raidcore.gg/Nexus) addon.

> **Status:** early development. This version only adds the addon's window and Quick Access icon.
> The macro library is being built. See [docs/design.md](docs/design.md) for the plan.

## Planned features

- Keep your chat macros in folders, nested up to 6 levels deep (for example
  expansion › map › event), or at the top level.
- Each macro sends one or more lines, and each line goes to its own channel: say, map, party,
  squad (including squad broadcast when you're commander), team, guild 1–6, whisper, emote, reply
  or whichever channel is open.
- An optional hotkey for each macro. Modifiers (CTRL, ALT, SHIFT) and mouse buttons work.
- Placeholders such as the nearest waypoint, the map name or the time.
- Raw text editing, `.txt` import and export, and import/export of whole folder trees, with a
  preview before anything is imported.
- Built so it never interferes with normal game controls.

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
