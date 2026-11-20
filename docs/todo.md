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

## Open questions

- Whisper sending: find a way that needs no Tab key (test in game before building the sender).
- Check ArenaNet's current macro policy before the first public release, and mention the
  multi-line sending when registering the addon with Raidcore.
