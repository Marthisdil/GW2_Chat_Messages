# To-do and ideas

A place to collect ideas, brainstorming and possible features before they are designed or built.
Items here are **not decided**. Once an item is agreed, move it into `docs/design.md` (or a PR)
and remove it here.

The sections are in priority order (ranked 2026-11-23): release blockers first, then quick wins,
bigger features, later items, and new ideas still to discuss.

## 1. Before a first public release

### In-game testing

Work through `docs/in-game-tests.md` (A1 passed, A2 mostly; the rest is still to do). Session B,
whether the game takes the addon's keys and text, matters most: everything else depends on it.

### ArenaNet's macro policy

Check ArenaNet's current macro policy before the first public release, and mention the
multi-line sending when registering the addon with Raidcore. Several lines from one key press is
exactly what the policy is about; if it calls for one line per press or a forced delay, it's
cheaper to know before the release. Also decides "Timed lines" (section 5).

### Whispers without Tab

Find a way to send whispers that needs no Tab key (Tab is the riskiest key the sender presses).
Test in game during session B.

### VirusTotal scan for every released DLL (added 2026-11-21)

If there's a way to automate it, submit every DLL that's to be released to virustotal.com, and
include the scan link in the release's text, for people to review if they wish. Build it with
the first release.

- Points to settle: VirusTotal has a public API (a free API key, with rate limits) that can
  upload a file and return its analysis page (`https://www.virustotal.com/gui/file/<sha256>`);
  the release workflow could upload the DLL, wait for the result, and add the link and the
  SHA-256 to the release notes. The key would be a GitHub Actions secret. Also: what to do when
  some engines flag it (false positives are common for DLLs that hook a game's window), and
  whether the release waits for the scan or the link is added afterwards.

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

## 2. Quick wins

### Translate the remaining messages (added 2026-11-21)

The multi-language UI (phase 13, `docs/design.md` "Languages") leaves some texts in English:
messages from the core modules (tree rules, import, sender status, link sync and hotkey errors,
key names, "Imported library", "From <file>"). They're built off the render thread or in modules
with no UI dependency, while `T()` is render-thread only. Options: return message ids or codes
from those modules and translate them in `Ui.cpp`, or a thread-safe lookup.

### Map ids from the game, with a wiki fallback (added 2026-11-20)

Build together with "Refresh map info" (next). Look at ways for the addon to fill in map ids
(and names) itself, so nobody has to look ids up by hand for the map filter.

- **What the game gives us:** MumbleLink only reports the *current* map id and map type. That's
  what "Add this map" uses today (PR #7). The game has no API for listing all maps.
- Options to look at:
  - **Remember visited maps:** keep a small list of every map id seen in MumbleLink, so the filter
    can offer "maps you've been to" in a picker.
  - **Names from the official GW2 API:** `/v2/maps?ids=...` (no API key needed) for the names of
    known ids, cached on disk (`maps.json`, PR #9; no waypoint or POI data: decided 2026-11-20).
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

### Warn when a hotkey is also a game keybind (added 2026-11-20)

Start with the cheap fallback (a built-in list of default keys); reading the real binds can come
later. The hotkey editor would warn "Ctrl+F1 is also bound in game to …" (hotkeys never block a
key, so a shared key does both things).

Options to research:
- **Fallback (first):** a short built-in list of GW2's default keys (1-0, F1-F4, WASD, Tab, T, ...)
  to warn about common clashes, clearly labelled as "default keys, yours may differ".
- **Nexus GameBinds:** Nexus keeps its own table of the game's binds (set in Nexus > Options >
  Game Keybinds). API v6 only exposes `GameBinds_IsBound` and press/release, not which key a bind
  uses. Check whether a newer API version adds a getter, or whether we could ask Raidcore for one.
- **The game's keybind files:** GW2 can save keybind profiles as XML (Options > Control Options
  > Save, into `Documents\Guild Wars 2\InputBinds\`). We could read the newest one, or one the user
  picks. A saved profile can be out of date compared with the live settings.
- **Live settings:** the game keeps its live binds in `Local.dat`, which is binary and
  undocumented. Probably not worth reading.

Points to settle:
- Whether the warning shows while picking the key, in the tree, or both.
- What to do with the game's secondary binds (each action has two).
- Mouse buttons in the game's binds (Mouse 4/5 are often bound to dodge or skills).

## 3. Bigger features

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

### Emote list, and which emotes the player has (added 2026-11-21)

Start with a built-in list (no API key needed); the "which emotes the player has" part can come
later. Investigate pulling the list of emotes (from the wiki, or from the game/API directly), so
a line can pick an emote instead of typing `/wave` by hand. Also find out whether we can see
which emotes the player has: many have to be earned or unlocked. Then decide what to do about the
ones they don't have.

To look into (none of this is checked yet):
- **Where the list could come from:**
  - The GW2 API's `/v2/emotes` (no key needed, as far as I know) may list each emote's commands
    and the items that unlock it. It could be cached like `maps.json`.
  - The wiki's emote page as a fallback. Scraping a wiki page is fragile, so it's only for
    whatever the API lacks.
  - A built-in list, updated with addon releases.
  - Whether the game client itself exposes the list. Probably not, since MumbleLink has no emote
    data.
- **Emotes in the player's language:** the commands differ per language (`/wave`, `/winken` ...).
  Check whether the API gives every language's command.
- **Which emotes the player has:** the GW2 API's `/v2/account/emotes` may list the unlocked
  ones. It needs the player's API key with the `unlocks` permission. Nexus has no shared API key
  (as of API v6), so the addon would need a key field of its own, or
  another addon's shared key if one exists. The key must be stored safely, and never sent
  anywhere but the official API.
  - Without a key: the list is shown as "unknown" with no warnings, or only the default emotes
    are marked as known.
- **What to do about emotes they don't have (to decide):**
  - Show them greyed out with "Not unlocked" (and how to get it, from the API's unlock items).
  - Or hide them, or allow them with a warning. In game, a locked emote shows an error message
    in chat and does nothing.
  - Warn in the editor when a macro line uses a locked emote, the same way as the line-length
    warning.
  - Refresh the unlocked list now and then (unlocks change), like "Refresh map info".
- **UI idea:** an "Emote..." picker next to the line's message box (search box + list), which
  fills in `/command`. It could share the pop-up idea of the special character picker.

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

## 4. Later, or when someone asks

### More text sizes from a font of our own (added 2026-11-21)

Build together with "Fonts for other scripts" (next). The Text size dropdown (built, see
`docs/design.md`, "Text size") offers only the fonts Nexus provides, so there are usually just
2-4 sizes. If that's not enough:

- Add a font of our own with `Fonts_AddFromFile`/`FromMemory` and change its size with
  `Fonts_Resize` (it arrives through a callback; never keep the `ImFont*`), offering for example
  12-28 px, or a slider.
- Whether to follow the game's own interface size (MumbleLink's identity `uisz`) as the default.

### Fonts for other scripts (added 2026-11-21)

Nexus' fonts cover Latin with accents. Chinese, Korean, Russian, etc. would show `?` boxes. Load a
font of our own with those glyph ranges (ties in with "More text sizes from a font of our own").

### Signed language files (added 2026-11-21)

Security signatures on translation files where the checksum is checked to make sure the file
comes from our repository and not some random 3rd party (where the file is rejected if it
doesn't pass the test).

- Points to settle: a real signature (a private key kept by the maintainer, the public key in
  the DLL) versus a list of checksums built into each release (which only covers files that
  existed at release time); what happens to a translator's own file while they test it (an
  "unsigned, use anyway" switch?); and how the release workflow signs the files.

### More mouse buttons as hotkeys (added 2026-11-20)

Mouse 3/4/5 now need Ctrl, Alt or Shift (built 2026-11-21, see `docs/design.md`, "Hotkeys").
Still to look at:

- Whether Ctrl/Alt/Shift + left or right click could be allowed safely. In GW2, Shift+click and
  Ctrl+click already do things (chat links, targeting, ground-target skills), and clicks over
  addon windows go to ImGui, so this needs careful in-game testing first.
- Extra mouse buttons beyond 5 (gaming mice usually send them as keyboard keys, which already
  work).
- Depends on in-game check F2: whether mouse buttons reach our hook at all while the pointer is
  over the addon window.

### Windows file picker for import and export (added 2026-11-20)

Phase 7 uses the `exports\` and `imports\` folders, a typed path and the clipboard, because a
modal Windows dialog over the game could grab input or hide behind a fullscreen game. Look at
whether a non-modal file picker (on its own thread, no owner window) works well in game.

## 5. New ideas to discuss (suggested 2026-11-23)

### A random pick inside the macro: `{pick a|b|c}`

Sends one of the choices at random, e.g. `/s {pick hi|hello|o/}`. It brings back the most-used
part of `{txt}` (a random line) with no file reading: every choice is written in the macro.

- Points to settle: the separator (`|`), how to write a literal `|`, whether empty choices are
  allowed, and a limit on the number of choices. The chosen text is a value, so the SEC-013
  mask applies (it can't start a command), and the length check uses the longest choice.

### Character placeholders: `{name}`, `{profession}`, `{spec}`

From MumbleLink's identity (already read for the map), e.g. "LF2M, {spec} here".

- Points to settle: profession and specialization names in the user's language (the GW2 API
  gives them; ids alone come from MumbleLink), and what to send when the identity isn't there
  yet (an error, like an unknown map).

### Filters by character or profession

"Only on these characters" and/or "only on these professions", next to the game mode and map
filters, from MumbleLink's identity.

- Points to settle: matching by character name (a rename breaks it) and showing names that
  aren't logged in; whether it applies to hotkeys, the quick menu and the library dimming like
  the other filters.

### Preview what would be sent

A button in the editor that shows every line exactly as it would be typed (placeholders filled
in, the channel command added), without sending anything. Errors show the same way as on send.

### Per-macro cooldown or double-send guard

A minimum time between two sends of the same macro (and maybe a global one), so a held,
bouncing or repeated hotkey can't spam chat or run into the game's chat flood limit.

- Points to settle: default value (off, or a second or two), whether the quick menu and Send now
  obey it too, and what the user sees when a press is ignored.

### Favourites and "recently sent" in the quick menu

Pinned macros and the last few sent ones at the top of the quick menu (and the pop-up), so the
common ones don't need digging through folders.

### Copy to clipboard as a fallback

When a send is refused (game not focused, a text box already open...), offer "Copy the text"
so the user can paste it themselves. The sender itself still never uses the clipboard; this is
only a user action, and only for one line at a time (or all lines joined).

### A starter library on first run

When there's no `macros.json` yet, create an "Examples" folder showing lines, a whisper,
placeholders and filters. Helps new users and makes good screenshots. Points to settle: whether
it's in every language, and a way to get it back later (Import from a built-in file).

### Daily rolling backups of `macros.json`

Keep the last 7 days (one copy per day) in `<addon>\backups\`, on top of today's single `.bak`
(which only covers the last save). Points to settle: how many to keep, and a "Restore a backup"
choice in the Import window.

### Timed lines ("Pull in 5... 4... 3...")

A delay per line (e.g. one second between lines) for countdowns. Raid leaders would like it, but
it's the idea most likely to conflict with ArenaNet's macro policy: decide only after the
policy check (section 1).
