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
| Hotkeys | Each macro can have a hotkey. CTRL, ALT and SHIFT modifiers are allowed, and so are mouse buttons (Mouse 3/4/5 only with a modifier, since 2026-11-21). Built into the addon, set in the macro editor (not Nexus keybinds). |
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

## Languages

*(Built 2026-11-21 from the todo item; rules and checks in `src/Localization.*`, tested in
`tests/LocalizationTests.cpp`; loading and the Nexus side in `src/Languages.*`; files in
`localization/`, kept in step with the code by `tools/localization.ps1`.)*

- **The English text is the key** (gettext style). UI code writes `T("Open macro library")`;
  `T` looks the literal up in the active language and caches the result by the literal's
  address. Texts in tables are marked `TX("...")` (an identity macro, so the tool finds them)
  and looked up with `Localization::Lookup`. A `##id` / `###id` suffix is split off before the
  lookup and put back after, so ImGui IDs never change with the language; window and pop-up
  titles use `###stableid`. Lookups are render-thread only. Formatted texts use
  `Fmt(T("Sent %s"), ...)`.
- **Files:** one `.json` per language with `Identifier` (`de`, `pt-br`), `DisplayName` (the
  language's own name), `AddonVersion` (what it was translated for), an optional `About`, and
  `Texts` (English → translation). An empty translation shows the English text.
  `localization/en.json` (every text, written from the code) and `template.json` (empty, for
  translators) are generated by `tools/localization.ps1 -Write`, which also adds new texts to
  the other files and drops unused ones; without `-Write` it checks, and `run_tests.ps1` runs
  it.
- **Built in:** `en`, `de`, `fr`, `es` are embedded in the DLL (resource type `LOCALE`), since the
  updater ships only the DLL. The tests require every built-in language to translate every text.
- **Extra languages:** `.json` files in `<addon>\localization\` (only regular files, no links, at
  most 32, read in name order). A file may replace a built-in language with the same id (to test
  a correction), but never English; two folder files with the same id: the second is refused.
  The repository's `localization/` folder is the place to get them; the addon never downloads
  anything. Files from the folder are marked "file from your localization folder" in the list.
- **Choosing:** the Language dropdown (options page) offers "Follow Nexus (language)" (the
  default) and every loaded language, with how much of it is translated. The choice is saved as
  `language` in `settings.json`. Nexus' language is found by giving Nexus a probe text in each of
  its languages and asking for it once a second; Nexus itself follows the game's language only
  with the Unofficial Extras addon. A language without a file shows English. The keybind names
  and the Quick Access tooltip are given to Nexus in every loaded language (`Localization_Set`),
  so Nexus shows them in its language.
- **Checks on every file, built in or not** (a file is untrusted input, see
  `docs/security-review.md`): at most 512 KB (read capped), JSON nesting at most 4 (checked
  before parsing), parsed without exceptions, a UTF-8 BOM allowed. `Identifier` must match
  `^[a-z]{2,3}(-[a-z0-9]{2,8})?$`; `DisplayName` at most 64 bytes, one line. At most 4000 texts of
  at most 2048 bytes; only keys that exist in English are used. Each text must be valid UTF-8
  (no overlong forms, surrogates or values above U+10FFFF) with no control characters except
  `\n`, no DEL or C1 controls, no bidi or invisible marks (U+061C, U+200B, U+200E/F,
  U+202A–202E, U+2066–2069, U+FEFF) and no `##`. Its printf codes must be the English text's,
  in the same order: flags `-+ #0`, width and precision of at most 2 digits, length
  `hh h l ll z`, conversions `s d i u x X f g c`; `%n`, `%p`, `%*` and `%1$` are refused, `%%` is
  ignored. A failing text is skipped (English is shown) and counted; the error window names the
  file and the count, and the Nexus log lists the first 5. A file that fails the file-level
  checks isn't loaded, and the error window says why.
- **Not translated (yet):** messages from the core modules that run off the render thread or
  have no UI dependency (tree, import, sender, link and hotkey errors, key names, "Imported
  library"), and the Nexus menu names quoted in texts. See `docs/todo.md`.
- **Fonts:** Nexus' fonts cover Latin with accents. Other scripts need fonts of their own
  (todo).

## Trash bin

*(Built 2026-11-21 from the todo item; rules in `MacroTree` (`MoveToTrash`, `Restore`,
`DeleteFromTrash`, `EmptyTrash`), tested in `tests/TrashTests.cpp`; UI in `Ui.cpp`.)*

- **Setting:** "Use a trash bin for deleted folders and macros" on the addon's Nexus options page,
  with a description under it. Saved as `trash_bin` in `settings.json`. **On by default.**
- **On:** Delete (toolbar of the details pane, right-click > "Delete (to trash bin)", or dragging
  an item onto the bin) moves the item to the bin with no question, and says so under the
  toolbar. **Off:** Delete asks first and removes it for good, as before. Items already in the
  bin stay, and the bin stays visible, until it's emptied.
- **Where:** a "Trash bin (F/M)" node at the bottom of the library tree (F folders / M macros,
  counting everything inside deleted folders; the tooltip spells it out), closed by default,
  newest first. Each entry shows its name (folders: "(folder, N inside)") and, on hover, when it was
  deleted and from where. Stored in `macros.json` under `"trash"` (left out when empty), each
  with the item, the ids and names of the folders it was in, its place there and the time.
- **In the bin, an item can't be used:** it's out of the tree, so it can't be sent, edited, found
  by a hotkey or the quick menu, or exported. Selecting it shows a read-only view (its lines, or
  what a folder holds) with **Restore** and **Delete permanently...**.
- **Restore** (button, or right-click the item): back into the folder it came from, at its old
  place. If that folder is gone, a pop-up asks: **Recreate the folders** (by name, reusing any
  that still exist, with their old ids where free) or **Put it at the top level**. A name already
  taken there gets " (2)" etc., and the message says so. If restoring would nest folders more
  than 6 deep, it's refused (error window) and stays in the bin; the top level always works.
- **Delete permanently...** and right-click the bin > **Empty trash bin...** ask first ("can't
  be undone").
- No size or age limit: items stay until restored or deleted.
- Undo import puts the bin back as it was before the import too (the whole library is restored).

## Search

*(Decided and built 2026-11-21 from the todo list; matching in `src/Search.*`, tested in
`tests/SearchTests.cpp`; UI in `Ui.cpp`.)*

- A **Search (Ctrl+F)** box above the tree, in the tree pane's width, with an **x** button while
  it holds text.
- **What matches:** a folder by its name; a macro by its name, the text of any line, a whisper
  recipient, or its hotkey's text (`ctrl+f1`). The whole trimmed query is looked for as one piece
  of text. Case and accents are ignored (`Search::Fold`: Latin-1 letters and œ lose their accents,
  ß is `ss`). The trash bin isn't searched.
- **Results: a filtered tree.** Only matches, the folders above them and everything inside a
  matching folder are shown. Folders above a match are held open while searching without changing
  their saved open state, so clearing the search brings the tree back as it was. Matched text in
  a name is highlighted; a macro found by its lines or hotkey gets "(found in its lines)" /
  "(found in its hotkey)". A line above the tree counts folder and macro matches, or says nothing
  matches.
- **While searching:** dragging and dropping, Move up and Move down are off (hidden neighbours);
  the trash bin is hidden; a selected item that's no longer shown is unselected. Editing still
  works and the results update every frame.
- **Keys:** Ctrl+F while the library window is focused puts the cursor in the box; Escape in the
  box clears it. It's a text box, so macro hotkeys are ignored while typing in it.
- **Only active here** (added 2026-11-22): a checkbox under the search box, saved as
  `active_only` in `settings.json` (off by default). Its label names where you are: "Only active
  in WvW on Eternal Battlegrounds (38)" (the mode from `GameModeOfMapType`, the map name from the
  map cache), or "Only active here (no game mode right now ...)" on character select or loading,
  where only macros active everywhere count. When ticked, only macros that `IsActive` here are
  shown, with the folders above them opened; folders without one are hidden. With search text
  too, an active macro shows when it matches or sits in a matching folder (`Search::Where`). It
  follows map changes live; dragging and Move up/down are off as when searching. The line above
  the tree says "N macro(s) active here", "No macros are active here.", or the search counts
  with ", active here".
- **Quick menu search** (added 2026-11-22): a search box at the top of the quick menu (the pop-up
  and the Quick Access right-click menu each have their own, cleared whenever the menu closes).
  With text, the nested menus are replaced by a flat list of the matching macros **active here**
  (the same matching as the library, always with the filter), each as "Name  (A > B)" with its
  folders, coloured, hotkey and preview as usual, at most 30 ("...and N more"). **Enter** sends
  the first one and closes the menu. Only the pop-up opened by its key puts the cursor in the box
  (you can type at once); in the Quick Access menu you click the box first. Hotkeys don't fire
  while typing in it (ImGui text input).

## Character counter

*(Built 2026-11-21 from the todo item; levels in `ChatLine::LengthLevel`, tested in
`tests/ChatLineTests.cpp`.)*

- In the Lines editor the counter sits **under** each message box (the box now uses the full
  width): `23/199 characters`. In Raw text it's next to each row of the "Will send" list.
- It counts what's typed into chat, including the channel command (`/s `), as the game does.
- Colours: grey; **yellow from 150**; **bright red from 180** ("nearly at the limit"); over 199
  stays bright red with "too long, this line won't be sent". With placeholders it starts with "~".

## Error window

*(Built 2026-11-21, from in-game testing; `src/ErrorReport.*` (queue and clipboard text, unit
tested in `tests/ErrorReportTests.cpp`), window in `Ui.cpp`.)*

- Every error the addon shows opens the **error window**: the error in red, "What you did: ..."
  (written at the place the error happens, e.g. `Created a folder named "test" in "Raids"`), an
  **OK** button and **Copy to clipboard**.
- The copied text has the addon name and version, the local time, what was done, the error and
  where in game (mode and map), one per line. Unknown parts are left out.
- Errors queue (oldest first, at most 20; the same action and error twice in a row is kept once).
  OK removes the one shown; "Error 1 of N" shows when more wait.
- It's a normal window, not an ImGui pop-up: an open pop-up takes every mouse click, which would
  block the game after a hotkey error. Errors from our own windows bring it to the front; errors
  from hotkeys, the quick menu and failed saves don't (they also keep their Nexus alert).
- Inside the Name pop-up (a modal, which blocks other windows) the error is a pop-up inside it;
  OK goes back to the name box.
- Not errors, so they stay where they are: good news (exported, sent), warnings next to what they
  are about (line length, whisper recipient, hotkey conflicts, link state, import notes), and the
  "couldn't be saved, retrying" line, which stays until a save works.

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
  - Raw text (`.txt` imports, pasted text and link files) is read as UTF-8. Bytes that aren't
    UTF-8 (an ANSI file, say) become U+FFFD, with a warning (`ChatLine::RepairUtf8`); they're
    not guessed from the ANSI code page. So the library always saves as valid JSON. For a link
    file the repair alone never writes the file back.
- **Preview step** (always shown):
  - A tree showing the result, with the new folders, new macros and merged folders marked.
  - The user picks the target folder.
  - For name conflicts the user can choose Rename (`Name (2)`), Skip or Replace. This can be set
    per item or for all items.
  - Depth violations are flagged. Those items are blocked, or flattened to the deepest allowed
    level if the user chooses that.
  - Nothing is written until the user clicks **Import**. Import can be undone once (the `.bak`
    file).
  - **Choosing items** (added 2026-11-21): every row has a checkbox, all ticked at first.
    Ticking or unticking a folder does the same to everything inside it; ticking a macro also
    ticks the folders above it. A folder whose contents are all unticked is unticked too, and a
    folder with only some contents ticked shows "[some]" and imports just those. **Select all**
    and **Deselect all** above the preview. Unticked items stay listed as "not selected" and
    aren't counted as name conflicts. The button is **Import selected**. Rules in
    `Exchange::SetSelected`/`SelectAll` (`Options::Unselected`), tested.
  - Taken names are marked **NAME TAKEN** (yellow) next to the item, with what will happen and a
    choice box.
- **A whole library** (decided 2026-11-21, from the todo list): someone else's `macros.json`
  (or `.bak`), imported as a copy. Rules in `Exchange::FromLibraryFile`, `LibraryFolderName`
  and `WrapInFolder`, tested.
  - The Import window's step 1 has two choices: **Macros** (as before) and **A whole library**.
    In library mode the imports list shows only `.json` and `.bak` files, the path box reads one
    file, and Paste reads JSON only.
  - Everything at the file's top level is put into **one new folder at the top level** (never
    the Import window's target folder), so it can't mix into the user's folders. Its name is
    editable in step 2: `From <file name>` (`From Alex` for `Alex.json`), or `Imported library`
    for a file still called `macros.json`, pasted text, or an empty name. A taken name gets
    " (2)" (folder conflicts default to Keep both, not Merge; the per-item choice box still
    offers the others).
  - **Keep hotkeys** starts unticked. Their trash bin and link files are left out, as for any
    import. Their folders are one level deeper, so the usual too-deep rules apply.
  - Not taken: their `settings.json` (text size and the trash bin setting are personal).
  - Switching modes, or closing the window, clears what was loaded; the window opens on Macros.
- **The trash bin is never exported or imported** (2026-11-21): exports are built from tree nodes,
  so the bin isn't in them, and while the bin holds anything, export messages, the Export submenu
  and the Export all tooltip say "Items in the trash bin (F/M) aren't exported". Importing a
  whole `macros.json` drops its bin, with a warning in the Import window.

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
  - **Mouse 3/4/5 need Ctrl, Alt or Shift** (decided 2026-11-21, from the todo list): plain mouse
    buttons are often dodge or a skill, so they'd send a macro by accident. A plain mouse hotkey
    saved by an older build is kept in `macros.json` (`Hotkey::KeepOnLoad`, no load warning) but
    never fires (`Hotkey::Fires`); the tree shows `[Mouse 4: off, needs Ctrl/Alt/Shift]`, the
    editor shows it in red with a note to change it, and the quick menu doesn't show the key.
    Ctrl/Alt/Shift + left/right click stay refused (they do things in GW2).
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

- Format: **`YYYY.MM.DD.HHMM.Channel`**, the date and time the version was set, **both in UTC** (24-hour; the date is the UTC date too,
  so late-evening US releases carry the next day's date),
  then `Dev`, `Beta` or `Rel` (normal release). Example: `2026.11.21.1412.Dev`. Every release is
  `Dev` until initial development is finished.
- `tools\set_version.ps1 [-Channel Dev|Beta|Rel]` sets it to now and writes `src/Version.h`
  (plain `#define`s), used by `Globals.h` and by the `.rc` file. Run it once per release and
  commit the header.
- Nexus gets the four numbers (`AddonVersion_t`: 2026, 11, 21, 1412; each fits 16 bits). The
  text form is shown in the Nexus log, the library window, `{nexus}` and error reports.
- The DLL's version resource (Properties > Details): File version and Product version are the
  text form, the fixed file version is the four numbers.
- **Build stamp** (added 2026-11-21): the version is set per release, so test builds in between
  share it. Every build therefore also carries a stamp, written fresh before each build by
  `tools\build_stamp.ps1` (a pre-build step; the header lives under `build\`, never committed):
  git's short SHA, `+changes` if tracked files had uncommitted edits (untracked files don't
  count), and the UTC build time, e.g. `db84a1e+changes, 2026-11-21 19:20 UTC`. Shown in the Nexus
  log (`Loaded 2026.11.21.1412.Dev (build ...).`), under the version on the options page, in the
  error report's first line, and as **Comments** in the DLL's Properties > Details. `{nexus}`
  stays the version only (it goes into chat). Without git the commit is `unknown`.
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
9. Error window: every error in its own window with OK and Copy to clipboard. *(done; not yet
   tested in game)*
10. Trash bin (with its setting) and the character counter under the message box. *(done; not
    yet tested in game)*
11. Search box, importing a whole library, and mouse hotkeys needing a modifier. *(done; not yet
    tested in game)*
12. "Only active here" filter in the library and a search in the quick menu. *(done; not yet
    tested in game)*
13. Languages: English, German, French and Spanish built in, Nexus' language followed, extra
    language files from the localization folder. *(done; not yet tested in game; the German,
    French and Spanish texts need a native speaker's review)*
