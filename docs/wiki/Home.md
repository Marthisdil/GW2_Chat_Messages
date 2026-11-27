# GW2 Chat Messages

GW2 Chat Messages is a [Nexus](https://raidcore.gg/Nexus) addon for Guild Wars 2. It keeps your
chat macros in folders and types them into chat for you.

> **Early development:** everything described here is built, but nothing has been tested in game
> yet.

![The macro library window](../images/library-window.png)

## Pages

- [Installation](Installation): install the addon and set the keys it needs.
- [The macro library](The-Macro-Library): folders, macros, drag and drop.
- [Editing macros](Editing-Macros): lines, channels, whispers, squad broadcasts, raw text.
- [Sending macros](Sending-Macros): Send now, hotkeys and the quick menu.
- [Filters](Filters): limit a macro, or a whole folder, to game modes and maps.
- [Placeholders](Placeholders): `{time}`, `{random}`, `{map}` and more.
- [Import and export](Import-and-Export): share and back up macros, with a preview and undo.
- [Languages](Languages): show the addon in English, German, French, Spanish, or a language
  file of your own.
- [Files and data](Files-and-Data): where your macros are saved.
- [Backups and restore](Backups-and-Restore): backups on a schedule you choose, and putting
  one back.
- [Advanced options](Advanced-Options): the features kept behind one checkbox (text file links,
  cooldowns, Raw text and more), and why.
- [Safety and troubleshooting](Safety-and-Troubleshooting): how sending stays safe, and what the
  error messages mean.
- [ArenaNet's policies](ArenaNet-Policies): the macro policy, what the addon is for (and not
  for), and the disclaimer.

## In short

1. Open the library by left-clicking the chat bubble icon in the Nexus bar. The first time, it
   opens by itself with a [welcome](The-Macro-Library#the-welcome-and-the-tour): add the
   [example macros](The-Macro-Library#the-examples-folder) to look at and try, and take the
   short tour.
2. Make a macro: give it one or more lines, each with its own channel and message.
3. Send it with **Send now**, a hotkey of your own, or the quick menu (right-click the chat
   bubble icon).

## Credits

The addon is modelled on [Chat Shorts](https://github.com/agaertner/bhm-chat-macros), a Blish
HUD module by Nekres (agaertner on GitHub): its feature set, and a few facts from it (the chat
commands in each game language, the channel colours, the name length limit). No code was
copied; this addon was written from scratch in C++ for Nexus.
