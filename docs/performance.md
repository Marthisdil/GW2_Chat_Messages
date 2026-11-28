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
   This is also a memory-safety rule: the link round (`s_LinkRound`) keeps `Node*` pointers
   only while `s_TreeGen` is unchanged, so a change that frees nodes without `MarkDirty()`
   would leave it pointing at freed memory.
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
  with a big library, and a recording with the frame recorder (below, session X).

## The frame recorder

A tool for the owner, to measure the addon in game instead of guessing. *(`src/Perf.*`, the
rules, tested by `tests/PerfTests.cpp`; `src/PerfRecorder.*`, the timing and the file;
`tools\perf_report.ps1`, the summary.)*

- **Only in builds made on the owner's PC.** The `.vcxproj` defines `CHATMSG_PERF_RECORDER`
  unless the build runs on GitHub Actions (`GITHUB_ACTIONS`), or `/p:ChatMsgNoPerfRecorder=true`
  is passed (CI passes it too). So no published release has it: not Rel, not Beta, and not the
  Dev pre-releases (owner, 2026-11-28). `tools\check_no_recorder.ps1` looks for its marker text
  in every DLL CI builds, and the Release workflow runs it before publishing. Without the define
  `PerfRecorder.h` is empty inline functions and nothing of it is in the DLL. It also needs a Dev
  version (`Version.h`).
- **Behind Advanced options:** on the options page (General tab, under the Advanced options
  box), only while the box is on. Turning the box off stops a recording.
- **What it measures, every frame while recording:** the game's frame (from one `AddonRender`
  to the next), the addon's time in it (`AddonRender`, the options page and the Quick Access
  menu), and the sections: languages, poll (sender, map, link files, hotkeys), library window,
  quick menu, other windows, saves (saves, backups, map list check), options page. Also the
  slowest pass through our two WndProc hooks each second. Not recording costs one `bool` check
  per scope.
- **The file:** `<addon>\perf\perf_YYYY-MM-DD_HH-MM-SS_UTC.csv`. A `#` line (version, build,
  limit), the column names, then rows:
  - `second`: once a second: frames, game and addon avg/max/99th percentile (ms), frames over
    2 ms in the addon, each section's average, the WndProc maximum (us), and what the player
    was doing (gameplay, map id, library open, quick menu open, sending, macro count);
  - `spike`: a frame where the addon took over 2 ms or the game over 50 ms (at most 20 a
    second), with its sections;
  - `event`: library or quick menu opened or closed, send start and end, map change, gameplay
    on or off, a macros save handed to the writer, a backup, **Mark** (numbered), and the stop.
  The rows are built on the render thread and written by the writer thread (`Storage::Post`,
  `Storage::AppendText`) once a second. A failed write stops the recording.
- **Stops by itself** at the chosen length (5, 10, 30 or 60 minutes), at 50 MB, when the addon
  unloads, or when Advanced options are turned off. The options page shows a live line while it
  runs and the last file's name and how it stopped.
- **The summary:** `tools\perf_report.ps1 -Dir <addon>\perf` (the newest file) or `-File`:
  game and addon averages and maxima, the addon's share of the frame, frames over 2 ms, the
  sections, the same by what the player was doing, the slowest frames with the events around
  them, and the marks.
- Not in the wiki, the changelog or the screenshots: players never get it.
