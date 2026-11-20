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
| Hotkeys | Each macro can have a hotkey. CTRL, ALT and SHIFT modifiers are allowed, and so are mouse buttons. |
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
- Storage is `<GW2>\addons\Chat Messages\macros.json`, written via a temp file and then renamed.
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
- **Quick menu**: the Quick Access right-click menu and a hotkey-opened pop-up show the tree as
  nested menus, with only the active macros listed.
- **Placeholders**: `{wp}`, `{poi}`, `{map}`, `{area}`, `{time}`, `{hour}`, `{min}`, `{today}`,
  `{random [min] max}`, `{txt file [line]}`, `{json path url}`. bhm's `{blish}` becomes
  `{nexus}` (the addon version). Map, waypoint and point-of-interest data come from the GW2 API.
  It is fetched once per map and cached.
- **`.txt` link file**: optional two-way sync of one macro with a text file in raw format.

## Import / export

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
  the box is still focused) and abort the rest of the macro.
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
- **Hotkeys use Nexus InputBinds.** Macros are registered **with no default key**
  (`"(null)"`), so the addon never takes a key the user has not chosen. Keys and mouse buttons
  are assigned in Nexus's own keybind settings.

## Policy note

Raidcore's guide quotes ArenaNet's macro policy ("one user action may lead to one game action"),
and Raidcore has a "Macro Usage" label. Sending several lines from one key press is how bhm
works and is what the user wants. Before the first public release, check ArenaNet's current
policy and mention the behaviour when registering with Raidcore.

## Implementation phases (one small PR each)

1. Scaffolding: submodules, VS project, CI, an addon that loads and shows an empty window. *(done)*
2. Data model, JSON storage, tree editor (create, rename, delete, drag-drop, depth limit). *(done)*
3. Macro editor: fancy and raw modes, parser and serializer, character count. *(done)*
4. Sender: the safe input pipeline (focus, `WM_CHAR`, verification), test button. *(done; not yet
   tested in game)*
5. Per-macro hotkeys, Quick Access and pop-up nested menus, game-mode and map filters.
6. Placeholders (plus the GW2 API map, waypoint and point-of-interest cache).
7. Import/export with preview, `.txt` import/export and link-file sync.
