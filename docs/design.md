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
  A `macros.json.bak` of the previous version is kept, and backups on a schedule (see "Backups").
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
  - The quick menu lists only active macros, and hides folders without any (except the
    Favourites at its top, shown everywhere, faded). The library tree
    shows every macro, with inactive ones dimmed. **Send now** in the library always works.
  - **Folder filters** (built 2026-11-24 from the todo list; owner's decisions): a folder has
    game modes and a map list too, and **they add up** with the macros' own: a macro (or
    folder) is active only where its own filter and every folder above it allow. Game modes:
    every one on the way up must allow the mode. Maps: every list that isn't empty must have the
    map (an empty list = any map). `MacroTree::ReachOf` combines them (`Reach`: modes, any map or
    the maps in common), used by `IsActive`, `NeverActive` and `FiltersOverlap` (hotkey clashes,
    in the editor and the import preview), so hotkeys, the quick menu, favourites' fading and
    **Only active here** all follow folders. A macro moved into a filtered folder follows it.
    - Why add up and not replace: a folder can only narrow things, so "WvW only" on a folder
      guarantees nothing inside fires in PvE, and a macro's own filter is never silently
      ignored. The catch, filters with nothing in common (a PvE macro in a WvW folder), is shown:
      the editor says "can't be active anywhere".
    - **Where it's set:** the name pop-up (new folder, rename a folder) has **Active in:** PvE /
      WvW / PvP (at least one stays ticked) and says the maps are in the folder's details. The
      folder's details pane has the same **Where it's active** section as a macro (game modes,
      maps, map search, "Active here"). Both editors list "Also limited by the folders above it:
      Raids (PvE), ..." and warn when the node can't be active anywhere.
    - **In the tree:** a folder with its own filter shows a tag after its name, `[PvE]`,
      `[WvW, PvP]`, `[3 maps]`, `[WvW, 2 maps]` (no tags on macros: they already dim). A folder
      that isn't active here (its filter or one above it) is dimmed like a macro, and so is
      everything inside it.
    - **macros.json:** a folder writes `"game_modes"` and `"map_ids"` only when it has a filter,
      so folders without one look as before; they're read like a macro's. No migration: there's
      no release yet, and the owner starts with a fresh library (planning for future format
      changes is a todo item).
    - **Import / export:** an exported folder carries its filter. An imported new folder, or a
      folder that **Replace**s one, brings its filter; **Merge** keeps the existing folder's. A
      too-deep folder that's left out with its contents moved up (**Move the contents of
      too-deep folders up a level**) loses its filter, so those macros are less limited.
  - Map names (GW2 API) come with phase 6; for now maps are entered by id or with "Add this map".
  - **Map picker** (built 2026-11-23 from the todo list, section 2): under the map buttons, a
    "Find a map by name or id" box lists up to 12 matches from the GW2 API's full map list
    ("Name - Region (id)", case and accents ignored; maps you've been to first, then by name), and
    a "Maps you've been to" dropdown lists the last 30 maps you visited (every map change is
    remembered, at most 200, in `maps.json`). Clicking one adds it; one already in the list is
    shown greyed. A map id the full list doesn't have is drawn in yellow with "(!)" and a tooltip:
    removed or replaced in a game update (the filter then never matches) or an instance the API
    doesn't list. Filters keep storing ids, so a rename only changes the label. Rules in
    `src/MapCatalog.*` (tested in `tests/MapCatalogTests.cpp`); network and threads in
    `src/MapNames.*`. Not done: the GW2 wiki as a fallback (the API covers the maps people use;
    the wiki refuses automated requests).
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
    reversed), and `{nexus}` (bhm's `{blish}`: "Chat Messages 2026.11.21.1412.Dev").
  - Also built: `{map}` (the current map's name).
  - **Removed (owner, 2026-11-23): `{txt file}` and `{json path url}`.** A shared or imported macro
    could use them to type out a file from the addon folder or to make the PC fetch an address
    (SEC-012, SEC-013). A line that still has one is refused when sent (`Expand` gives an error;
    nothing is typed, and it's never sent as plain text), the editor shows a red note under it,
    and the import preview marks the macro REMOVED PLACEHOLDER (`Placeholders::UsesRemoved`).
    `Http::GetPublic` and `src/NetAddress.*` went with `{json}`; `Http::Get` stays for map names,
    with an address built into the addon.
  - Map names: `src/MapNames.*` asks `https://api.guildwars2.com/v2/maps?ids=...&lang=<lang>` (no
    API key) and caches the names in `<GW2>\addons\GW2_Chat_Messages\maps.json`. The current map is
    looked up in the background whenever it changes; ids the API doesn't know aren't asked again
    for 5 minutes. A bad cache file is just rebuilt. The filter editor shows names instead of ids.
  - **Names follow the addon's language** (2026-11-23): German, French, Spanish and Chinese where
    the API has them, English otherwise (`MapCatalog::ApiLang`). `{map}` types the name in that
    language. Changing the language throws the names away (the visited maps are kept) and fetches
    them again.
  - **The full list** (2026-11-23): `/v2/maps` (every id), then the names 200 ids at a time with a
    short pause between requests, on a background thread: on the first run (10 seconds after
    load, so the language has settled), when the list is a week old, when the language changes,
    and with **Refresh map list** on the options page (which also shows how many maps and from
    when, or why the last refresh failed). The list and names are only replaced when every
    request worked. `maps.json` is format 2: `lang`, `maps` (id, name, region), `listed`,
    `listed_at` (UTC seconds) and `visited`; a file in format 1 is rebuilt. Names and regions must
    pass `Localization::SafeText` and be at most 200 bytes; the file is read only up to 4 MB.
  - Syntax: `{name arg arg}`, case-insensitive, arguments separated by spaces.
  - Differences from bhm, on purpose: an unknown `{word}` is sent as typed (bhm drops the whole
    line), and a known placeholder that fails stops the whole macro with a message (bhm silently
    drops that line).
  - Filled in on the sender's worker thread (`{map}` may wait on the network), before the plan is
    built, so the 199-unit check uses the real text and nothing is typed if a placeholder fails.
    The status shows "Preparing..." meanwhile. A value isn't expanded again. The editor shows "~" on lengths of lines with placeholders.
  - A value can't add a key or a command (SEC-013). Its control characters (C0, DEL, C1) become
    spaces. `SendPlan::Build` refuses typed text with any control character, and a "/" that came
    from a value where chat reads a command: at the start of a box we type into (after any kind of
    space, even after our own `/s `), or right after a command word and its spaces. The game reads
    commands as the text arrives and a channel command empties the box, so in `/g {map}` a value
    `/w Name` would whisper. Kept after `{txt}`/`{json}` were removed: `{map}` and `{today}`
    still bring in outside text. `Placeholders::Result::FromValue` marks the bytes that came from values.
  - Waypoint and other chat links (`[&BDAEAAA=]`) need no feature: they're typed as text and the
    game turns them into links.
- **Preview** (built 2026-11-23): a button next to **Send now** shows what it would type, line by
  line, without sending anything. It goes through `SendPlan::Prepare` (fill in placeholders, then
  `Build` with their origins), the same function a real send uses, so every check runs (199
  units, control characters, SEC-013 commands, an empty whisper recipient or one over
  `ChatLine::kMaxRecipientLength`, 29 units: SEC-035). `SendPlan::Describe`
  turns the plan's steps into the text per box: `/s hello`; for a whisper `/w Name`, then Tab,
  then the message; for a squad broadcast (as commander) just the message. On the render
  thread `{map}` uses only the map name cache (`Sender::Preview`): an unknown name is requested in
  the background and the preview says to try again, so the game never waits on the network. An
  error opens the error window ("Clicked Preview for ... (nothing was sent)"). The preview stays
  under the button until the macro's lines change, another macro is selected, or **Close
  preview**. `{random}` and the time are filled in again on the real send, and the preview says
  so.
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

## Starter library

Built 2026-11-23 (todo section 2). On the **first run** (no `macros.json` yet), the library starts
with an **Examples** folder, with nested folders, so new users see what a macro can do:

```
Examples
├── Hello                      Say: "Hello everyone!"
├── Basics
│   ├── Two channels           Say, then Party
│   ├── Whisper a friend       a whisper to "Friend Name"
│   └── Wave                   an emote
├── Placeholders
│   ├── Roll a die             {random 1 6}
│   ├── Time check             {time}
│   └── Where am I             {map}
└── Filters
    ├── In WvW
    │   └── Stack on tag       squad (broadcast as commander), WvW only
    └── In Lion's Arch
        └── Meet at the bank   map chat, only in Lion's Arch
```

- **In the addon's language** at the moment it's added (names and lines are `TR` texts, so
  de/fr/es get their own; map names in the lines are written per language). No hotkeys and no
  cooldowns: nothing fires until the user sets a key.
- **Once only:** `settings.json` gets `"examples_added": true` the first time the addon loads
  (also when a library already exists), so a `macros.json` removed on purpose later starts
  empty, as before. A library restored from the `.bak` doesn't get examples either, and a
  missing `macros.json` with its `.bak` still there isn't a first run (saving the examples
  would soon overwrite that backup).
- **Getting them back:** the options page has **Add the example macros** (decided by the owner
  2026-11-23: ask first, replace, never merge). The button only opens the library; the library
  window asks, at its own ID level:
  - **An Examples folder exists** (`Examples::Find`, by the reserved id, so a renamed or moved
    one counts): "Replace the examples?", with how many folders and macros go and that macros
    the user added or changed there go too. **Replace** puts a fresh copy in the old one's place
    (same folder and position; the top level if its sub-folders wouldn't fit within 6 levels).
    The old folder goes to the trash bin when the bin is on (as an ordinary folder: it gives up
    the reserved id first, so a restore never brings back a second tracked one), otherwise it's
    deleted for good, and the prompt says which. No merge and no "Examples (2)" beside it.
  - **None in the tree, but `"examples_added"` is set** (added at the first run or by the button
    before): "Add the examples again?". An old one in the trash bin gives up the id to the new
    one and stays there as an ordinary folder.
  - **Never added** (a library from before the starter library): added at once, and the setting
    is set.
  The new folder is selected and shown, with a status line. `Examples::AddAgain`, `Replace` and
  `Tree::ReleaseReservedId`, tested in `tests/ExamplesTests.cpp`.
- `src/Examples.*` builds it (no Nexus or ImGui), tested by `tests/ExamplesTests.cpp`: the shape,
  every line within 199 units with no removed placeholder, a save and load round trip, and the
  "(2)" names. It's an ordinary folder: edit, move or delete it like any other.
- **A reserved id** (decided by the owner, 2026-11-23), so the addon can always find the
  Examples folder it made, even after a rename or a language change (for the "ask first,
  overwrite" idea in `docs/todo.md`). The top Examples folder gets
  `MacroTree::kReservedExamplesId` (`Examples::kFolderId`, `4558414d504c4531` in
  `macros.json`), and nothing else can ever hold it:
  - `Tree::NewId` never returns it (nor 0), so no folder or macro the user or the addon makes
    gets it. Only `Tree::ClaimReservedId` gives it out, to a folder in the tree, and only while
    no folder in the tree or the bin has it.
  - Loading keeps it only on a folder, and only once (like any id); a macro with it, or a second
    folder, gets a new id with the usual "Gave new ids" warning. A hand-edited or damaged file
    can't move it onto something else.
  - Imports and copies always get new ids, so someone else's exported Examples folder comes in
    as an ordinary folder.
  - In the trash bin it still counts (a new copy gets an ordinary id), and restoring the folder
    keeps it. A folder rebuilt by a restore (for an item whose folder is gone) never takes it.
  - A second copy ("Examples (2)") has an ordinary id.
  Tested in `tests/ExamplesTests.cpp`.

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
- **Core modules and worker threads** (built 2026-11-23 from the todo list): messages built in
  the modules with no UI dependency or on another thread (tree rules, import, sender status and
  errors, link files, hotkey refusals, placeholders, map lookups, the web request errors, loading
  `macros.json`) use `TR("...")`. `Localization::Translate` is thread-safe: `SetActive` keeps a
  copy of each language's texts that lives until `ClearShared` in `AddonUnload` (after every
  thread has stopped), so a text handed to the sender stays valid if the language changes; going
  back to a language reuses its copy. Messages are whole sentences with `%`-codes (no English
  pieces glued together), and "folder"/"macro" variants are separate texts for languages with
  gender. Code that checked a message's English text now checks a flag (`AddTextMacro`'s
  `aTooBig`) or the translated format (`MacroTree::IsNewIdsWarning`).
- **Key names** are shown translated (`Hotkey::Display`, `DisplayKeyName`: "Maus 4", "Bild auf");
  `Hotkey::ToString` stays English, because that's what `macros.json` and exports store. The
  library search matches both forms. Ctrl, Alt and Shift keep their names in every built-in
  language (the existing texts already write them that way).
- **Not translated:** the Nexus menu names quoted in texts (Nexus > Options > Game Keybinds), and
  the headings of the error report copied to the clipboard ("What I did:", "Error:"), which are
  meant for bug reports.
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

## Backups

*(Built 2026-11-23 from the todo item "Daily rolling backups", widened by the owner. Rules in
`src/Backups.*` (schedule, file names, which copies go, the setting), tested in
`tests/BackupsTests.cpp`; files in `Storage` (`BackupNames`, `WriteBackup`, `DeleteBackup`,
`ReadBackup`); UI in `Ui.cpp`.)*

- **Options page tabs:** **General** (everything from before), **Backups**, **Restore**.
- **Setting** (`settings.json` `"backups"`: `enabled`, `unit` `hours|days|weeks|months`,
  `every`, `keep`). Off by default; when turned on: every 1 day, keep 7. `every` is 1-48 hours,
  1-30 days, 1-8 weeks or 1-12 months; `keep` 1-50. Anything out of range is brought into range,
  anything unreadable falls back to the default.
- **Where:** `<addon>\backups\`, created at load. File names carry the time in UTC:
  `macros_backup_YYYY-MM-DD_HH-MM-SS_UTC.json`, and `macros_safety_..._UTC.json` for the safety
  copies. Only names of exactly that form are backups: nothing else in the folder is listed,
  read or deleted, and a name is checked before every read or delete.
- **Content:** the library as `macros.json` would be written (with the trash bin), from memory,
  so unsaved edits are in it too. Written via a temp file and rename; an existing name is never
  overwritten.
- **When:** the render thread looks once a minute. Due when there's no copy of that kind yet, or
  the newest is `every` units old (months are calendar months in UTC, clamped to the month's last
  day). A copy dated in the future (clock changed) waits. An empty library, or one identical to
  the newest copy of that kind, isn't copied; it's looked at again after the next change. A
  failed write is retried after an hour (logged).
- **Off:** a safety copy once a week, the last 4 kept, without any message. The two kinds are
  pruned separately: turning backups off or on never deletes the other kind.
- **Keeping:** after each new copy, the oldest of the same kind beyond `keep` are deleted (never
  the one just made).
- **Back up now:** a backup at once, even if unchanged; counts toward `keep`. A second one in the
  same second is refused (error window).
- **Restore tab:** every backup and safety copy, newest first: Date (UTC), Time (UTC), Folders,
  Macros (the tree, not the trash bin), Type, and **Restore...**. Hovering the date shows the file
  name. The files are read a few per frame (at most 32 MB each); one that can't be read shows
  "Can't be read" with the reason, and no button. **Refresh** reads the folder again.
- **Restore:** a pop-up asks first (date, time, counts; "the trash bin too"). The file is read
  and checked into a new tree before anything changes; then the whole library is replaced, the
  library window opens, and **Undo restore** (the Undo import bar) puts the old one back until
  the game closes. Link files sync as at a start (the file wins), so a restore never writes old
  lines into a linked `.txt`.

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
- **Favourites and recently sent** (built 2026-11-23 from the todo list): the quick menu (both
  the pop-up and the Quick Access menu) lists, above the folders, **Favourites** (folders and
  macros marked as favourite, in tree order) and **Recently sent** (the last sent, newest first,
  without the favourite macros), each as "Name  (A > B)" like search results. Both are hidden
  while the menu's search box has text. Rules in `src/QuickLists.*` (no Nexus or ImGui, tested
  in `tests/QuickListsTests.cpp`).
  - **Favourites everywhere** (owner, 2026-11-23): favourites are listed on every map, whatever
    their game mode and map filters. One that isn't active here is drawn faded (alpha 0.45,
    like the library's dimmed macros), its preview says so, and clicking still sends it.
    **Hotkeys still follow the filters.** A favourite folder is a sub-menu with everything
    inside it, at any depth, whatever the filters (inactive entries faded, sub-folders with no
    macro left out); the folder is faded when nothing inside is active here. A favourite inside
    a favourite folder is listed on its own too. The quick menu shows its lists whenever a
    favourite exists, even on a map where nothing is active. Recently sent and the menu's search
    still show only what's active here.
  - **At most 10** (`QuickLists::kMaxFavourites`; folders and macros together, trash bin not
    counted). Marking one more is refused through the error window, saying to untick one
    first. A hand-edited `macros.json` with more keeps them all marked; the first 10 in tree
    order are listed.
  - **Favourite:** `"favourite": true` on a folder or macro in `macros.json` (left out when
    false; any other value is dropped with a warning). Set by the **Favourite** box (a macro's
    editor, a folder's details) or the tree's right-click **Favourite** (a check mark; a queued
    action). The tree shows `  *` after the name. Exports leave it out at every level, and
    imports never set it (a replaced macro keeps its own).
  - **Recently sent:** every send that starts (hotkey, quick menu, Send now) moves the macro to
    the front; at most 10 ids are kept, in `settings.json` `"recently_sent"` (16-digit hex ids,
    like `macros.json`). Ids that are gone or in the trash bin are skipped. The General tab's
    **Recently sent in the quick menu** sets how many show, 0-10 (`"recent_count"`, 5 by
    default), with **Clear the list**.
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
unload. Hotkeys are kept only if "Keep hotkeys" is ticked; it starts unticked for every
import, each time a source is loaded (changed 2026-11-22, security review: a shared file's
hotkey would send its macro on an ordinary key press). While it's ticked, the preview shows
each macro's hotkey as `Name  [key]`. Link files are never exported or imported. A Replace
keeps the existing item's id and place. Since 2026-11-23 the addon creates `imports\` and
`exports\` when it loads (`Ui::Init`), the toolbar has **Exports folder**, and the "Exported to
..." message has **Open exports folder**; the Import window already had **Open imports
folder**.)*

- **Export**: the whole tree, one folder, one macro, or a selection. The format is
  `ChatMessages.export.json` (`{ "format": 1, "root": Node }`). A single macro can also be
  exported as raw `.txt`.
  - **A selection** (built 2026-11-24 from the todo list): **Export some...** in the toolbar, or
    right-click > Export > **Choose more to export...**, opens the **Export** window (it starts
    with the selected or right-clicked item ticked). It shows the library as a tree with a tick
    box per folder and macro, **Select all** / **Deselect all**, "N folder(s) and M macro(s)
    ticked", a file name (`Chat Messages selection` by default; `.json` is added, a taken name
    gets " (2)") and **Save as a .json file** / **Copy to the clipboard** / **Exports folder** /
    **Close**. The ticks follow the import preview's rules: a folder's tick ticks or unticks
    everything inside; ticking an item ticks the folders above it; a folder whose contents are
    all unticked is unticked (an empty folder keeps its own tick). The file holds the ticked
    items in library order, in their folders: a part-ticked folder holds only its ticked
    contents. Link files, favourites and the trash bin stay out, as in every export. No `.txt`
    here (that's right-click > Export on one macro). The ticks are ids
    (`Exchange::Pick`), so editing the library while the window is open is safe; items that are
    gone are ignored. Rules in `src/Exchange.*` (`SetPicked`, `ExportPickedJson`), tested in
    `tests/ExchangeTests.cpp`.
- **Import sources**: our JSON format, a raw `.txt` (becomes one macro), or a folder of `.txt`
  files (sub-folders become folders).
  - Raw text (`.txt` imports, pasted text and link files) is read as UTF-8. Bytes that aren't
    UTF-8 (an ANSI file, say) become U+FFFD, with a warning (`ChatLine::RepairUtf8`); they're
    not guessed from the ANSI code page. So the library always saves as valid JSON. For a link
    file the repair alone never writes the file back. Text over its limit once repaired (each
    byte becomes three) is refused: a `.txt` import is left out, a link file isn't read, and a
    macro whose file text would be over 256 KB isn't written to its link (a warning) (SEC-023).
  - A folder import reads at most 2000 `.txt` files and 16 MB of text (`kMaxFolderBytes`,
    SEC-025). Anything that throws while reading becomes the package's error; if `Apply` throws
    (out of memory), the library is put back from the copy made for Undo import and the error
    window says nothing was imported.
  - At most 200 warnings (`kMaxWarnings`) are kept from one file, then "...and N more problem(s)
    like these." (SEC-022). File names and typed paths go through `PathText`, which never
    throws (SEC-020, SEC-024).
- **Preview step** (always shown):
  - A tree showing the result, with the new folders, new macros and merged folders marked.
  - The user picks the target folder.
  - For name conflicts the user can choose Rename (`Name (2)`), Skip or Replace. This can be set
    per item or for all items.
  - Depth violations are flagged. Those items are blocked, or flattened to the deepest allowed
    level if the user chooses that.
  - **Flags** (added 2026-11-23, `PlanItem` and `Plan` counts): imported macros that still use
    the removed `{txt}` or `{json}` (`Placeholders::UsesRemoved`, the same name parsing as
    `Expand`) in a message or whisper recipient, marked REMOVED PLACEHOLDER (they won't send); with Keep hotkeys ticked, hotkeys with no Ctrl/Alt/Shift
    (`EHotkeyRisk::NoModifier`) or equal to one of the user's library hotkeys where both can be
    active (`Clash`, `FiltersOverlap` as in the editor, since such a key sends nothing; not the
    macro being replaced, nor anything inside a folder being replaced). **Replace never brings
    a risky hotkey** (`HotkeyCleared`, owner 2026-11-23: only on a clash, not always); a replaced
    macro losing its own hotkey is shown too (`LosesOwnHotkey`). New macros keep a flagged
    hotkey (only marked). Each flag has a row mark with a tooltip and a counting line above the
    preview; after Import, the undo line counts the replaced macros whose hotkey changed.
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
- **Tab only for whispers, only inside chat.** The game needs Tab for whispers (in-game check C2,
  2026-11-25): it doesn't split `/w Name message` on a space, typed or pasted, so there's no way
  around it. Tab is only sent while a chat box is focused, and focus is verified as released
  afterwards (the Enter's wait).
- **Whispers wait for the game's boxes.** `/w ` opens the recipient box and Tab moves to the
  message box, and each takes the game a few frames: text typed sooner is lost (C1 failed on
  2026-11-25 this way: the name went in, the message didn't, and Enter sent nothing). The plan
  pauses 100 ms and 2 drawn frames after `/w `, and 200 ms and 3 drawn frames after Tab
  (`SendPlan`, `kPauseAfterWhisperCommandMs`, `kPauseAfterTabMs` and their `kFrames...`), so a
  low frame rate still gets enough time. The sender counts frames from the render callback
  (`Sender::FrameRendered`); if none are drawn for 1.5 s it closes chat and stops with an error.
  To be tuned in game.
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
  - **GW2's default keys** (built 2026-11-23 from the todo list, the "fallback" option): when a
    hotkey is one of the game's default binds (`src/GameKeys.*`, tested in
    `tests/GameKeysTests.cpp`: about 90 combos from the GW2 wiki's Options page, PC with a US
    keyboard, checked 2026-11-23), the editor says so in yellow ("In GW2's default keys, F1 is
    \"Profession skill 1\"...") and that the user's own keys may differ. Exact matches only
    (Ctrl+1 isn't Weapon skill 1). Plain Tab, Enter, Space and Backspace aren't listed: they're
    refused anyway. It replaces the plain-letters warning for those keys; other plain letters and
    numbers still get it. Not done: reading the user's real binds (Nexus API v6 has no getter;
    the game's saved XML profiles can be out of date).
  - Keyboard keys plus Mouse 3, 4 and 5, with Ctrl, Alt and Shift. Refused: left/right mouse,
    Escape, modifier keys alone, and plain Tab, Enter, Space and Backspace.
  - **Mouse 3/4/5 need Ctrl, Alt or Shift** (decided 2026-11-21, from the todo list): plain mouse
    buttons are often dodge or a skill, so they'd send a macro by accident. A plain mouse hotkey
    saved by an older build is kept in `macros.json` (`Hotkey::KeepOnLoad`, no load warning) but
    never fires (`Hotkey::Fires`); the tree shows `[Mouse 4: off, needs Ctrl/Alt/Shift]`, the
    editor shows it in red with a note to change it, and the quick menu doesn't show the key.
    Ctrl/Alt/Shift + left/right click stay refused (they do things in GW2). Decided for good by
    the owner on 2026-11-23 (todo list): never left or right mouse, with or without a modifier,
    as a safety measure; the README and the wiki say why.
  - Ignored: auto-repeat, a focused game text box (Mumble), ImGui text input in any addon
    (`WantTextInput`), and outside gameplay.
  - The window thread only queues the press. The render thread matches it against the tree and
    starts the sender. A hotkey set on more than one macro sends nothing and shows an alert.
  - **Cooldown** (built 2026-11-23 from the todo list): each macro has a cooldown, 0 to 600
    whole seconds, **off (0) by default**, saved as `"cooldown"` in `macros.json` only when it's
    on (exported and imported with the macro). Longer values are cut to 600 and anything else is
    dropped on load, each with a warning. It counts from the **start** of the macro's last send,
    from anywhere, but **only the hotkey waits**: Send now and the quick menu always send (the
    user chose them on purpose). On top of that, a **1 second guard** after any send of the macro
    ends stops a bounce or a second press from sending it twice, cooldown or not. A press that's
    ignored by the cooldown shows **one** Nexus alert per cooldown ("Say: cooldown, wait 3 s."),
    later presses are silent; the guard alone is always silent. A failed send starts no cooldown,
    only the guard. The editor has a **Cooldown:** box under the hotkey row and shows "(N s left)"
    while it runs. Rules in `src/Cooldown.*` (no Nexus or ImGui, tested by
    `tests/CooldownTests.cpp`); times are `ImGui::GetTime()` and aren't saved (a reload clears
    them).
  - A hotkey send that fails shows a Nexus alert, since the library window may be closed.
  - Only the addon's own window keybinds (show/hide the library, open the quick menu) stay Nexus
    InputBinds (`"(null)"`).

## Policy note

Raidcore's guide quotes ArenaNet's macro policy ("one user action may lead to one game action"),
and Raidcore has a "Macro Usage" label. Sending several lines from one key press is how bhm
works and is what the user wants. When registering with Raidcore, mention the multi-line
sending.

**Checked 2026-11-23** ([Policy: Macros and Macro Use](https://help.guildwars2.com/hc/en-us/articles/360013762153-Policy-Macros-and-Macro-Use)):
attended macros are allowed if they give no unfair advantage; unattended ones never are. The
examples of what isn't allowed are all gameplay (skills, skill chains, farming, events, several
accounts, bots), and each macro should be a single action that needs the user to start it
again. The addon fits that: the user starts every send, one press sends the macro once, and it
only types chat. So that users know, the policy is linked:
- on the options page, under the version and build (`kMacroPolicyUrl` in `Globals.h`, opened by
  `Storage::OpenWebPage`, which only opens `https://` addresses built into the addon), with a
  line saying the addon only sends messages the user starts and mustn't automate gameplay;
- in the README ("Macro use and ArenaNet's policy") and the wiki's
  [ArenaNet's policies](https://github.com/Marthisdil/GW2_Chat_Messages/wiki/ArenaNet-Policies)
  page (the macro and third-party program policies, what the addon is and isn't for, and the
  disclaimer).
The options page also links the repository (`kRepositoryUrl`).

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
