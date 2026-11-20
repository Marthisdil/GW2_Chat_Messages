# Changelog

All notable user-visible changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- **Macro editor:** select a macro to edit the messages it sends.
  - **Lines mode:** each line has a channel dropdown (Current channel, Say, Map, Party, Squad, Team,
    Reply, Whisper, Emote, Guild, Guild 1-6) and a message box.
    - Whisper lines have a recipient box. Squad lines have a **Broadcast** option, which only works
      while you're the commander.
    - You can add lines (a new line copies the previous line's channel and recipient), remove them,
      and reorder them with the up and down arrows. A macro can have up to 50 lines.
  - **Raw text mode:** type the lines the way you would in game, one per row, for example
    `/s hello`, `/w [Name] hi`, `/d ! Stack on tag`, `/g3 hi`.
    - The German, French and Spanish chat commands are understood too, and so are the long forms
      such as `/say` and `/guild3`.
    - Anything else starting with `/` (like `/wave`) is sent as typed.
    - A "Will send" list under the text shows how each row will be sent.
  - Each line shows a live character count against the game's 199-character limit, turning red
    when a line is too long. Lines with no message, and whispers with no recipient, are flagged.
  - In the tree, each macro is coloured by the channel of its first line.
- **Macro library window:** a folder tree on the left and details of the selected item on the right.
  - Create folders and macros at the top level or inside any folder. Folders can be nested up to
    6 levels deep.
  - Rename, delete (with a confirmation that says how much is inside a folder), move up/down, and
    sort A-Z, from the right-click menu or the buttons.
  - Drag and drop to move items: drop on the middle of a folder to put an item inside it, or just
    above or below an item to place it there. A move that isn't allowed (too deep, into itself, or
    a name that's already used there) shows the reason while you drag.
  - Expand all / collapse all. Open and closed folders are remembered.
  - Names are unique per folder (ignoring case), separately for folders and macros, up to 100
    characters.
- **Saving:** the library is saved to `<GW2>\addons\Chat Messages\macros.json` shortly after each
  change and when the addon unloads. The previous version is kept as `macros.json.bak`. If the file
  can't be read, it's kept under a new name (never overwritten) and the backup is loaded instead.
- Project scaffolding: a Nexus addon that loads, with a Quick Access icon (a scroll) and an
  empty "Chat Messages" window. The window opens from the icon, from the Nexus options page, or
  with a keybind that you can set in Nexus (it has no default key).
- Visual Studio build producing `GW2_Chat_Messages.dll`, with the Nexus API, Mumble API,
  Dear ImGui v1.80 and nlohmann/json as git submodules.
- CI build on GitHub Actions. The Release DLL is available as a workflow artifact.
- Design notes in `docs/design.md`.
