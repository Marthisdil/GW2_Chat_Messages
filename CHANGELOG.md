# Changelog

All notable user-visible changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- **The wiki is online:** the full user documentation is at
  https://github.com/Marthisdil/GW2_Chat_Messages/wiki (the README links to it).
- **Map search in the filters:** type part of a map's name (or its id) under **Maps** to find it
  in the GW2 API's full map list, shown with its region, and click it to add it. **Maps you've
  been to** lists the maps you visited lately. A map id the API no longer lists is marked in
  yellow with **(!)**, because the map may have been removed or replaced in a game update.
- **Map list on the options page:** how many maps are known and from when, and a **Refresh map
  list** button. The list is fetched on the first run, once a week, and when you change the
  language.
- **Hotkeys that are also GW2 default keys:** the hotkey editor says when a key is one of the
  game's default keybinds (for example F1 is "Profession skill 1", Ctrl+T is "Call target"),
  since pressing it does both. Your own keybinds may differ.

### Changed
- **Everything is translated now:** error messages from the library, imports, sending, link files,
  placeholders and map lookups, and the key names shown for hotkeys ("Maus 4", "Bild auf"), follow
  the addon's language. `macros.json` keeps the English key names, so files work in any language.
- **Map names follow the language** where the GW2 API has them (German, French, Spanish), in the
  filters and in `{map}`.
- The left and right mouse buttons stay refused as hotkeys, with or without Ctrl, Alt or Shift:
  the message now says why, and so do the README and the wiki.

### Removed
- **The `{txt}` and `{json}` placeholders** (a line from a text file, a value from a web page).
  A shared or imported macro could use them to type out a file from your PC or to make it fetch
  an address. A line that still has one isn't sent: the editor shows a red note under it, the
  error window says which placeholder it is, and the Import window marks such macros
  **REMOVED PLACEHOLDER**. Edit the line to remove it.

### Added
- **Import preview marks:** macros that still use the removed `{txt}` or `{json}` are marked
  **REMOVED PLACEHOLDER**. With **Keep hotkeys** ticked, a hotkey with no Ctrl, Alt or Shift, or
  one you already use, is marked too. A line above the preview counts each mark.
  Replace never brings such a hotkey along, and when a replaced macro loses or changes its
  hotkey, the preview marks it and the line after the import says so.
