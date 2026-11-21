# Installation

## What you need

- Guild Wars 2 (64-bit, as it is today).
- [Nexus](https://raidcore.gg/Nexus), the addon loader by Raidcore.

## Install

1. Install Nexus and start the game once, so it creates `<GW2>\addons\`.
2. Copy `GW2_Chat_Messages.dll` into `<GW2>\addons\`.
3. Start the game. Nexus loads the addon, and a scroll icon appears in the Nexus bar
   (Quick Access).

<!-- screenshot: nexus-quick-access.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the scroll icon in the Nexus bar.*

There is no public release yet. Until there is, build the DLL from source as described in the
README.

## Set your chat keys in Nexus

The addon opens chat with your own game keys, through Nexus. It never guesses a key. Check these
in **Nexus > Options > Game Keybinds**, and make them match your game settings:

- **Chat** (needed for every macro).
- **Squad Broadcast** chat (only needed for squad broadcast lines, as commander).

<!-- screenshot: nexus-game-keybinds.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Chat key in Nexus > Options > Game Keybinds.*

If a key isn't set, the macro isn't sent and you get a message telling you which key is missing.

## Optional Nexus keybinds

In **Nexus > Options > Keybinds** you can set keys for:

- **Show/hide Chat Messages library**
- **Open Chat Messages quick menu**

Neither has a key by default. Your macro hotkeys are set in the addon itself (see
[Sending macros](Sending-Macros)), not here.

## Updating and removing

- To update, replace the DLL. Your macros are kept, because they're stored in a separate folder
  (see [Files and data](Files-and-Data)).
- To remove the addon, delete the DLL. Delete `<GW2>\addons\Chat Messages\` too if you don't want
  to keep your macros.
