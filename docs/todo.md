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

## Open questions

- Whisper sending: find a way that needs no Tab key (test in game before building the sender).
- Check ArenaNet's current macro policy before the first public release, and mention the
  multi-line sending when registering the addon with Raidcore.
