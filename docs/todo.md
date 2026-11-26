# To-do and ideas

A place to collect ideas, brainstorming and possible features before they are designed or built.
Items here are **not decided**. Once an item is agreed, move it into `docs/design.md` (or a PR)
and remove it here.

The sections are in priority order (ranked 2026-11-23, updated the same day after the security
scan plan; sections 1 and 5 re-ranked 2026-11-24; section 2 refilled from section 5 on 2026-11-26): release blockers first, then quick wins,
bigger features, later items, and new ideas still to discuss. Within each section, items are in
priority order too.

## 1. Before a first public release

### A first-run welcome: ask about the examples, then offer a tour (added 2026-11-25, top priority)

The first time the addon opens in game:

1. **Ask before adding the example macros.** Today the Examples folder is added on the first
   run without asking (`src/Examples.*`). Instead, offer it at that point (for example "Add some
   example folders and macros to start with?" with Yes / No). Saying No adds nothing, and the
   options page's "Add the example macros" still works later.
2. **Then offer a short click-through tutorial.** Text boxes, one at a time, each pointing at a
   part of the interface (the tree, the search box, the details pane, New folder / New macro,
   Send now, the quick bubble icon, the options page), with Next / Back and a way to cancel at
   any step. It can stay generic. If we build it, the options page gets a button to run the
   tutorial again.

To discuss: what counts as the "first time" (no `macros.json` yet? a saved "welcome shown"
setting, so an upgrade from an older version doesn't show it?); whether the tour uses the
example macros if they were added; how the text boxes point at things in ImGui 1.80 (arrows or
highlights drawn over our own windows, not the game's); translations for every step; and new
in-game checks and screenshots for both.

**Advanced options (2026-11-25):** the tour follows the box. With it off (the default on a new
install) it never points at anything hidden behind it; at most one step points at the box itself
("more features for experienced players"). Whether the tour has extra steps when it's on, or
the [?] wiki page is enough, is to discuss. The welcome itself isn't an advanced option.

### In-game testing

Work through `docs/in-game-tests.md` (A1 passed, A2 mostly; the rest is still to do). Session B,
whether the game takes the addon's keys and text, matters most: everything else depends on it.

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

### Look at the layout of the macro settings (added 2026-11-25)

Go over the layout of a macro's settings in the library window (the details pane on the right:
name, lines, hotkey, filters, link file, the Send now / Preview buttons and the rest) to see
whether it can be clearer: what goes where, in what order, what's grouped or folded away, and
how it fits at small window sizes and bigger text sizes. Do it before the `Ui.cpp` cleanup
below, so that cleanup works on the final layout.

**For reference** (owner, 2026-11-25): how the Blish HUD module we started from
(bhm-chat-macros) lays out one macro, in
[`docs/reference/bhm-chat-macros-editor.webp`](reference/bhm-chat-macros-editor.webp):
- the macro's name as a wide title bar at the top;
- two boxes side by side: **Active Areas** (a list of maps, an "Add Map..." field, and PvE /
  WvW / PvP tick boxes under it) and **Trigger Options** (voice commands, an "Add Voice
  Command..." field, and the key binding as "Non-Voice Trigger");
- a **Message Sequence** header with small icon buttons on its right (toggle, link a file,
  open the file);
- one row per line: a channel drop-down (`[G2]`), the text in a monospaced box, a red ✕ to
  delete it and a drag handle to reorder;
- a wide **Add Line** button at the bottom.

What stands out: everything about one macro is on one screen, with where it's active and how
it's triggered side by side above the lines, and each line is compact (channel, text, delete,
drag). The text box doesn't wrap, so long lines get cut off at the edge.

**Advanced options (2026-11-25):** design the layout for both states. With the box off (the
default) the pane shows only the everyday settings, with no gaps where hidden sections would
be. With it on, the advanced sections (now the text file link; later the planned ones in
`docs/design.md`, "Advanced options", such as timed lines or a schedule) have a clear place of
their own, e.g. grouped under one "Advanced" heading at the bottom, rather than scattered among
the everyday ones. A macro that uses an advanced feature while the box is off still needs its
note and faded Send now to fit. Screenshots of the new layout: one with the box off, and one
with it on where it differs.

### Cleanup PR for `Ui.cpp` after the in-game testing (added 2026-11-24)

**Reminder (owner):** once every session in `docs/in-game-tests.md` has been run and passed,
make one cleanup PR for `src/Ui.cpp` alone (about 4,200 lines, no unit tests): run `/simplify`
on it, with no feature changes in the same PR. Rules from `CLAUDE.md` ("`/simplify`"): behaviour
stays the same (texts, popup IDs, JSON keys, the `Action` queue), the input path is left out,
then the build, the tests, `/code-review`, and the in-game checks of every screen it touched,
named in the PR. Not before the in-game testing, so a break can't be mixed up with a feature
bug.

## 2. Quick wins

Built so far (in `docs/design.md`): the remaining messages translated ("Languages"), the filters'
map search ("Features"), the default-key warning ("Input safety", hotkeys), and the import and
export folders, ready to open ("Import / export"), Preview next to Send now ("Features"), the
per-macro hotkey cooldown ("Input safety", hotkeys), and the starter library with its
reserved id and "ask first, replace" button ("Starter library"), backups on a schedule with
a Restore tab ("Backups"), favourites and recently sent in the quick menu, and favourites shown
everywhere whatever the filters, folders too, at most 10 ("Features"), all on 2026-11-23. What was left out of them is under section 4.

Re-ranked 2026-11-23 after the starter library: smallest and safest first. "Export a selection"
(owner, 2026-11-24) was built the same day ("Import / export"), and so were filters on folders
("Features", Filters).

Refilled 2026-11-26 (owner): six items moved up from section 5, all small, low risk and useful
to everyone, smallest first.
Own language files outside Advanced options (their own opt-in) and the future compatibility of
language files were built on 2026-11-26 ("Languages", in `docs/design.md`).

### Duplicate a macro

Right-click > **Duplicate** makes "Name (2)" next to it, with the same lines and filters but no
hotkey (a hotkey must stay unique). Quick to build; handy for variants of one callout.

**Advanced option?** No (2026-11-26): an everyday edit, nothing surprising.

### Refresh the map list after a game update (added 2026-11-25)

The map list (`maps.json`) is fetched on the first run, once a week, on a language change, and by
**Refresh map list**. New maps arrive with game updates, so fetch it again soon after the game is
updated. Possible signal: MumbleLink's `Context.BuildID` (the game's build number, in
`vendor/mumble`): remember the last one seen in `maps.json` or `settings.json`, and when it
changes, start a background refresh (the same one as the weekly refresh, on the worker thread).
Points to settle: whether `BuildID` is filled in before the first map load, a small delay so a
refresh doesn't start during the loading screen, and that the GW2 API may list a new map a little
after the patch (maybe refresh again a day later).

**Advanced option?** No: it's automatic and invisible, and helps everyone. With it, the
**Refresh map list** button matters even less.

### Notes on a macro (suggested 2026-11-23)

A free-text note that's never sent (what it's for, when to use it), shown in the editor and in
the quick menu's hover preview.