- **Languages:** the addon's text can be shown in English, German, French or Spanish. By default
  it follows Nexus' language; a **Language** dropdown on the options page picks one instead.
  The keybind names and the icon's tooltip follow Nexus' language too. More languages can be
  added as `.json` files in `<GW2>\addons\GW2_Chat_Messages\localization\` (**Open localization
  folder**, **Reload language files**); every file is checked before use, and texts that fail a
  check are shown in English. The German, French and Spanish texts were made by AI and need a
  native speaker's review. `localization/template.json` and `CONTRIBUTING.md` explain how to
  translate.
- **Only active here:** a checkbox under the library's search box shows only the macros that are
  active where you are, with a label that names it (e.g. "Only active in WvW on Eternal
  Battlegrounds (38)"). It follows map changes and is remembered.
- **Quick menu search:** a search box at the top of the quick menu lists the matching macros that
  are active here, with their folders; Enter sends the first. In the pop-up opened by its key you
  can type straight away.
- **Search box:** a **Search (Ctrl+F)** box above the library tree finds folders and macros by
  name, by the text of their lines, a whisper's recipient, or their hotkey (`ctrl+f1`), ignoring
  case and accents. The tree shows only the matches (with their folders opened and the match
  highlighted) until you clear it with **x** or Escape; then it's back as it was. Dragging and
  Move up/down are off while searching.
- **Import a whole library:** in the Import window, choose **A whole library** to import a copy
  of someone else's `macros.json` (or `macros.json.bak`). It goes into one new folder at the top
  level, named after the file (e.g. `From Alex`) and renamable, with their hotkeys off by default.
- **Build stamp:** every build shows the commit it was built from and its build time (UTC), e.g.
  `build db84a1e, 2026-11-21 19:20 UTC`, in the Nexus log, under the version on the options page,
  in copied error reports and in the DLL's Properties > Details (Comments).
- **Import, choose what to import:** every item in the Import preview has a checkbox. A folder's
  checkbox takes everything inside it, a macro's just that macro; **Select all** and **Deselect
  all** sit above the preview, and **Import selected** imports only what's ticked. Taken names are
  marked **NAME TAKEN**, next to what will happen and the choice box.
- **Trash bin counter:** the bin shows `Trash bin (folders/macros)`, e.g. `(1/4)`, counting
  everything inside deleted folders.
- **The trash bin isn't exported or imported:** exports never include it (the export message and
  menus say so while the bin holds anything), and importing a whole `macros.json` leaves its bin
  out, with a warning.
- **Trash bin:** deleted folders and macros go to a **Trash bin** at the bottom of the library
  instead of being gone for good. Right-click an item there to restore it (back where it was;
  if its folder is gone, you choose to recreate the folders or put it at the top level) or to
  delete it permanently; right-click the bin to empty it. Items in the bin can't be sent or used
  by a hotkey. Turn it off with "Use a trash bin for deleted folders and macros" on the addon's
  Nexus options page (on by default).
- **Character counter under the message box:** each line shows `23/199 characters` below its
  message, turning yellow from 150 and bright red from 180.
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
- **Map names in filters:** the map list in "Where it's active" shows names like
  "Queensdale (15)" instead of bare ids.
  - Names come from the official GW2 API (no API key) and are saved in
    `<GW2>\addons\GW2_Chat_Messages\maps.json`, so each map is only looked up once.
- While placeholders are being filled in, the status says "Preparing...".
- **Placeholders:** type these in a message and they're filled in when it's sent:
  - `{time}` (like 21:05), `{hour}`, `{min}` and `{today}` (like Friday, 20.11.2026).
  - `{random}` (0 to 100), `{random max}` and `{random min max}`.
  - `{nexus}` is this addon's name and version.
  - Anything else in `{ }` is sent exactly as typed.
  - If a placeholder can't be filled in (for example a wrong number in `{random}`), nothing is
    sent and you're told why.
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
- **Mouse hotkeys need Ctrl, Alt or Shift:** Mouse 3, 4 and 5 can only be hotkeys together with
  a modifier (e.g. Ctrl+Mouse 4), so a plain dodge or skill button can't send a macro by accident.
  A plain mouse hotkey you set earlier is kept but doesn't fire; the tree and the editor mark it
  so you can change it.
- **Imported macros come without their hotkeys:** **Keep hotkeys** in the Import window now starts
  unticked for every import (it was ticked for macros), so a shared file can't bind its macros to
  keys you press. Tick it for your own exports; the preview then shows each macro's hotkey.
- The data folder is now `<GW2>\addons\GW2_Chat_Messages\`, named like the DLL (it was
  `Chat Messages`). If you ran an earlier build, move that folder's files across.
- **Version numbers** are now the date and time the version was set (both UTC) plus the kind of build, like
  `2026.11.21.1412.Dev` (`Dev` until the first full release, then `Beta` or `Rel`). The DLL's
  Properties > Details tab shows it as File version and Product version.

### Fixed
- A macro line with a huge number of `{` (from an imported file) no longer freezes the game while
  the Import window shows it or the editor has it open, and sending such a line no longer takes
  minutes.
- Hardening against crafted import files and odd file names:
  - A file or folder name Windows allows but UTF-8 can't hold, or such a character typed into a
    path or file name box, no longer ends the addon's frame with an error; it shows as `�`.
  - A damaged `maps.json` (wrongly typed values) is rebuilt instead of causing an error.
  - The Import window lists at most 200 warnings for one file (a crafted file could have
    millions, all drawn every frame), then counts the rest.
  - A folder import reads at most 16 MB of text. If the game runs out of memory while importing,
    nothing is imported and the error window says so.
  - Text that isn't UTF-8 grows when its characters are replaced. A `.txt` import or link file
    that would be over 256 KB that way is refused, and a macro too big for its link file isn't
    written to it, so a link no longer stops syncing.
  - Names crafted to slow down the check for duplicate names no longer freeze the game.
  - A control character pasted into a message or recipient box (such as U+0085) becomes a space
    right away, as a newline or tab already did.
- A placeholder value (such as `{map}`) could type control characters (Backspace, Escape,
  Tab...) into chat, or put a `/` where chat reads a command and so pick a channel or command the
  macro didn't name, even after one you wrote (`/g {map}` with a value starting `/w Name`).
  Control characters in a value now become spaces, and a macro isn't sent if a line's own text has
  one, or if a placeholder would start a command.
- Restoring from the trash bin, or importing next to an item with the same name, no longer
  freezes the game when that name is 99-100 characters long. The new name is shortened so all
  of " (2)" fits.
- Loading a library or an import with many items that have no id (for example a hand-written
  file) no longer freezes the game for many seconds.
- Importing a file with thousands of macros into one folder (or showing its preview when the
  names are already there) no longer freezes the game.
- An unexpected internal error while sending a macro (for example running out of memory, or a
  bad placeholder value) could close the game. It now fails like any other send: chat is
  closed if the addon opened it, and the error is shown.
- A damaged map-name cache (`maps.json`) entry is now skipped more strictly (only whole map ids
  are read).
- When a send was refused because you were typing in chat, the addon could press Escape and close
  your chat box. It now only ever closes a chat box it opened itself.
- An imported file, a whole library or a `macros.json` whose trash bin held a very deeply nested
  entry could close the game while it was read. It's now read without copying that entry.
- Importing or loading a file with thousands of folders or macros of the same name could freeze
  the game, and a repeated name close to the 100-character limit could freeze it for good.
  Repeated names are now renamed (`Name (2)`, `Name (3)`...) quickly; a name too long for the
  number is cut shorter to make room.
- Importing or pasting a file in which a folder's `expanded` value isn't `true` or `false` (for
  example `"yes"`) could close the game. That folder is now shown open, as when the value is missing.
- Importing a JSON file or pasted text whose `"format"` was `null`, text (`"1"`), a list or an
  object could close the game. It's now refused like any other file that isn't a Chat Messages
  export, and other errors while reading an import's JSON are shown as an import error.
- A `.txt` file not saved as UTF-8 (e.g. ANSI, with `ä` or `é`), imported or linked to a macro,
  stopped your macros from being saved for the rest of the session, and a `.json` export could
  close the game. Its characters that can't be read are now replaced with `�` (the font may show
  `?`), with a warning, and saving and exporting work.
- A tab, a lone carriage return or another control character inside a macro's line (from an
  import, a link file or `macros.json`) was typed into chat as that key. It's now a space.

### Documentation
- The README describes what the addon does, with a quick start.
- Full usage documentation, written for the project wiki, is in `docs/wiki/`.
- `docs/screenshots.md` lists the screenshots the README and wiki still need.
