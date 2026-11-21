# GW2 Chat Messages — design

A Nexus addon that sends pre-written chat messages. It is modelled on the Blish HUD module
[bhm-chat-macros](https://github.com/agaertner/bhm-chat-macros) (v0.15.16, the version on its
`main` branch). The main difference is that macros are kept in a folder tree that the user builds.

## Decisions (2026-11-20)

| Topic | Decision |
|---|---|
| Tree depth | At most **6 levels of folders** (root → folder 1 → … → folder 6). Macros can sit at the root or in any folder. |
| Features | Every chat feature from bhm-chat-macros, including raw edit mode and `.txt` import/export. **No voice commands.** |
| Multi-line | A macro is an ordered list of lines. Each line has its own channel and its own message. One trigger sends the lines in order. |
| Hotkeys | Each macro can have a hotkey. CTRL, ALT and SHIFT modifiers are allowed, and so are mouse buttons. Built into the addon, set in the macro editor (not Nexus keybinds). |
| Import/export | Folder structure is carried along. An import **always shows a preview** (folders created, where each macro goes, conflicts) and changes nothing until the user confirms. |
| Game usability | Top priority: the addon must never break normal game input. See "Input safety". |

## Data model

```
Node   = Folder | Macro            (each has a stable id: 64-bit random, hex in JSON)
Folder = { id, name, children: [Node], expanded }
Macro  = { id, title, lines: [Line], gameModes: PvE|WvW|PvP, mapIds: [int], linkFile? }
Line   = { channel, message, whisperTo?, squadBroadcast? }
channel ∈ Current, Emote, Say, Map, Party, Squad, Team, Reply, Whisper,
          Guild, Guild1..Guild6
```

- Folders are sorted the way the user arranges them, using drag and drop (with an option to
  sort A–Z).
- The 6-folder limit is checked on create, move and import. If moving a folder would push any of
  its descendants past level 6, the move is refused and the UI explains why.
- Storage is `<GW2>\addons\GW2_Chat_Messages\macros.json`, written via a temp file and then renamed.
  A `macros.json.bak` of the previous version is kept.
- Messages are limited to 199 characters each, after placeholders are filled in. This is the same
  limit bhm uses. The editor shows a live character count.

## Features carried over from bhm-chat-macros

- Per-line channel dropdown plus message box (the "fancy" editor). It has add, remove and
  reorder buttons.
- **Whisper** lines have a recipient field. **Squad** lines have a "broadcast" checkbox, which
  only takes effect when the player is the commander (`Mumble::Identity.IsCommander`).
