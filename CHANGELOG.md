# Changelog

All notable user-visible changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- **Error window:** every error now opens a small window with the message, what you did to get
  it, an **OK** button and **Copy to clipboard**. The copy includes the addon version, the time,
  what you did, the error and where you were in game, ready for a bug report. Several errors
  queue up ("Error 1 of 3"). Errors from a hotkey or the quick menu still show a Nexus alert too,
  and their window doesn't take focus, so the game keeps working.
- **Text size:** a "Text size" dropdown in the library's toolbar (and on the addon's Nexus options
  page) picks the size of the text in the addon's windows, from the fonts Nexus provides. Windows
  grow or shrink with it, long text wraps at the window's edge, buttons that don't fit move to
  the next line, and the tree and Import preview scroll sideways for long names. Saved in
  `settings.json`.
- **Text file links:** in the macro editor, "Text file link" keeps a macro in step with a `.txt`
  file (one chat line per row), both ways. Edit the file in Notepad and the macro follows within
  a second; edit the macro and the file is rewritten. A name alone goes in
  `<GW2>\addons\GW2_Chat_Messages\links\`. Open file, Open folder, Unlink and Recreate the file
  buttons. A deleted file is never recreated without asking, and a file that can't be read is
  never overwritten.
- **Export:** "Export all" in the toolbar saves your whole library, and right-click > Export saves
  one folder or macro. Save as a `.json` file, a macro as a `.txt` file of raw lines, or copy
  either to the clipboard. Files go to `<GW2>\addons\GW2_Chat_Messages\exports\` and are never
  overwritten.
- **Import:** a new Import window (toolbar "Import...", or right-click a folder > "Import into
  this folder...") reads a `.json` export, a `.txt` file (one macro), a folder of `.txt` files,
  or pasted text.
  - A preview shows what happens to every folder and macro before anything changes.
  - When a name is taken, choose Keep both (rename), Skip, Replace or (folders) Merge, for all
    items or one by one.
  - Folders that would be nested more than 6 deep are left out, or their contents moved up.
  - Choose whether to keep the imported hotkeys.
- **Undo import:** after an import, one click puts the library back as it was.
- **More placeholders:**
  - `{map}` is the name of the map you're on, like "Lion's Arch".
  - `{json path url}` is one value from a JSON web page, for example
    `{json id https://api.guildwars2.com/v2/build}`. The path is separated by dots, and numbers
    pick list entries (`data.0.name`).
- **Map names in filters:** the map list in "Where it's active" shows names like
  "Queensdale (15)" instead of bare ids.
  - Names come from the official GW2 API (no API key) and are saved in
    `<GW2>\addons\GW2_Chat_Messages\maps.json`, so each map is only looked up once.
- While placeholders are being filled in, the status says "Preparing...".
- **Placeholders:** type these in a message and they're filled in when it's sent:
  - `{time}` (like 21:05), `{hour}`, `{min}` and `{today}` (like Friday, 20.11.2026).
  - `{random}` (0 to 100), `{random max}` and `{random min max}`.
  - `{txt file}` sends a random line from a text file, and `{txt file 3}` sends its third line.
    Text files go in `<GW2>\addons\GW2_Chat_Messages\` (or a folder inside it). Use quotes for
    names with spaces: `{txt "my lines.txt"}`.
  - `{nexus}` is this addon's name and version.
  - Anything else in `{ }` is sent exactly as typed.
  - If a placeholder can't be filled in (for example the file is missing), nothing is sent and
    you're told why.
  - Hover **Placeholders (?)** in the macro editor for the list. Lines with placeholders show
    their length as approximate ("~"), and the real length is checked when sending.
- **Filters:** choose where each macro is active, under "Where it's active" in the macro editor.
  - Tick the game modes it's for: PvE, WvW (including Armistice Bastion and Edge of the Mists)
    and PvP.
  - Optionally limit it to certain maps: "Add this map" adds the map you're on, or type a map id.
  - Hotkeys and the quick menu only use macros that are active where you are. **Send now** in the
    library always works.
  - The same hotkey can now be used by macros that are never active together, for example one
    for WvW and one for PvE. The editor only warns when two macros with the same hotkey can both
    be active.
  - In the library, macros that aren't active where you are are shown dimmed.
- **Quick menu:** right-click the scroll icon in the Nexus bar to see your library as menus and
  send any macro with one click.
  - Folders open as sub-menus. Folders with no macros inside are hidden.
  - Macros are coloured by channel and show their hotkey. Hovering one shows the lines it sends.
  - The same menu can pop up at the mouse pointer with its own key: set "Open Chat Messages quick
    menu" in Nexus > Options > Keybinds (no key is set by default). Press it again to close it.
  - If a macro sent from the menu fails, an on-screen alert says why.
  - The menu also has an item to open or hide the macro library.
- **Hotkeys:** give any macro its own hotkey in the macro editor: click **Set...** and press the
  key, with Ctrl, Alt or Shift if you like. Mouse 3 (middle), Mouse 4 and Mouse 5 work too.
  - The hotkey shows next to the macro's name in the tree. **Clear** removes it.
  - Hotkeys never block a key: the key still does its normal job in game, so pick one you don't
    use there. Plain letters and numbers get a warning; Escape, left/right click, and plain Tab,
    Enter, Space and Backspace can't be used.
  - Hotkeys don't fire while you're typing in chat or in any addon text box, outside gameplay, or
    while a macro is already sending.
  - If two macros share a hotkey, the editor warns you and pressing it sends nothing.
  - When a macro sent by hotkey fails, an on-screen alert says why.
  - Hotkeys are saved with your macros in `macros.json`.
- **Send now:** a button in the macro editor (and "Send now" in a macro's right-click menu) types
  each line into its chat channel and sends it, one line after another.
  - Chat is opened with your own Chat key from Nexus > Options > Game Keybinds. If it isn't set
    there, you're told so and nothing is sent.
  - Keys go only to the game window, never through Windows' global keyboard input, so your real
    Tab, Shift, Ctrl and Alt are never pressed or released for you.
  - Nothing is sent while you're typing in a game text box, while the game isn't focused, outside
    gameplay, or while you're holding Shift, Ctrl, Alt or Windows.
  - Each step is checked. If one fails, chat is closed and the rest of the macro is dropped, with
    a message saying why.
  - A macro with a line that's too long, or a whisper with no recipient, isn't sent at all.
  - One macro at a time: a second trigger while sending is ignored.
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
- **Saving:** the library is saved to `<GW2>\addons\GW2_Chat_Messages\macros.json` shortly after each
  change and when the addon unloads. The previous version is kept as `macros.json.bak`. If the file
  can't be read, it's kept under a new name (never overwritten) and the backup is loaded instead.
- Project scaffolding: a Nexus addon that loads, with a Quick Access icon (a scroll) and an
  empty "Chat Messages" window. The window opens from the icon, from the Nexus options page, or
  with a keybind that you can set in Nexus (it has no default key).
- Visual Studio build producing `GW2_Chat_Messages.dll`, with the Nexus API, Mumble API,
  Dear ImGui v1.80 and nlohmann/json as git submodules.
- CI build on GitHub Actions. The Release DLL is available as a workflow artifact.
- Design notes in `docs/design.md`.

### Changed
- The data folder is now `<GW2>\addons\GW2_Chat_Messages\`, named like the DLL (it was
  `Chat Messages`). If you ran an earlier build, move that folder's files across.
- **Version numbers** are now the build date and time (GMT) plus the kind of build, like
  `2026.11.21.1412.Dev` (`Dev` until the first full release, then `Beta` or `Rel`). The DLL's
  Properties > Details tab shows it as File version and Product version.

### Fixed
- When a send was refused because you were typing in chat, the addon could press Escape and close
  your chat box. It now only ever closes a chat box it opened itself.

### Documentation
- The README describes what the addon does, with a quick start.
- Full usage documentation, written for the project wiki, is in `docs/wiki/`.
- `docs/screenshots.md` lists the screenshots the README and wiki still need.
