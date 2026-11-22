# Security findings

Every security finding from audits ([security-audit.md](security-audit.md)): when it was found,
what was decided, and where it was fixed. Times are UTC, like the version and the build stamp.
New findings get the next ID and are never removed; a finding that turns out to be wrong is
closed as **Not a bug** with the reason.

**Status:** **Open** (not decided yet), **Fixing** (decided, fix in progress), **Fixed**
(merged; the PR and the first build with the fix are named), **Accepted** (kept on purpose, with
the reason), **Not a bug** (false positive).

**Severity:** **High** (can crash the game, break game input, or read/write outside the addon
folder with someone else's file), **Medium** (wrong behaviour reachable with real input),
**Low** (hardening; hard or impossible to reach), **Info** (code quality, no real risk).

## Summary

| ID | Found (UTC) | Severity | Where | Finding | Status | Fixed in |
|---|---|---|---|---|---|---|
| [SEC-001](#sec-001) | 2026-11-21 20:07 | High | `src/Sender.cpp` (`OnWndProc`, `Type`) | An exception in our call inside the game's window procedure would end the game | Fixed | PR #29, build `29feb1d` |
| [SEC-002](#sec-002) | 2026-11-21 20:22 | High | `src/Sender.cpp` (`WorkerMain`) | No exception handling on the sender thread: any exception during a send would end the game | Fixed | PR #29, build `29feb1d` |
| [SEC-003](#sec-003) | 2026-11-21 20:07 | Info | `src/Sender.cpp:401` | Optional job read without a check | Fixed | PR #29, build `29feb1d` |
| [SEC-004](#sec-004) | 2026-11-21 20:07 | Low | `src/MapNames.cpp:57` | Map ids from the cache parsed with `stoul` inside an empty `catch` | Fixed | PR #29, build `29feb1d` |
| [SEC-005](#sec-005) | 2026-11-21 20:07 | Info | `src/Ui.cpp:1319` | Implicit `uint32_t` to `float` conversions (x3) | Fixed | PR #29, build `29feb1d` |
| [SEC-006](#sec-006) | 2026-11-21 20:13 | Info | `tests/MacroTreeTests.cpp:32` | Test uses a pointer after a failed null check | Fixed | PR #29, build `29feb1d` |
| [SEC-007](#sec-007) | 2026-11-22 00:00 (approx.) | Info | `src/Search.h:27` (`Search::Result`) | The search result's implicit move could throw (MSVC's `unordered_set` move allocates) | Fixed | this PR (`feature/search-import-mouse`), build `afab95b` |
| [SEC-008](#sec-008) | 2026-11-22 01:59 | High | `src/MacroTree.cpp:700` (`Tree::FromJson`) | A deeply nested trash item was deep-copied recursively: stack overflow, the game closes | Fixed | PR #33 (merged via #34), commit `5b97f25`, build `1dcd3c0` |
| [SEC-009](#sec-009) | 2026-11-22 01:59 | High | `src/MacroTree.cpp:258` (`MakeNamesUnique`) | Renaming repeated names took cubic time, and never ended for names near 100 bytes: the game freezes | Fixed | PR #33 (merged via #34), commit `dc2cba4`, build `1dcd3c0` |
| [SEC-010](#sec-010) | 2026-11-22 01:59 | High | `src/MacroTree.cpp:185` (`ParseChildren`) | A wrongly typed `"expanded"` threw `type_error` out of the render callback | Fixed | PR #33 (merged via #34), commit `f8f58b1`, build `1dcd3c0` |
| [SEC-011](#sec-011) | 2026-11-22 01:59 | High | `src/MacroTree.cpp:662` (`Tree::FromJson`) | A wrongly typed `"format"` threw `type_error` out of the render callback | Fixed | PR #33 (merged via #34), commit `36e562c`, build `1dcd3c0` |
| [SEC-012](#sec-012) | 2026-11-22 01:59 | Medium | `src/Placeholders.cpp:181` (`{json}`) | An imported `{json}` can fetch this PC's or the LAN's addresses and type the answer into chat | Open | |
| [SEC-013](#sec-013) | 2026-11-22 01:59 | Medium | `src/Sender.cpp:149` (`TypeUtf8`) | Placeholder values can add control characters or start a line with a `/` command | Open | |
| [SEC-014](#sec-014) | 2026-11-22 01:59 | High | `src/Exchange.cpp:445` (`ExportJsonText`) | Non-UTF-8 text from a `.txt` made every save fail and a JSON export close the game | Fixed | PR #33 (merged via #34), commit `8ea6ce0`, build `1dcd3c0` |
| [SEC-015](#sec-015) | 2026-11-22 01:59 | Medium | `src/Sender.cpp:149` (`TypeUtf8`) | Control characters in imported lines were typed into chat as `WM_CHAR` | Fixed | PR #33 (merged via #34), commit `d7435e2`, build `1dcd3c0` |
| [SEC-016](#sec-016) | 2026-11-22 01:59 | Medium | `src/Exchange.cpp:350` (`CopyMacro`) | Imported macros kept their hotkeys by default | Fixed | PR #33 (merged via #34), commit `11e0ab7`, build `1dcd3c0` |
| [SEC-017](#sec-017) | 2026-11-22 (approx.) | High | `src/MacroTree.cpp` (`Tree::FromJson`, `NewId`) | Giving ids to many id-less imported nodes takes quadratic time: the game freezes | Fixing | `feature/sec-017-018`, commit `644f9ed` |
| [SEC-018](#sec-018) | 2026-11-22 (approx.) | High | `src/Exchange.cpp:179`, `src/MacroTree.cpp:414` (`FreeName`) | Rename (import) and trash Restore loop forever on a taken name of 99-100 bytes | Fixing | `feature/sec-017-018`, commit `644f9ed` |
| [SEC-019](#sec-019) | 2026-11-22 (approx.) | Low | `src/MacroTree.cpp` (`MakeNamesUnique`, after SEC-009) | Names crafted to share a hash bucket still cost about 13 s | Open | |
| [SEC-020](#sec-020) | 2026-11-22 (approx.) | Low | `src/Exchange.cpp:65` (`Utf8Name`) | `path::u8string()` throws on a file name with an unpaired surrogate, outside any `try` | Open | |
| [SEC-021](#sec-021) | 2026-11-22 (approx.) | Low | `src/MapNames.cpp:51` (`LoadFile`) | `json::value()` on `maps.json` throws on a wrongly typed value | Open | |
| [SEC-022](#sec-022) | 2026-11-22 (approx.) | Low | `src/Ui.cpp:2471` (import preview) | Every import warning is drawn each frame; a crafted file can have about 2 million | Open | |
| [SEC-023](#sec-023) | 2026-11-22 (approx.) | Low | `src/Ui.cpp:1636` (link files, after SEC-014) | A repaired non-UTF-8 link file can grow past the 256 KB limit and stop syncing | Open | |
| [SEC-024](#sec-024) | 2026-11-22 (approx.) | Low | `src/Storage.cpp:98`, `LinkSync.cpp:47`, `Ui.cpp:2401` | A typed name or path with a lone surrogate makes `fs::u8path` throw in the render callback | Open | |
| [SEC-025](#sec-025) | 2026-11-22 (approx.) | Low | `src/Exchange.cpp` (folder import) | A crafted folder import can hold about 1.5 GB; an out-of-memory error there isn't caught | Open | |
| [SEC-026](#sec-026) | 2026-11-22 (approx.) | Info | `src/Ui.cpp:1404` (`InputString`) | A C1 control character pasted into the editor stays in the line until the next load | Open | |
| [SEC-027](#sec-027) | 2026-11-22 22:05 | High | `src/Exchange.cpp` (`ApplyItems`), `src/MacroTree.cpp` (`CheckName`) | Importing many macros into one folder takes quadratic time (20,000: 304 s in the test build): the game freezes | Open | |

"Build" is the build stamp's commit (shown in the Nexus log and the DLL's Properties). Until a
release is tagged, it's the PR's commit; the release that first ships a fix is added when it's
made.

## Findings

### SEC-001

- **Found:** 2026-11-21 20:07 UTC, first `tools/run_audit.ps1` run (clang-tidy
  `bugprone-exception-escape`), on `main` @ `ece9dd2` + the audit tooling (PR #28).
- **Severity:** High. **Where:** `src/Sender.cpp`, the window-thread call (`OnWndProc`) and the
  `Type` lambda.
- **Finding:** keys and text are sent from a call that runs inside the game's window procedure.
  If that call threw (for example out of memory while converting the text), the exception would
  go through the game's message loop and end the game.
- **Decision (2026-11-21):** fix. Catch everything around the call and report the step as
  failed, so the worker aborts the macro the usual safe way (Escape only if our chat box is
  still open). The lambda's own text copy happens on the worker thread, covered by SEC-002; it
  keeps a `NOLINT(bugprone-exception-escape)` with that reason.
- **Status:** Fixed in **PR #29** (`feature/audit-fixes`, commit `29feb1d`), first build
  `29feb1d`. Release: not released yet.

### SEC-002

- **Found:** 2026-11-21 20:22 UTC, while fixing SEC-001 (the rerun's remaining warning led to
  `WorkerMain`).
- **Severity:** High. **Where:** `src/Sender.cpp`, `WorkerMain`.
- **Finding:** the sender thread had no `try`. Any exception while preparing or running a send
  (placeholders, a bad value from a `{json}` page, `{map}`, out of memory) would reach
  `std::terminate` and close the game.
- **Decision (2026-11-21):** fix. Catch `std::exception` and everything else around
  `Prepare`/`Run` and treat it as a failed send: `CloseChatIfOpen`, status Failed with "Internal
  error while sending", logged.
- **Status:** Fixed in **PR #29**, commit `29feb1d`, first build `29feb1d`. Release: not
  released yet.

### SEC-003

- **Found:** 2026-11-21 20:07 UTC, first audit run (`bugprone-unchecked-optional-access`).
- **Severity:** Info. **Where:** `src/Sender.cpp:401`, `WorkerMain`.
- **Finding:** `*s_Job` was read without a check. The wait before it only returns when there is
  a job or the addon is stopping, so it couldn't be empty: a false positive in practice.
- **Decision (2026-11-21):** fix anyway (one line, and safer if the wait ever changes):
  `if (!s_Job) continue;`.
- **Status:** Fixed in **PR #29**, commit `29feb1d`, first build `29feb1d`.

### SEC-004

- **Found:** 2026-11-21 20:07 UTC, first audit run (`bugprone-empty-catch`).
- **Severity:** Low. **Where:** `src/MapNames.cpp:57`, reading the `maps.json` cache.
- **Finding:** map ids were parsed with `std::stoul` inside `catch (...) {}`. It worked (a bad
  key was skipped), but it hid errors and accepted partial numbers like `"12abc"`. `maps.json`
  can be damaged or hand-edited.
- **Decision (2026-11-21):** fix. Parse with `std::from_chars`, which can't throw, and accept only
  a whole number that fits in 32 bits.
- **Status:** Fixed in **PR #29**, commit `29feb1d`, first build `29feb1d`.

### SEC-005

- **Found:** 2026-11-21 20:07 UTC, first audit run (`bugprone-narrowing-conversions`, 3 places).
- **Severity:** Info. **Where:** `src/Ui.cpp:1319`, `ChannelColorVec`.
- **Finding:** colour bytes (0 to 255) converted from `uint32_t` to `float` without a cast.
  Harmless: the values always fit.
- **Decision (2026-11-21):** fix with explicit `static_cast<float>`, so the audit stays clean.
- **Status:** Fixed in **PR #29**, commit `29feb1d`, first build `29feb1d`.

### SEC-006

- **Found:** 2026-11-21 20:13 UTC, audit run with `-Tests`
  (`clang-analyzer-core.NonNullParamChecker`).
- **Severity:** Info (test code only; not in the DLL). **Where:**
  `tests/MacroTreeTests.cpp:32`, `TestDepthLimit`.
- **Finding:** `CHECK` doesn't stop the test, so a failed `deepest != nullptr` check would be
  followed by `*deepest`.
- **Decision (2026-11-21):** fix: return after the failed check.
- **Status:** Fixed in **PR #29**, commit `29feb1d`, first build `29feb1d`.

### SEC-007

- **Found:** 2026-11-22 00:00 UTC (approx.), quick audit of `feature/search-import-mouse`
  (`tools/run_audit.ps1 -Changed -Tests`, clang-tidy `bugprone-exception-escape`), on commit
  `821f6cd`.
- **Severity:** Info. **Where:** `src/Search.h:27`, `Search::Result`, moved into `s_Search` every
  frame (`Ui.cpp`, `UpdateSearch`).
- **Finding:** `Search::Run` returned a new `Result` (three `unordered_set`s) that was moved into
  the UI's copy each frame. MSVC's `unordered_set` move can allocate, so the move can throw; only
  when out of memory, like any other allocation on the render thread. No real risk, but it also
  rebuilt the sets every frame.
- **Decision (2026-11-22):** fix. `Search::Run` now fills the caller's `Result` in place (cleared
  first), so nothing is moved and the sets keep their memory.
- **Status:** Fixed in this PR (`feature/search-import-mouse`), commit `afab95b`, first build
  `afab95b`. Release: not released yet.
- The same audit's `/security-review` (the branch diff, with `docs/security-review.md`) found
  nothing: the checked candidates (search range maths, UTF-8 folding at string ends, highlight
  pointers, `WrapInFolder` ownership and the import package's lifetime, link files from an
  imported library, the mouse hotkey rules) were all false positives.

### The 2026-11-22 full audit (SEC-008 to SEC-026)

- **How:** the claude-security full scan (medium effort, scope from
  [security-review.md](security-review.md): `src/`, `tests/`, `tools/`, `.github/workflows/`) on
  `feature/localization` @ `028549c`, started 2026-11-22 01:59 UTC. Report:
  `CLAUDE-SECURITY-20261122-015931/` (git-ignored). Every finding below was confirmed by the
  scan's three-lens panel. The `tools/run_audit.ps1` clang audit on the same commit was clean.
  The scan's ids (F1-F9) are given for each.
- **Patches:** claude-security's suggest-patches job wrote a patch for each finding, checked by
  an independent verifier and an adversarial reviewer; one revision was allowed. F5 and F6 were
  declined (see SEC-012, SEC-013). SEC-017 to SEC-026 were found by those reviewers while
  checking the patches; they aren't in the scan report.
- **Release:** none of these is released yet.

### SEC-008

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F1). **Severity:** High.
  **Where:** `src/MacroTree.cpp:700`, `Tree::FromJson`.
- **Finding:** each trash item was copied with `json::array({*item})`. nlohmann's copy recurses
  once per nesting level, so an import, a whole library or a `macros.json` whose trash item is
  nested about 200,000 levels deep overflowed the stack, and the game closed.
- **Decision (2026-11-22):** fix. The item is read in place through `Parser::ParseNode`.
- **Status:** Fixed in PR #33 (into `feature/localization`, brought to `main` by PR #34), commit `5b97f25`; first build `1dcd3c0`.

### SEC-009

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F2). **Severity:** High.
  **Where:** `src/MacroTree.cpp:258`, `MakeNamesUnique`.
- **Finding:** every repeated name tried `Name (2)`, `Name (3)`... and rescanned all siblings for
  each try, about N³ work: 10,000 same-named macros froze the game. The patch review also found
  that the loop never ended for two copies of a 99- or 100-byte name, because the 100-byte cut
  removes the number again.
- **Decision (2026-11-22):** fix. Hash sets of taken names and a counter per name, with the same
  names as before; a name too long for its number is cut shorter. Output was compared with the old
  code on 20,000 random libraries.
- **Status:** Fixed in PR #33 (into `feature/localization`, brought to `main` by PR #34), commit `dc2cba4`; first build `1dcd3c0`.

### SEC-010

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F3). **Severity:** High.
  **Where:** `src/MacroTree.cpp:185`, `ParseChildren`.
- **Finding:** `jc.value("expanded", true)` throws `type_error` when the key isn't a bool (for
  example `"yes"`). Nothing on the import or paste path catches it, so it left the render
  callback.
- **Decision (2026-11-22):** fix. `find` plus `is_boolean`; a wrongly typed value reads as open,
  like a missing one. A `macros.json` with such a value now loads instead of being set aside.
- **Status:** Fixed in PR #33 (into `feature/localization`, brought to `main` by PR #34), commit `f8f58b1`; first build `1dcd3c0`.

### SEC-011

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F4). **Severity:** High.
  **Where:** `src/MacroTree.cpp:662`, `Tree::FromJson`.
- **Finding:** `aJson.value("format", 0)` throws on `null`, text, a list or an object, the same
  way as SEC-010.
- **Decision (2026-11-22):** fix. Such a file is refused (`FromJson` returns false: an import
  error, or "unreadable" for `macros.json`, as before). Numbers and bools read exactly as before.
  `Exchange::FromJsonText` also catches `std::exception` and shows "It couldn't be read.". A first
  attempt that loaded these files with a warning was rejected for changing more than needed.
- **Status:** Fixed in PR #33 (into `feature/localization`, brought to `main` by PR #34), commit `36e562c`; first build `1dcd3c0`.

### SEC-012

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F5; the panel lowered it to Low).
  **Severity:** Medium. **Where:** `src/Placeholders.cpp:181`, `{json}`, through `Http::Get`.
- **Finding:** the URL of `{json path url}` can come from someone else's import. Only the scheme
  is checked, so it can reach `localhost`, the LAN or link-local services (also through a
  redirect), and one value of the answer is typed into a chat channel the macro's author chose.
- **Decision:** open, needs the owner. Two patches were declined. The first checked the address
  when connecting, but a reused keep-alive connection skipped the check. The second checks the
  connection actually used (`WINHTTP_OPTION_CONNECTION_INFO` at `SENDING_REQUEST`), closing every
  path the reviewer tried, but anyone with a WinHTTP proxy on their PC or LAN would lose `{json}`
  entirely. Its draft is `CLAUDE-SECURITY-20261122-015931/F5-declined-draft.diff`. See also the
  todo "Hide {txt} and {json} behind an advanced setting".
- **Status:** Open.

### SEC-013

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F6). **Severity:** Medium.
  **Where:** `src/Sender.cpp:149` (`TypeUtf8`), `src/Placeholders.cpp`, `src/SendPlan.cpp`.
- **Finding:** a `{json}` value (or a `{txt}` line) can hold control characters such as Backspace
  or Escape, or start with `/`, so it can pick a channel or a chat command the macro didn't name.
- **Decision:** open, needs the owner. The second patch attempt was still rejected: after a
  channel command the user wrote (`/g {json ...}`), a value `/w Name` still picks another channel.
  Its draft is `CLAUDE-SECURITY-20261122-015931/F6-declined-draft.diff`. SEC-015 already cleans
  control characters in stored lines.
- **Status:** Open.

### SEC-014

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F7). **Severity:** High.
  **Where:** `src/Exchange.cpp:445` (`ExportJsonText`), `src/Storage.cpp` (`WriteJson`).
- **Finding:** a `.txt` import, pasted text or a link file that isn't UTF-8 (for example an ANSI
  file with `ä`) stored bytes that nlohmann's strict `dump()` refuses. Every save then failed for
  the rest of the session, and a JSON export threw out of the render callback.
- **Decision (2026-11-22):** fix. Invalid UTF-8 becomes U+FFFD where text comes in
  (`ChatLine::RepairUtf8`, with a warning), both dumps use `error_handler_t::replace`, and exports
  catch exceptions and go through `ShowError`. Checked against Python's decoder on about 2.9
  million inputs.
- **Status:** Fixed in PR #33 (into `feature/localization`, brought to `main` by PR #34), commit `8ea6ce0`; first build `1dcd3c0`.

### SEC-015

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F8; 2 of 3 panel votes).
  **Severity:** Medium. **Where:** `src/Sender.cpp:149` (`TypeUtf8`), fed by `ParseMacro`,
  `ParseLine` and `ParseRaw`.
- **Finding:** CR, Tab, Esc and other control characters in an imported line were stored and then
  typed into chat as `WM_CHAR`, outside the sender's checked steps.
- **Decision (2026-11-22):** fix. `ChatLine::ControlsToSpaces` turns each C0, DEL and C1 character
  into a space where text comes in. Control-free lines parse exactly as before (200,000 inputs
  compared).
- **Status:** Fixed in PR #33 (into `feature/localization`, brought to `main` by PR #34), commit `d7435e2`; first build `1dcd3c0`.

### SEC-016

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F9). **Severity:** Medium.
  **Where:** `src/Exchange.cpp:350` (`CopyMacro`), `src/Exchange.h` (`Options::KeepHotkeys`).
- **Finding:** **Keep hotkeys** was ticked by default and the preview didn't show hotkeys, so a
  shared file could bind its macro to an ordinary key such as `1`.
- **Decision (2026-11-22):** fix. The box starts unticked on every load; when ticked, the preview
  shows each hotkey. Left for later: flagging `{txt}`/`{json}` in the preview, and imported
  hotkeys without Ctrl/Alt/Shift or that clash with one of yours.
- **Status:** Fixed in PR #33 (into `feature/localization`, brought to `main` by PR #34), commit `11e0ab7`; first build `1dcd3c0`.

### SEC-017

- **Found:** 2026-11-22 (approx.), the SEC-009 patch reviewers. **Severity:** High.
  **Where:** `src/MacroTree.cpp`, `Tree::FromJson` → `NewId` → `IdInUse` → `Find`.
- **Finding:** every node without an id gets a new id by walking the whole tree, so N id-less
  nodes cost about N²: 50,000 id-less macros (1.4 MB) took about 40 s with SEC-009 fixed.
- **Decision (2026-11-22):** fix. The tree keeps the set of ids in use (filled from the parser's
  ids on load, or once on the first new id), so each new id is one lookup. 50,000 id-less macros
  now load in well under a second (test `TestManyMissingIds`). Importing many items into one
  folder is still slow for another reason: see SEC-027.
- **Status:** Fixing in `feature/sec-017-018`, commit `644f9ed`.

### SEC-018

- **Found:** 2026-11-22 (approx.), the SEC-009 and SEC-011 patch reviewers. **Severity:** High.
  **Where:** `src/Exchange.cpp:179-186` (`FreeName`), `src/MacroTree.cpp:414-421`
  (`Tree::FreeName`).
- **Finding:** the same `CleanName(name + " (n)")` loop as SEC-009: for a taken name of 99-100
  bytes it never ends. The import one runs every frame in `BuildPlan`, so re-importing your own
  export of a folder with a 100-character macro name into the same place freezes the game. The
  trash one runs on Restore.
- **Decision (2026-11-22):** fix. Both loops (and `AddNamed` in `Apply`, which gave up after 1,000
  tries) now use `Tree::NumberedName`, the shortened-name rule from SEC-009: the name is cut so
  all of " (n)" fits, so every number gives a new name. A 100-byte name becomes 96 bytes + " (2)".
- **Status:** Fixing in `feature/sec-017-018`, commit `644f9ed`.

### SEC-019

- **Found:** 2026-11-22 (approx.), the SEC-009 patch verifier. **Severity:** Low.
- **Finding:** with SEC-009's hash sets, 32,768 names crafted to share one bucket (3.7 MB) still
  take about 13 s (the old code: about 217 s). `std::set` would bound it at N log N.
- **Status:** Open.

### SEC-020

- **Found:** 2026-11-22 (approx.), the SEC-011 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Exchange.cpp:65` (`Utf8Name`) and its callers (folder reader, `FromPath`,
  `FromLibraryFile`, `Ui.cpp:2212/2349`).
- **Finding:** `path::u8string()` throws `std::system_error` on a file name with an unpaired
  surrogate, outside any `try`. A file `a\xD800.json` in `imports\` would throw every frame.
  Getting such a name onto the disk is not verified.
- **Status:** Open.

### SEC-021

- **Found:** 2026-11-22 (approx.), the SEC-011 adversarial reviewer. **Severity:** Low.
  **Where:** `src/MapNames.cpp:51`, `LoadFile`.
- **Finding:** `j.value("format", 0)` and `j.value("lang", "")` throw on a wrongly typed
  `maps.json`, outside any `try`. Local file only.
- **Status:** Open.

### SEC-022

- **Found:** 2026-11-22 (approx.), the SEC-009 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Ui.cpp:2471`, the import preview's warnings.
- **Finding:** every warning is drawn each frame; a crafted 4 MB file can produce about 2 million
  (for example non-object entries). Older than the audited changes.
- **Status:** Open.

### SEC-023

- **Found:** 2026-11-22 (approx.), the SEC-014 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Ui.cpp:1636`, `LinkSync`.
- **Finding:** with SEC-014, a 100 KB non-UTF-8 link file is repaired to about 300 KB. After an
  edit it's written back that size, past the 256 KB read limit, so the link stops syncing; a
  `.txt` export of it can't be imported again.
- **Status:** Open.

### SEC-024

- **Found:** 2026-11-22 (approx.), the SEC-014 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Storage.cpp:98` (`FreeFile`, from `ExportToFile`), `src/LinkSync.cpp:47`,
  `src/Ui.cpp:2401`.
- **Finding:** ImGui 1.80 passes a lone surrogate `WM_CHAR` into a text box, and `fs::u8path`
  then throws in the render callback. Needs a crafted keypress or a faulty input method.
- **Status:** Open.

### SEC-025

- **Found:** 2026-11-22 (approx.), the SEC-014 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Exchange.cpp`, folder import.
- **Finding:** 2,000 files of 256 KB can hold about 1.5 GB after repair; a `bad_alloc` in
  `FromPath` or `Apply` on the render thread isn't caught. Needs a crafted 512 MB folder.
- **Status:** Open.

### SEC-026

- **Found:** 2026-11-22 (approx.), the SEC-015 adversarial reviewer. **Severity:** Info.
  **Where:** `src/Ui.cpp:1404`, `InputString`.
- **Finding:** the editor maps only `\n`, `\r` and `\t`, and ImGui doesn't filter C1 characters,
  so a pasted U+0085 stays in the line (and is typed) until the library is next loaded. Needs the
  user's own paste. Fix: use `ChatLine::ControlsToSpaces` there.
- **Status:** Open.

### SEC-027

- **Found:** 2026-11-22 22:05, while fixing SEC-017 (a timing test of `Exchange::Apply`).
  **Severity:** High. **Where:** `src/Exchange.cpp` (`ApplyItems` → `AddNamed`) →
  `src/MacroTree.cpp` (`Tree::Add` → `CheckName`).
- **Finding:** every imported item is checked against all its new siblings, lower-casing each
  name again, so importing N macros into one folder costs about N². Measured in the unoptimised
  test build: 5,000 macros took 19 s and 20,000 took 304 s (reading and planning stay under 2 s).
  Release is faster but still quadratic, and a 4 MB export can hold about 60,000 macros: clicking
  Import on a shared file freezes the game. SEC-017's fix removed the id half of this cost, not
  the name half.
- **Decision:** open. Likely fix: `Apply` keeps the lower-case names per destination folder (as
  `BuildPlan` already does) and adds with a name it knows is free, without `CheckName`'s scan.
- **Status:** Open.

## Tuning decisions (checks turned off)

Checks turned off in `.clang-tidy` because they only produced noise here. They're kept here so
the decision can be revisited.

| Decided (UTC) | Check | Hits | Reason |
|---|---|---|---|
| 2026-11-21 19:55 (approx.) | `bugprone-easily-swappable-parameters` | many | Flags every function with two string parameters |
| 2026-11-21 19:55 (approx.) | `bugprone-implicit-widening-of-multiplication-result` | 3 | Our size constants (`4 * 1024 * 1024`) |
| 2026-11-21 19:55 (approx.) | `clang-analyzer-optin.core.EnumCastOutOfRange` | 2 | Inside Microsoft's `<filesystem>` header |
| 2026-11-21 20:10 (approx.) | `bugprone-signed-bitwise` | 55 | Win32 and ImGui flags are signed ints combined with `\|` |
| 2026-11-21 20:10 (approx.) | `bugprone-throwing-static-initialization` | 18 | File-level containers and colours; their constructors don't throw in practice |

## Adding a finding

Give it the next `SEC-` number, a row in the summary and an entry below with: when it was found
(UTC) and by what (`run_audit.ps1`, claude-security, `/security-review`, a report), the
severity, where, what's wrong, the decision with its date, and the status. When it's fixed,
name the PR, the commit and the first build; when a release ships it, add the release version.