- **Raw edit mode**: one line per row, written as you would type it in game (`/s hi`,
  `/w [Name] hi`, `/d ! broadcast`). Switching between the two editors converts losslessly.
  English commands are accepted, and so are the short and long forms in the German, French and
  Spanish clients (taken from bhm's localized resources).
- **Filters**: game mode (PvE/WvW/PvP) and an optional list of map ids. A macro is *active* when
  it matches the current map. Inactive macros never fire from a hotkey.
  *(Built.)* Rules, in `MacroTree` (unit tested in `tests/FilterTests.cpp`):
  - The game mode comes from Mumble's map type. PvP: PvP, GvG and tournament maps. WvW: the WvW
    maps, Edge of the Mists and Armistice Bastion. PvE: instance, public, tutorial, public-mini and
    big-battle maps. Redirects and character creation are "no mode", where only macros active
    everywhere match.
  - A macro needs at least one game mode (the editor refuses to untick the last one). An empty
    map list means any map; otherwise the current map id must be listed (at most 50 ids).
  - A hotkey only counts for active macros, so two macros can share a hotkey if their filters
    never overlap (for example one WvW and one PvE). The editor warns only about overlapping ones.
  - The quick menu lists only active macros, and hides folders without any. The library tree
    shows every macro, with inactive ones dimmed. **Send now** in the library always works.
  - Map names (GW2 API) come with phase 6; for now maps are entered by id or with "Add this map".
- **Quick menu**: the Quick Access right-click menu and a hotkey-opened pop-up show the tree as
  nested menus, with only the active macros listed. *(Built: folders without macros are hidden;
  macros are coloured by channel, show their hotkey, and preview their lines on hover. The pop-up
  is opened by a Nexus InputBind, "Open Chat Messages quick menu", registered with `"(null)"`. Menu
  sends report failures as alerts. Until filters exist, every macro is listed.)*
- **Placeholders** (decided 2026-11-20: **no `{wp}`, `{poi}` or `{area}`**; GW2 API map data is
  only used as a cache, for map names):
  - Built (`src/Placeholders.*`, unit tested in `tests/PlaceholderTests.cpp`): `{time}` (21:05),
    `{hour}`, `{min}`, `{today}` ("Friday, 20.11.2026", weekday in the Windows language),
    `{random}` (0-100), `{random max}` (0-max), `{random min max}` (inclusive, swapped if
    reversed), `{txt file}` (a random non-blank line), `{txt file line}` (1-based), and `{nexus}`
    (bhm's `{blish}`: "Chat Messages 2026.11.21.1412.Dev").
  - Also built: `{map}` (the current map's name) and `{json path url}` (one value from a JSON
    page: dot-separated path, numbers index arrays, like bhm; objects, lists and null are refused;
    http/https only, at most 512 KB, 3 s timeouts).
  - Map names: `src/MapNames.*` asks `https://api.guildwars2.com/v2/maps?ids=...&lang=en` (no API
    key) and caches the names in `<GW2>\addons\GW2_Chat_Messages\maps.json`. The current map is
    looked up in the background whenever it changes; ids the API doesn't know aren't asked again
    for 5 minutes. A bad cache file is just rebuilt. The filter editor shows names instead of ids.
    English only until the multi-language UI (docs/todo.md).
  - Syntax: `{name arg arg}`, case-insensitive, `"quotes"` for arguments with spaces.
  - Differences from bhm, on purpose: an unknown `{word}` is sent as typed (bhm drops the whole
    line), and a known placeholder that fails stops the whole macro with a message (bhm silently
    drops that line).
  - `{txt}` files must be inside `<GW2>\addons\GW2_Chat_Messages\` (relative path, no `..`, no
    drive), at most 1 MB, UTF-8. This keeps an imported macro from typing out other files.
  - Filled in on the sender's worker thread (`{map}` and `{json}` may wait on the network), before
    the plan is built, so the 199-unit check uses the real text and nothing is typed if a
    placeholder fails. The status shows "Preparing..." meanwhile. Text from a file isn't
    expanded again. The editor shows "~" on lengths of lines with placeholders.
  - Waypoint and other chat links (`[&BDAEAAA=]`) need no feature: they're typed as text and the
    game turns them into links.
- **`.txt` link file**: optional two-way sync of one macro with a text file in raw format.
  *(Built in phase 7 part 2; `src/LinkSync.*`, unit tested in `tests/LinkSyncTests.cpp`.)*
  - Set in the macro editor ("Text file link"). A path without a drive is relative to
    `<addon>\links\`; it must end in `.txt`. Two macros can't link the same file.
  - Linking: a file that already has lines replaces the macro's lines; a missing or empty file is
    created from the macro.
  - The render thread checks linked files once a second (a stat each; a file is only read when
    its last-write time changed). A file changed since the last sync wins over the macro (it's
    the newer edit); otherwise a macro edited in the addon is written to the file (CRLF, atomic
    temp + rename). Formatting-only differences never cause a write. At load, the file wins.
  - A deleted file is never recreated behind the user's back: the editor says so and offers
    "Recreate the file". A file that can't be read is never overwritten. Over 50 lines: the first
    50 are used and the file's extra lines are kept until the macro is edited.
  - The sync state is in memory only. Unload does a last sync.

## Text size

*(Built 2026-11-21; `src/TextSize.*` (rules, unit tested in `tests/TextSizeTests.cpp`) and
`src/UiFont.*` (ImGui side).)*

- A **Text size** dropdown in the library toolbar and on the addon's Nexus options page lists the
  fonts Nexus gives addons: the default ImGui font as Nexus set it up, and NexusLink's `Font`,
  `FontUI` and `FontBig`. Each entry shows its size in pixels, smallest first; one font offered
  under two names is listed once. The choice is saved as `text_size` in `settings.json`.
- The chosen font is pushed around our own windows, popups, menus and tooltips (the library, the
  Import window, the quick menu pop-up and our part of the Quick Access menu). Nexus' options page
  keeps Nexus' font. Fonts are read from NexusLink every time and never kept; while Nexus rebuilds
  its fonts (null pointers) the default font is drawn and the last scale is kept.
- **Scale** = chosen size / default size. Fixed widths and window sizes were laid out for the
  default font and are multiplied by it (`UiFont::Px`); widths next to text (the length label,
  the Link/Load buttons) are measured from the text instead.
- **Window sizes follow:** when the scale changes, an open window's size is multiplied by the
  new scale over the old one, from its top-left corner, kept within the screen and moved back on
  screen if needed. Minimum sizes scale too. A window that was closed is resized when it's next
  shown.
- **Nothing runs off a window:** text wraps at the window's (or pane's) edge; tooltips wrap at
  about 35 characters' width; rows of buttons continue on the next line when the next button
  doesn't fit (`UiFont::Row`); the tree, the details pane, the Import window and its lists scroll
  sideways for names and rows that can't wrap.
- Later (see `docs/todo.md`): more sizes than Nexus offers, from a font of our own.

## Import / export

*(Built in phase 7 part 1; `src/Exchange.*`, unit tested in `tests/ExchangeTests.cpp`.
Decisions 2026-11-20: no Windows file dialog (a modal dialog over the game is risky); exports
go to `<addon>\exports\` (never overwritten: `Name (2).json`), and the Import window lists
`<addon>\imports\`, takes a typed path, or pastes from the clipboard. Undo is an in-memory
copy of the library from just before the import, kept until dismissed, the next import, or
unload. Hotkeys are kept unless "Keep hotkeys" is unticked. Link files are never exported or
imported. A Replace keeps the existing item's id and place.)*

- **Export**: the whole tree, one folder, or one macro. The format is
  `ChatMessages.export.json` (`{ "format": 1, "root": Node }`). A single macro can also be
  exported as raw `.txt`.
- **Import sources**: our JSON format, a raw `.txt` (becomes one macro), or a folder of `.txt`
  files (sub-folders become folders).
- **Preview step** (always shown):
  - A tree showing the result, with the new folders, new macros and merged folders marked.
  - The user picks the target folder.
  - For name conflicts the user can choose Rename (`Name (2)`), Skip or Replace. This can be set
    per item or for all items.
  - Depth violations are flagged. Those items are blocked, or flattened to the deepest allowed
    level if the user chooses that.
  - Nothing is written until the user clicks **Import**. Import can be undone once (the `.bak`
    file).

## Input safety (highest priority)

bhm-chat-macros sometimes leaves Tab (Target Nearest Enemy) not working. These are the likely
causes in its code (`Blish_HUD.Extended.ChatUtil` / `KeyboardUtil`):

1. It sends keys with **`SendInput`**, which goes into the *system-wide* input stream. That
   changes the OS keyboard state, and the user's real key presses can mix with the injected ones.
2. It presses **LCtrl** for Ctrl+V and Ctrl+A, then releases it after a `Thread.Sleep`. If a step
   fails, or a real key press happens in between, the game can believe a modifier is still held.
3. For whispers it presses **Tab** inside the chat box. If the game keeps focus in the whisper
   box (the code has a workaround for exactly this), later Tab presses go to the chat box instead
   of targeting.
4. Before opening chat it force-releases the user's physical Shift keys.

Rules this addon follows:

- **Never use `SendInput` / `keybd_event`.** Keys and text go only to the game window, through
  `WndProc_SendToGameOnly` (or `PostMessage` to the game HWND). The OS keyboard state is never
  touched.
- **Type text with `WM_CHAR`**, not the clipboard. This leaves the user's clipboard alone and
  needs no Ctrl presses. If testing shows the game ignores `WM_CHAR`, the fallback is
  clipboard + paste with the clipboard restored afterwards. Any modifier we press is released in
  a scope guard, whatever happens.
- **Open chat through Nexus game binds** (`GameBinds_Press/Release` with `GB_UiChatFocus` /
  `GB_UiSquadBroadcastChatFocus`), using the user's real game keys. If a bind is not set, show an
  alert telling the user to set it in Nexus. Never guess a key.
- **Never touch the user's physical modifiers.** If any modifier is physically held when a macro
  fires (for example the macro's own CTRL+… hotkey), wait up to 500 ms for it to be released.
  If it is still held, abort.
- **Don't fire** when a text box is already focused (`Mumble IsTextboxFocused`, meaning the user
  is typing), when the game is not focused, or when not in gameplay (`NexusLink.IsGameplay`).
- **Every step is verified.** After opening chat, wait (bounded) for `IsTextboxFocused`. After
  each Enter, wait for it to clear. If anything fails, close chat with a single Escape (only if
  the box is still focused, and only if we opened it: a chat box the user opened is never
  touched) and abort the rest of the macro.
- **Avoid Tab where possible.** Whisper handling will be tested in game first, to look for a way
  that needs no Tab. If Tab is needed, it is only sent while the whisper box is verified as
  focused, and focus is verified as released afterwards.
- **One macro at a time** runs on a dedicated sender thread. Triggers that arrive while a macro
  is sending are ignored. A 50 ms gap is left between lines (bhm uses 20 ms; to be tuned in game).
  Unload waits for the thread to finish.
- **Keys reach the game on its own window thread.** The sender thread posts a private message
  (`RegisterWindowMessageW`) to the game window. Our WndProc hook runs the queued step there,
  calls `WndProc_SendToGameOnly`, and consumes the message; every other message passes through
  untouched. Key presses are sent as `WM_KEYDOWN`, `WM_CHAR`, `WM_KEYUP`. At most one Escape is
  sent per macro, because Mumble lags a frame and a second Escape would open the game menu.
- The step sequence is built by `src/SendPlan.cpp` (no Nexus code, unit tested) and executed by
  `src/Sender.cpp`. In-game checks: `docs/in-game-tests.md`.
- **Hotkeys are built in (decided 2026-11-20), not Nexus InputBinds.** Each macro's hotkey is
  set in the macro editor ("Set...", then press the key) and saved in `macros.json` as text
  (`"hotkey": "Ctrl+Shift+F1"`), so it travels with import/export. A macro has no hotkey until the
  user sets one. Rules (`src/Hotkey.*`, unit tested; `src/Hotkeys.*` reads the presses):
  - Presses are read in a WndProc hook and **never blocked or changed**: the key still reaches the
    game. The user picks keys they don't use in game; plain letters and numbers get a warning.
  - Keyboard keys plus Mouse 3, 4 and 5, with Ctrl, Alt and Shift. Refused: left/right mouse,
    Escape, modifier keys alone, and plain Tab, Enter, Space and Backspace.
  - Ignored: auto-repeat, a focused game text box (Mumble), ImGui text input in any addon
    (`WantTextInput`), and outside gameplay.
  - The window thread only queues the press. The render thread matches it against the tree and
    starts the sender. A hotkey set on more than one macro sends nothing and shows an alert.
  - A hotkey send that fails shows a Nexus alert, since the library window may be closed.
  - Only the addon's own window keybinds (show/hide the library, open the quick menu) stay Nexus
    InputBinds (`"(null)"`).

## Policy note

Raidcore's guide quotes ArenaNet's macro policy ("one user action may lead to one game action"),
and Raidcore has a "Macro Usage" label. Sending several lines from one key press is how bhm
works and is what the user wants. Before the first public release, check ArenaNet's current
policy and mention the behaviour when registering with Raidcore.

## Versioning

*(Decided 2026-11-21.)*

- Format: **`YYYY.MM.DD.HHMM.Channel`**, the date and time (GMT, 24-hour) the version was set,
  then `Dev`, `Beta` or `Rel` (normal release). Example: `2026.11.21.1412.Dev`. Every release is
  `Dev` until initial development is finished.
- `tools\set_version.ps1 [-Channel Dev|Beta|Rel]` sets it to now and writes `src/Version.h`
  (plain `#define`s), used by `Globals.h` and by the `.rc` file. Run it once per release and
  commit the header.
- Nexus gets the four numbers (`AddonVersion_t`: 2026, 11, 21, 1412; each fits 16 bits). The
  text form is shown in the Nexus log, the library window, `{nexus}` and error reports.
- The DLL's version resource (Properties > Details): File version and Product version are the
  text form, the fixed file version is the four numbers.
- **GitHub release tags** are the numbers only (`2026.11.21.1412`): Nexus' updater ignores tags
  with a suffix, and the tag must match the DLL's version or Nexus keeps re-downloading it. `Dev`
  and `Beta` builds are published as GitHub pre-releases (Nexus offers those only to users who
  allow pre-release updates for the addon); `Rel` builds as normal releases.

## Implementation phases (one small PR each)

1. Scaffolding: submodules, VS project, CI, an addon that loads and shows an empty window. *(done)*
2. Data model, JSON storage, tree editor (create, rename, delete, drag-drop, depth limit). *(done)*
3. Macro editor: fancy and raw modes, parser and serializer, character count. *(done)*
4. Sender: the safe input pipeline (focus, `WM_CHAR`, verification), test button. *(done; not yet
   tested in game)*
5. Per-macro hotkeys, Quick Access and pop-up nested menus, game-mode and map filters. *(done;
   not yet tested in game)*
6. Placeholders, with a cached GW2 API map-name lookup. *(done; not yet tested in game)*
7. Import/export with preview, `.txt` import/export and link-file sync. *(done; not yet tested
   in game)*
8. Text size setting for the addon's windows (Nexus' fonts). *(done; not yet tested in game)*