**Advanced option?** No (2026-11-26): never sent, useful to anyone.

### Split a long line (suggested 2026-11-23)

When a line is over 199 characters, offer to split it into two lines on the same channel at a
word break, instead of only showing it in red.

**Advanced option?** No (2026-11-26): it only fixes a line that can't be sent as it is.

### Keyboard use of the quick menu pop-up (suggested 2026-11-23)

Up/down arrows to move through the search results and Enter to send the highlighted one (today
Enter sends the first). Must never let the keys reach the game.

**Advanced option?** No (2026-11-26): plain menu navigation.

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

**Advanced option?** No (2026-11-26): a way to send, like the quick menu; it's off until the
user turns the window on anyway.

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

**Advanced option?** No (2026-11-26): a picker that fills in a normal chat command.

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

**Advanced option?** No (2026-11-26): it only inserts text.

### Timed lines ("Pull in 5... 4... 3...") (moved from section 5, 2026-11-23, for more discussion)

**Advanced option (owner, 2026-11-25):** when it's built, it goes behind the **Advanced options**
checkbox (`src/Advanced.*`; `docs/design.md`, "Advanced options"): hidden, and not working, while
the box is off.

A delay per line (e.g. one second between lines) for countdowns. Raid leaders would like it, but
it's the idea closest to ArenaNet's macro policy. The policy was read on 2026-11-23 (see the
wiki's ArenaNet's policies page): it's about gameplay (skills, farming, bots, playing while
away) and asks that each macro is a single action you start again. A countdown is still one
press sending chat lines once, but it runs on its own for several seconds, so decide with
care (and say so when registering with Raidcore).

### Scheduled and repeating macros (added 2026-11-24)

**Advanced option (owner, 2026-11-25):** when it's built, it goes behind the **Advanced options**
checkbox (`src/Advanced.*`; `docs/design.md`, "Advanced options"): hidden, and not working, while
the box is off.

- **Scheduled:** the user sets a date and time, and the macro sends itself then (for example a
  guild event reminder at 19:55).
