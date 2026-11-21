# To-do and ideas

A place to collect ideas, brainstorming and possible features before they are designed or built.
Items here are **not decided**. Once an item is agreed, move it into `docs/design.md` (or a PR)
and remove it here.

## Ideas

### Trash bin for deleted macros and folders (added 2026-11-20)

Explore a trash bin where deleted macros and folders go, so they can be restored later.

- Items in the bin can't be used directly: they can't be sent, edited or triggered by a hotkey.
  They must be restored first.
- **Right-click the trash bin:** "Empty trash bin" deletes everything in it permanently.
- **Right-click an item in the bin** (macro or folder):
  - **Delete permanently.**
  - **Restore.** The item goes back into the folder it came from. If that folder (or any folder
    above it) no longer exists, ask the user to either:
    - recreate the missing folder(s), or
    - put the item at the top level, where it can be moved later.

Points to settle when we discuss it:
- Restoring when a same-named item already exists in the original folder: rename, replace, or ask?
- Restoring a folder whose original location would now go past the 6-level limit.
- Whether the bin has a size or age limit (e.g. auto-empty after N days), or stays until emptied.
- Where the bin lives: in `macros.json`, or a separate `trash.json`.
- Whether the delete confirmation is still needed once deleting is reversible.

### On-screen icon bar for macros (added 2026-11-20)

Explore a small on-screen window that shows macros as icons, so a macro can be sent with one
click.

- A set of icons to choose from (Claude designs up to 30 full-colour icons, embedded as
  resources). In the macro editor, the user can assign one to a macro.
- The user can turn the icon window on. It shows only the icons, nothing else.
- Hovering over an icon shows the macro's title (and maybe its lines, like the quick menu).
- Clicking an icon sends the macro, exactly like its hotkey or the quick menu.
- An icon only shows when the macro's filters say it's active on the current map, so the window
  changes as you move between maps and game modes.

Points to settle:
- Layout: a row, a column or a grid; icon size; whether the window can be moved, locked and
  made click-through when not hovered; its position saved.
