# Performance: keeping the game's frame rate

The addon draws inside the game's frame: everything `AddonRender` (and the options page, the
Quick Access menu and the WndProc hooks) does is time the game waits for. The goal (owner,
2026-11-27) is that loading the addon costs no frames you can notice, with the library closed
or open, while sending, and with a big library.

## What runs on the render thread

Every frame, `Ui::Render`:

- polls the sender (`Sender::StatusSeq` first: no copy, no lock, unless a send ended), the
  keybind flags and the hotkey queue;
- works out where you are (MumbleLink, no file or network);
- checks results from the background: the last `macros.json` save, backups, the Restore tab's
  reads;
- checks a couple of link files (a round each second; a file is only read when it changed);
- draws the open windows. The tree uses cached row labels and a cached search; the quick menu
  uses a cached "active here" list and a cached search.

It never writes a file. The only file work left on the render thread is reading a link file that
changed, and the actions you click (import, export, restore), which you'd wait for anyway.

## Rules for changes

1. **No disk, network or slow lock in a frame.** Writes go to `Storage::Post` (the writer
   thread). Reads that can wait go there too, with the result picked up on the render thread
   under a small lock (see the backups in `Ui.cpp`). The network is only used on MapNames' own
   thread.
2. **A lock the render thread takes is never held during file or network work.** Copy under the
   lock, then do the slow part (MapNames' `SaveOutsideLock` is the pattern).
3. **No whole-library work every frame.** A loop over the tree, a search, or strings built per
   macro is cached: key it on what it depends on (`s_TreeGen` for the library, the map and mode,
   the language, the text typed) and rebuild it at least every `kCacheRefreshSec` (0.5 s). Any
   change that alters the tree must go through `MarkDirty()` so `s_TreeGen` moves.
4. **Spread bursts.** Work that grows with the library or the number of links runs a few items
   per frame (link files: `kLinksPerFrame`).
5. **Avoid per-frame allocations in hot paths** when it's easy: reuse a vector, return a const
   reference, skip a copy when nothing changed.
6. **Writer jobs only touch what they were given** and `Storage`/`Backups`: never the tree,
   ImGui or another module's state.

## Checking a change

- **The PR body** has a line "Render-thread budget: ..." for every code PR: what the change adds
  to a frame (and why it's cheap or cached), or "no change".
- **Code review** (`/code-review`) looks for file, network or lock work reached from
  `AddonRender`, the options page, the Quick Access menu or a WndProc hook, and for loops over the
  library without a cache.
- **In game:** the checks in `docs/in-game-tests.md` that say "without a stutter" (P9, R8, K7)
  with a big library. A frame recorder for Dev builds, to measure this, is planned next.