- **Repeating:** the user starts the macro, and it sends again on a timer they set (every N
  minutes), either until they stop it or for X repeats.
- **Must (owner):** before every send, the addon checks that the chat box isn't open and the user
  isn't typing. If it is, the send **waits** until the box is free, and doesn't skip it or
  interrupt. All the existing send rules still apply on top (game focused, in gameplay, one macro
  at a time; see `docs/design.md`, "Input safety").
- Points to settle:
  - **ArenaNet's macro policy:** this is the closest idea to it, closer than timed lines. A
    macro that sends on its own, or again and again, is the "automation" and "while away" the
    policy warns about, and repeated chat can count as spam. Read the policy again (the wiki's
    ArenaNet's policies page), decide limits (for example a minimum interval, a maximum number of
    repeats, stop when the map changes or the player is idle), and say so when registering with
    Raidcore. This may decide whether it's built at all.
  - How long a waiting send may wait (the user typing for minutes), and whether it's dropped or
    still sent after that. Is a late scheduled message still useful?
  - What shows that something is scheduled or repeating: a line in the library, the quick menu,
    an on-screen marker, and a clear **Stop** (and whether a hotkey stops it too).
  - What happens on a map change, a loading screen, a character switch, alt-tab, or the game
    closing. Are schedules kept across restarts?
  - Times: the user's local time or UTC, and one-off dates versus "every day at".
  - Whether it shares the per-line delay with timed lines (above): both are "wait, then send".

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

### Brainstorm: more languages that need no new fonts (added 2026-11-23)

Brainstorm a list of languages we could add easily because Nexus' fonts already cover their
letters (Latin with accents), for example which ones GW2 players are likely to want, and what
each needs (a translator, or AI with a native speaker to check).

### Brainstorm: languages that need other fonts (added 2026-11-23)

Brainstorm a list of languages that would need fonts of their own (for example Chinese, Korean,
Japanese, Russian and other Cyrillic, Greek, Arabic or Hebrew), and for each: where to get a font
with a licence that allows shipping it (Noto, for example), how big it is, how the addon would
store it (embedded as a resource, since the updater ships only the DLL, or downloaded into the
addon folder), and how it's loaded (`Fonts_AddFromMemory`, glyph ranges; see "Fonts for other
scripts" above).

### Signed language files (added 2026-11-21)

Security signatures on translation files where the checksum is checked to make sure the file
comes from our repository and not some random 3rd party (where the file is rejected if it
doesn't pass the test).

- Points to settle: a real signature (a private key kept by the maintainer, the public key in
  the DLL) versus a list of checksums built into each release (which only covers files that
  existed at release time); what happens to a translator's own file while they test it (an
  "unsigned, use anyway" switch?); and how the release workflow signs the files.

### Read the player's real keybinds (left from the default-key warning, 2026-11-23)

**Advanced option (owner, 2026-11-25):** when it's built, it goes behind the **Advanced options**
checkbox (`src/Advanced.*`; `docs/design.md`, "Advanced options"): hidden, and not working, while
the box is off.