- Order of the icons (tree order, or the user's own order), and what happens when none are
  active (hide the window, or show an empty frame).
- Whether it hides during combat, cutscenes or loading screens (like the rest of Nexus UI), and
  whether a click can ever reach the game behind it (it must not, and it must not steal camera
  or movement input).
- Icon style: which 30 icons (callouts, emotes, LFG, guild, trade, waypoint...), how they look
  next to the GW2 UI, and whether users may add their own images later.
- Whether the same icon may be used for several macros.

### Windows file picker for import and export (added 2026-11-20)

Phase 7 uses the `exports\` and `imports\` folders, a typed path and the clipboard, because a
modal Windows dialog over the game could grab input or hide behind a fullscreen game. Look at
whether a non-modal file picker (on its own thread, no owner window) works well in game.

### Special character picker (added 2026-11-20)

Add an icon next to the message box that opens a small window of special characters, so they can
be clicked into a message.

- Only offer characters that actually show up correctly in Guild Wars 2's chat. This needs
  research: which symbols the game's chat font can draw (for example arrows, hearts, stars,
  music notes, box and block characters), and which ones show as empty boxes or get stripped.
- Points to settle:
  - Whether to test each character in game ourselves, or start from a community-tested list.
  - The addon's own window has to draw the characters too. That means adding those glyph ranges
    to the ImGui font, or the picker will show "?" boxes even when the game can show them.
  - Grouping (arrows, symbols, shapes...), a "recently used" row, and inserting at the text
    cursor rather than at the end of the message.
  - Each special character counts toward the 199-character limit (one or two units).

### Multi-language UI (added 2026-11-20)

Translate the addon's own text (windows, buttons, messages, tooltips) and make it multi-language
by design.

- Pick the language automatically from the game client's language if possible, with a manual
  override in the settings.
- Points to settle:
  - How to detect the client language. Options to research include Nexus' localization
    settings (Nexus has its own language setting and a `Localization_*` API) and the game's own
    settings or launch arguments. Mumble Link doesn't report it.
  - Which languages: the game's English, German, French and Spanish to start (the raw-text parser
    already understands their chat commands), then maybe more.
  - How to store translations: Nexus' `Localization_Set`/`Translate` with per-language files,
    or our own string tables.
  - All UI text needs to move to string keys. It's easier to start this early than to retrofit it
    later.
  - Fonts: German, French and Spanish accented letters need to be in the font's glyph ranges.

### Character counter under the message box (added 2026-11-20)

Show a counter directly under each message input box: characters typed / maximum (for example
`23/199`).

- Today the lines editor shows the count as a small label to the right of the message box, and
  raw mode shows no count per row.
- Points to settle:
  - Whether the count includes the channel command (`/s `, `/w Name`) the way the game counts it,
    or only the message (and the maximum shrinks by the command's length).
  - Raw mode: a counter per row in the "Will send" list, or one under the box for the row the
    cursor is on.
  - Colours: normal, a warning near the limit (for example 180+), red over the limit.

### Warn when a hotkey is also a game keybind (added 2026-11-20)

Look into ways to read the game's current keybinds, so the hotkey editor can warn "Ctrl+F1 is
also bound in game to …" (hotkeys never block a key, so a shared key does both things).

Options to research:
- **Nexus GameBinds:** Nexus keeps its own table of the game's binds (set in Nexus > Options >
  Game Keybinds). API v6 only exposes `GameBinds_IsBound` and press/release, not which key a bind
  uses. Check whether a newer API version adds a getter, or whether we could ask Raidcore for one.
- **The game's keybind files:** GW2 can save keybind profiles as XML (Options > Control Options
  > Save, into `Documents\Guild Wars 2\InputBinds\`). We could read the newest one, or one the user
  picks. A saved profile can be out of date compared with the live settings.
- **Live settings:** the game keeps its live binds in `Local.dat`, which is binary and
  undocumented. Probably not worth reading.
- **Fallback:** a short built-in list of GW2's default keys (1-0, F1-F4, WASD, Tab, T, ...) to warn
  about common clashes, clearly labelled as "default keys, yours may differ".

Points to settle:
- Whether the warning shows while picking the key, in the tree, or both.
- What to do with the game's secondary binds (each action has two).
- Mouse buttons in the game's binds (Mouse 4/5 are often bound to dodge or skills).

### Mouse buttons as hotkeys, with a required modifier (added 2026-11-20)

Look at letting mouse buttons send a macro only together with Ctrl, Alt or Shift.

- Today (PR #5): Mouse 3 (middle), Mouse 4 and Mouse 5 can be hotkeys, with or without a
  modifier. Left and right click are always refused.
- Ideas to look at:
  - Require a modifier for every mouse-button hotkey, so a plain Mouse 4/5 (often dodge or a
    skill) can't send a macro by accident.
  - Whether Ctrl/Alt/Shift + left or right click could be allowed safely. In GW2, Shift+click and
    Ctrl+click already do things (chat links, targeting, ground-target skills), and clicks over
    addon windows go to ImGui, so this needs careful in-game testing first.
  - Extra mouse buttons beyond 5 (gaming mice usually send them as keyboard keys, which already
    work).
- Depends on in-game check F2: whether mouse buttons reach our hook at all while the pointer is
  over the addon window.

### Map ids from the game, with a wiki fallback (added 2026-11-20)

Look at ways for the addon to fill in map ids (and names) itself, so nobody has to look ids up
by hand for the map filter.

- **What the game gives us:** MumbleLink only reports the *current* map id and map type. That's
  what "Add this map" uses today (PR #7). The game has no API for listing all maps.
- Options to look at:
  - **Remember visited maps:** keep a small list of every map id seen in MumbleLink, so the filter
    can offer "maps you've been to" in a picker.
  - **Names from the official GW2 API:** `/v2/maps?ids=...` (no API key needed) for the names of
    known ids, cached on disk. Phase 6 plans a map-name cache for `{map}` (no waypoint or POI
    data: decided 2026-11-20).
  - **The full list to search:** `/v2/maps` lists every map id. Fetched once and cached, it would
    allow a "search by name" picker (e.g. type "Lion's Arch").
  - **Wiki fallback:** when the API is down or a map is missing from it, look it up on the GW2
    wiki (its MediaWiki API, or its map id tables). The wiki also knows old and removed maps.
- Points to settle:
  - Network use: one request per new map, a one-off full list, or both. Respect the API's rate
    limits and keep the cache valid across game patches.
  - Several maps share a name (instances, story versions, festival copies). The picker should show
    the id and region too.
  - Works offline: ids still work without names; names just show as "Map 1234".

### Refresh map info

- On the first launch (and maybe after a game build change), check the API/wiki for new or
  updated map info, instead of only looking up maps as you visit them (PR #9's cache).
- A button in the settings window to force that check now.
- To discuss: how to handle a map's name or id changing (renames, maps that are removed or
  replaced with a new id). Filters store ids, so a rename only changes the label, but a new id
  would silently stop a filter from matching. Options: keep old names alongside new ones, flag
  filters that use ids the API no longer knows, or offer to swap to the new id.

### More text sizes from a font of our own (added 2026-11-21)

The Text size dropdown (built, see `docs/design.md`, "Text size") offers only the fonts Nexus
provides, so there are usually just 2-4 sizes. If that's not enough:

- Add a font of our own with `Fonts_AddFromFile`/`FromMemory` and change its size with
  `Fonts_Resize` (it arrives through a callback; never keep the `ImFont*`), offering for example
  12-28 px, or a slider.
- Whether to follow the game's own interface size (MumbleLink's identity `uisz`) as the default.

### Publish the wiki (reminder: do this once the repo is public)

The usage docs are written in `docs/wiki/`. A private repo on GitHub's free plan can't have a
wiki, so they wait here.

Before making the repo public: the whole commit history becomes public too, including commit
author names and email addresses. Check you're happy with that.

Your steps (on github.com, in the repo):
1. **Settings > General**, at the bottom under **Danger Zone**: **Change visibility** >
   **Make public**, and confirm.
2. **Settings > General > Features**: tick **Wikis**. Optionally also tick **Restrict editing to
   collaborators only**, so only you can edit it.
3. Open the new **Wiki** tab and click **Create the first page**. Leave the title as "Home",
   type anything in the body, and click **Save page**. (GitHub only creates the wiki's git repo
   after this first page exists. It will be replaced.)
4. Tell Claude "the wiki is ready".

Claude's steps:
1. Clone `https://github.com/Marthisdil/GW2_Chat_Messages.wiki.git` into a temporary folder.
2. Copy every page from `docs/wiki/` into it (replacing the blank Home page), point the image
   links (`../images/...`) at the images in the main repo, commit and push.
3. Check that each page and the sidebar show up and that the links between pages work.
4. Update the README's Documentation section to link to the wiki, and decide with you whether
   `docs/wiki/` stays as the source (and gets pushed again after each change) or is removed.

## Open questions

- Whisper sending: find a way that needs no Tab key (test in game before building the sender).
- Check ArenaNet's current macro policy before the first public release, and mention the
  multi-line sending when registering the addon with Raidcore.