The hotkey editor only knows GW2's default keys. Reading the player's own binds: a newer Nexus
API with a getter for its game-bind table (API v6 has none; ask Raidcore?), or the newest saved
profile in `Documents\Guild Wars 2\InputBinds\` (can be out of date). Also: secondary binds,
and mouse buttons in the game's binds.

### The GW2 wiki as a map fallback (left from the map picker, 2026-11-23)

Old or removed maps the API doesn't list. The wiki refused automated requests when this was
tried (HTTP 403), so this needs another way in, or a list built into the addon.

### More mouse buttons as hotkeys (added 2026-11-20)

Mouse 3/4/5 now need Ctrl, Alt or Shift (built 2026-11-21, see `docs/design.md`, "Hotkeys").
The left and right mouse buttons stay refused, with or without Ctrl, Alt or Shift (decided by the
owner 2026-11-23, documented in the README and the wiki's Sending Macros page). Still to look at:

- Extra mouse buttons beyond 5 (gaming mice usually send them as keyboard keys, which already
  work).
- Depends on in-game check F2: whether mouse buttons reach our hook at all while the pointer is
  over the addon window.

## 5. New ideas to discuss (suggested 2026-11-23)

Ranked 2026-11-23: low risk and most use first (re-ranked 2026-11-24: Duplicate first, then
the clipboard fallback before the character placeholders). Preview, cooldown, favourites, the
starter library and backups moved up to section 2. Filters by character or profession were
dropped (not needed), and timed lines moved to section 3 for more discussion (owner,
2026-11-23). Duplicate, the map refresh after a game update, own language files, notes, split a
long line and the quick menu keyboard moved up to section 2 (owner, 2026-11-26).

### A random pick inside the macro: `{pick a|b|c}`

**Advanced option (owner, 2026-11-25):** when it's built, it goes behind the **Advanced options**
checkbox (`src/Advanced.*`; `docs/design.md`, "Advanced options"): hidden, and not working, while
the box is off.

Sends one of the choices at random, e.g. `/s {pick hi|hello|o/}`. It brings back the most-used
part of `{txt}` (a random line) with no file reading: every choice is written in the macro.

- Points to settle: the separator (`|`), how to write a literal `|`, whether empty choices are
  allowed, and a limit on the number of choices. The chosen text is a value, so the SEC-013
  mask applies (it can't start a command), and the length check uses the longest choice.

### Copy to clipboard as a fallback

**Advanced option (owner, 2026-11-25):** when it's built, it goes behind the **Advanced options**
checkbox (`src/Advanced.*`; `docs/design.md`, "Advanced options"): hidden, and not working, while
the box is off.

When a send is refused (game not focused, a text box already open...), offer "Copy the text"
so the user can paste it themselves. The sender itself still never uses the clipboard; this is
only a user action, and only for one line at a time (or all lines joined).

### Character placeholders: `{name}`, `{profession}`, `{spec}`

From MumbleLink's identity (already read for the map), e.g. "LF2M, {spec} here".

- Points to settle: profession and specialization names in the user's language (the GW2 API
  gives them; ids alone come from MumbleLink), and what to send when the identity isn't there
  yet (an error, like an unknown map).

**Advanced option?** No (2026-11-26): like `{map}`, it fills in text from MumbleLink.

### Import macros from Blish HUD's chat macro module (added 2026-11-24)

Could the Import window read the macros someone made in Blish HUD's chat macro module ("chat
shorts"; this addon is modelled on agaertner's `bhm-chat-macros`, see `docs/design.md`), so a
player moving from Blish HUD to Nexus doesn't have to type them all again?

- Points to settle:
  - Which module exactly ("chat_shorts" may be `bhm-chat-macros` or another one), and where and
    how it keeps its macros: the file, its folder under Blish HUD's settings, and its format (a
    database file, JSON...). Read its source before deciding anything; a database format may
    need a reader we don't have, and the file is untrusted input like any import
    (`docs/security-review.md`).
  - How its data maps onto ours: lines and channels (including squad broadcast and whispers),
    its placeholders (`{blish}` is our `{nexus}`; anything we don't have is marked in the
    preview, like `REMOVED PLACEHOLDER`), game mode and map filters, hotkeys (off unless **Keep
    hotkeys**, as for every import), and its folders or lists, if any.
  - Where it appears: a new source in the Import window ("From Blish HUD"), going through the
    usual preview, so nothing changes until **Import selected**; whether the addon looks for the
    file in Blish HUD's usual folder or the user copies it into `imports\`.
  - Whether its export or share format (if it has one) is easier to read than its saved file.

**Advanced option?** No (2026-11-26): players moving from Blish HUD are new users, who start
with the box off.

### More ideas (suggested 2026-11-23, not ranked yet)

- **Ask before sending to a big channel:** a per-macro "ask first" box, so a macro for Map,
  Squad broadcast or Guild shows a small Yes/No before it types anything. Guards against a wrong
  click in the quick menu. Points to settle: whether hotkeys skip it, and where the question
  shows (a Nexus alert can't take a click). Advanced option? No (2026-11-26): it's a safety
  feature.
- **Your own text values:** placeholders you define once on the options page, like
  `{v:guild}` = "[TAG] My Guild" or `{v:raid}` = "Wing 4", used in many macros and changed in one
  place. Text only, never files or addresses (the SEC-013 mask applies, as for `{map}`).
  **Advanced option (owner, 2026-11-25):** behind the **Advanced options** checkbox when built.
- **Send to another channel once:** right-click a macro in the quick menu > "Send to..." with a
  channel list, so a Say macro can go to Party once without editing it. Whisper lines keep their
  recipient. Advanced option? No (2026-11-26).
- **Undo for edits:** Ctrl+Z in the library for the last few changes (rename, move, delete, line
  edits), like Undo import but for everyday edits. Bigger; needs a snapshot or change list.
  Advanced option? No (2026-11-26).
- **Favourites in your own order:** drag favourites into an order of their own in the quick
  menu, instead of library order (the first version uses library order). Advanced option? No
  (2026-11-26).
