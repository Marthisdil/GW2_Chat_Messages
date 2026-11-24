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
| [SEC-007](#sec-007) | 2026-11-22 00:00 (approx.) | Info | `src/Search.h:27` (`Search::Result`) | The search result's implicit move could throw (MSVC's `unordered_set` move allocates) | Fixed | PR #30, commit `afab95b`, build `afab95b` |
| [SEC-008](#sec-008) | 2026-11-22 01:59 | High | `src/MacroTree.cpp:700` (`Tree::FromJson`) | A deeply nested trash item was deep-copied recursively: stack overflow, the game closes | Fixed | PR #33 (merged via #34), commit `5b97f25`, build `1dcd3c0` |
| [SEC-009](#sec-009) | 2026-11-22 01:59 | High | `src/MacroTree.cpp:258` (`MakeNamesUnique`) | Renaming repeated names took cubic time, and never ended for names near 100 bytes: the game freezes | Fixed | PR #33 (merged via #34), commit `dc2cba4`, build `1dcd3c0` |
| [SEC-010](#sec-010) | 2026-11-22 01:59 | High | `src/MacroTree.cpp:185` (`ParseChildren`) | A wrongly typed `"expanded"` threw `type_error` out of the render callback | Fixed | PR #33 (merged via #34), commit `f8f58b1`, build `1dcd3c0` |
| [SEC-011](#sec-011) | 2026-11-22 01:59 | High | `src/MacroTree.cpp:662` (`Tree::FromJson`) | A wrongly typed `"format"` threw `type_error` out of the render callback | Fixed | PR #33 (merged via #34), commit `36e562c`, build `1dcd3c0` |
| [SEC-012](#sec-012) | 2026-11-22 01:59 | Medium | `src/Placeholders.cpp:181` (`{json}`) | An imported `{json}` can fetch this PC's or the LAN's addresses and type the answer into chat | Fixed | PR #37, commits `9f7d407`, `07fcc1a`, build `1c64321` |
| [SEC-013](#sec-013) | 2026-11-22 01:59 | Medium | `src/Sender.cpp:149` (`TypeUtf8`) | Placeholder values can add control characters or start a line with a `/` command | Fixed | PR #36, commit `727675a`, build `15aba71` |
| [SEC-014](#sec-014) | 2026-11-22 01:59 | High | `src/Exchange.cpp:445` (`ExportJsonText`) | Non-UTF-8 text from a `.txt` made every save fail and a JSON export close the game | Fixed | PR #33 (merged via #34), commit `8ea6ce0`, build `1dcd3c0` |
| [SEC-015](#sec-015) | 2026-11-22 01:59 | Medium | `src/Sender.cpp:149` (`TypeUtf8`) | Control characters in imported lines were typed into chat as `WM_CHAR` | Fixed | PR #33 (merged via #34), commit `d7435e2`, build `1dcd3c0` |
| [SEC-016](#sec-016) | 2026-11-22 01:59 | Medium | `src/Exchange.cpp:350` (`CopyMacro`) | Imported macros kept their hotkeys by default | Fixed | PR #33 (merged via #34), commit `11e0ab7`, build `1dcd3c0` |
| [SEC-017](#sec-017) | 2026-11-22 (approx.) | High | `src/MacroTree.cpp` (`Tree::FromJson`, `NewId`) | Giving ids to many id-less imported nodes takes quadratic time: the game freezes | Fixed | PR #35, commit `644f9ed`, build `f055144` |
| [SEC-018](#sec-018) | 2026-11-22 (approx.) | High | `src/Exchange.cpp:179`, `src/MacroTree.cpp:414` (`FreeName`) | Rename (import) and trash Restore loop forever on a taken name of 99-100 bytes | Fixed | PR #35, commit `644f9ed`, build `f055144` |
| [SEC-019](#sec-019) | 2026-11-22 (approx.) | Low | `src/MacroTree.cpp` (`MakeNamesUnique`, after SEC-009) | Names crafted to share a hash bucket still cost about 13 s | Fixed | PR #38, commit `5d1d31f`, build `b76b291` |
| [SEC-020](#sec-020) | 2026-11-22 (approx.) | Low | `src/Exchange.cpp:65` (`Utf8Name`) | `path::u8string()` throws on a file name with an unpaired surrogate, outside any `try` | Fixed | PR #38, commit `7bc9411`, build `b76b291` |
| [SEC-021](#sec-021) | 2026-11-22 (approx.) | Low | `src/MapNames.cpp:51` (`LoadFile`) | `json::value()` on `maps.json` throws on a wrongly typed value | Fixed | PR #38, commit `92381e8`, build `b76b291` |
| [SEC-022](#sec-022) | 2026-11-22 (approx.) | Low | `src/Ui.cpp:2471` (import preview) | Every import warning is drawn each frame; a crafted file can have about 2 million | Fixed | PR #38, commit `82e86e0`, build `b76b291` |
| [SEC-023](#sec-023) | 2026-11-22 (approx.) | Low | `src/Ui.cpp:1636` (link files, after SEC-014) | A repaired non-UTF-8 link file can grow past the 256 KB limit and stop syncing | Fixed | PR #38, commit `973b231`, build `b76b291` |
| [SEC-024](#sec-024) | 2026-11-22 (approx.) | Low | `src/Storage.cpp:98`, `LinkSync.cpp:47`, `Ui.cpp:2401` | A typed name or path with a lone surrogate makes `fs::u8path` throw in the render callback | Fixed | PR #38, commit `7bc9411`, build `b76b291` |
| [SEC-025](#sec-025) | 2026-11-22 (approx.) | Low | `src/Exchange.cpp` (folder import) | A crafted folder import can hold about 1.5 GB; an out-of-memory error there isn't caught | Fixed | PR #38, commit `7afa630`, build `b76b291` |
| [SEC-026](#sec-026) | 2026-11-22 (approx.) | Info | `src/Ui.cpp:1404` (`InputString`) | A C1 control character pasted into the editor stays in the line until the next load | Fixed | PR #38, commit `4716bfe`, build `b76b291` |
| [SEC-027](#sec-027) | 2026-11-22 22:05 | High | `src/Exchange.cpp` (`ApplyItems`), `src/MacroTree.cpp` (`CheckName`) | Importing many macros into one folder, and its preview on a re-import, took quadratic time: the game freezes | Fixed | PR #35, commit `1247a73`, build `f055144` |
| [SEC-028](#sec-028) | 2026-11-23 01:14 | High | `src/Placeholders.cpp` (`Uses`, `HasPlaceholders`, `Expand`) | Finding placeholder names took quadratic time on a line full of `{`, every frame in the import preview and the editor: the game freezes | Fixed | PR #39, commit `83be7f6`, build `aa37a53`; `Expand`: PR #40, commit `f225118`, build `ce420d7` |
| [SEC-029](#sec-029) | 2026-11-23 05:00 | Medium | `src/MapNames.cpp` (`ThreadMain`) | The map-names worker thread had no `try`: an exception there (out of memory, a bad answer) would end the game | Fixed | PR #45, commit `0bdcc55`, build `c52820b` |
| [SEC-030](#sec-030) | 2026-11-23 04:59 | Info | `src/Exchange.h` (`Package`), `src/MacroTree.h` (`Tree`), `src/MapCatalog.h` (`Cache`) | Moving these can throw (MSVC's `std::set` allocates when moved), only when out of memory | Accepted | - |
| [SEC-031](#sec-031) | 2026-11-23 04:59 | Info | `src/GameKeys.h` (`Bind::Use`) | Field without a default value | Fixed | PR #45, commit `0bdcc55`, build `c52820b` |
| [SEC-032](#sec-032) | 2026-11-23 04:59 | Info | `src/Ui.cpp` (`DrawMapPicker`) | `size_t` to `float` without a cast | Fixed | PR #45, commit `0bdcc55`, build `c52820b` |
| [SEC-033](#sec-033) | 2026-11-24 03:34 | High | `src/MacroTree.cpp` (`ParseFilter`, `ReachOf`) | A file (an import, a shared library) could give a folder a huge map list; with folder filters, every item inside it sorted that list every frame: the game freezes | Fixed | PR #63, commit `5cf4246`, build `3070952` |
| [SEC-034](#sec-034) | 2026-11-24 05:01 | High | `src/MacroTree.cpp:289` (`ParseMacro`) | A JSON import (or a shared library) skipped the 50-line limit: one macro can hold about 1.4 million lines, and selecting it freezes the game | Fixed | PR #67, commit `7389e5d`, build `7d9876e` |
| [SEC-035](#sec-035) | 2026-11-24 12:58 | High | `src/SendPlan.cpp:110` (`Build`), `src/Sender.cpp:146` (`TypeUtf8`) | A whisper recipient from a file has no length limit: sending it types every character on the game's window thread (the game freezes) and leaves chat open | Fixed | PR #70, commit `7f3167e`, build `e1d33cf` |
| [SEC-036](#sec-036) | 2026-11-24 22:12 | Low | `src/MapNames.cpp:291` (`Shutdown`), `src/Sender.cpp:406` (`Shutdown`) | The stop flag is set and the worker woken without its mutex: a wakeup lost at exactly the wrong moment would hang unloading the addon (the game freezes) | Fixed | PR #73, commit `134714d`, build `b4815cd` |

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
- **Status:** Fixed in **PR #30** (`feature/search-import-mouse`), commit `afab95b`, first build
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
- **Decision (2026-11-22, the owner):** fix it proxy-aware. `Http::GetPublic` (for `{json}`) uses
  the draft's address rules (`src/NetAddress.*`) and pins WinHTTP's default proxy setting for the
  request:
  - **no proxy:** the draft's check, on the address of the connection actually used, before every
    request (new, reused keep-alive and redirect hops);
  - **a proxy:** that address is the proxy's, so the target's name is looked up first
    (`GetAddrInfoW`) and every redirect's (`WINHTTP_CALLBACK_STATUS_REDIRECT`); any local address
    refuses it, and so does a name that can't be looked up (owner: fail closed). Only plain host
    names (letters, digits, `-`, `.`) and IP literals are sent, so the proxy can't read the name
    differently from our lookup. The real connection is still checked before every request: a
    local address passes only if it is one of the proxy's own (its names looked up when the
    request starts), so a host WinHTTP reaches directly (the bypass list) gets the no-proxy check.
  - **Accepted limit (owner):** behind a proxy, a DNS server that answers us with a public
    address and the proxy with a local one (DNS rebinding), or split-horizon DNS on the proxy's
    side. It needs the attacker's own DNS server and a proxy that can reach the LAN.
  Quick audit of the PR (2026-11-22, `/security-review`): no findings; its notes 1 (check the real
  connection behind a proxy too) and 2 (odd host names) are closed as above, note 3 (split-horizon
  DNS) falls under the accepted limit.
  Checked without a proxy: the GW2 API works; `127.0.0.1`, `localhost`, `[::1]`, `192.168.1.1`,
  `169.254.169.254` and `localtest.me` (a public name for 127.0.0.1) are refused. Through a real
  local proxy (`127.0.0.1:8888`, a test build that pins it): the GW2 API works through the proxy;
  the same local targets are refused at the lookup and never reach the proxy; `odd_name.example.com`
  is refused for its name. With a dead proxy: "couldn't connect". Not checked: a redirect through
  a proxy, and a bypass-list host (in-game check I5).
- **Status:** Fixed in PR #37, commits `9f7d407` and `07fcc1a`; first build `1c64321`.
- **Later (2026-11-23, owner):** `{json}` was removed altogether (branch `feature/remove-txt-json`,
  commit `b606a93`), and with it `Http::GetPublic` and `src/NetAddress.*`: no macro can make the
  addon fetch an address now. A line that still has `{json}` is refused. The finding stays Fixed.

### SEC-013

- **Found:** 2026-11-22 01:59 UTC, claude-security full scan (F6). **Severity:** Medium.
  **Where:** `src/Sender.cpp:149` (`TypeUtf8`), `src/Placeholders.cpp`, `src/SendPlan.cpp`.
- **Finding:** a `{json}` value (or a `{txt}` line) can hold control characters such as Backspace
  or Escape, or start with `/`, so it can pick a channel or a chat command the macro didn't name.
- **Decision:** open, needs the owner. The second patch attempt was still rejected: after a
  channel command the user wrote (`/g {json ...}`), a value `/w Name` still picks another channel.
  Its draft is `CLAUDE-SECURITY-20261122-015931/F6-declined-draft.diff`. SEC-015 already cleans
  control characters in stored lines.
- **Decision (2026-11-22, the owner):** finish the fix. Built on the draft, with the gap closed:
  `Placeholders::Expand` marks which bytes came from a value (`Result::FromValue`), and
  `SendPlan::Build` walks each box we type into the way chat reads commands. Every `/` at the
  start (after any kind of space) or right after a command word and its spaces must be one the
  user wrote, so `/g /w Name` from `/g {json}` is refused while `/g {time}` and the user's own
  `/g /wave` are not. Control characters in every value become spaces; typed text with one is
  refused. Tests: `TestControlCharacters`, `TestFromValue` (placeholders), `TestControlCharacters`,
  `TestPlaceholderCommand` (send plan); in-game check I8.
- **Status:** Fixed in PR #36, commit `727675a`; first build `15aba71`.
- **Later (2026-11-23, owner):** `{txt}` and `{json}` were removed (branch
  `feature/remove-txt-json`, commit `b606a93`). The guard stays: `{map}` and `{today}` still bring
  outside text into a line.

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
  folder was also slow for another reason, fixed in the same PR: see SEC-027.
- **Status:** Fixed in PR #35, commit `644f9ed`; first build `f055144`.

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
- **Status:** Fixed in PR #35, commit `644f9ed`; first build `f055144`.

### SEC-019

- **Found:** 2026-11-22 (approx.), the SEC-009 patch verifier. **Severity:** Low.
- **Finding:** with SEC-009's hash sets, 32,768 names crafted to share one bucket (3.7 MB) still
  take about 13 s (the old code: about 217 s). `std::set` would bound it at N log N.
- **Decision (2026-11-22, the owner):** fix them in a batch (PR 3), one commit per finding. Ordered `std::set`/`std::map` for the names and ids read from files: `MakeNamesUnique`, the parser's `SeenIds` and the tree's `m_UsedIds`, and the import's `Dest` and `NameIndex` maps (from SEC-027), so crafted hash collisions can't slow them. Language-file keys must already be known English texts, so those sets stay hashed.
- **Status:** Fixed in PR #38, commit `5d1d31f`; first build `b76b291`.

### SEC-020

- **Found:** 2026-11-22 (approx.), the SEC-011 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Exchange.cpp:65` (`Utf8Name`) and its callers (folder reader, `FromPath`,
  `FromLibraryFile`, `Ui.cpp:2212/2349`).
- **Finding:** `path::u8string()` throws `std::system_error` on a file name with an unpaired
  surrogate, outside any `try`. A file `a\xD800.json` in `imports\` would throw every frame.
  Getting such a name onto the disk is not verified.
- **Decision (2026-11-22, the owner):** fix them in a batch (PR 3), one commit per finding. New `src/PathText.*` converts paths to and from UTF-8 without throwing (a bad sequence becomes U+FFFD); every `u8string()` (also in log lines inside `catch` blocks) uses it. Tested by `tests/PathTextTests.cpp`.
- **Status:** Fixed in PR #38, commit `7bc9411`; first build `b76b291`.

### SEC-021

- **Found:** 2026-11-22 (approx.), the SEC-011 adversarial reviewer. **Severity:** Low.
  **Where:** `src/MapNames.cpp:51`, `LoadFile`.
- **Finding:** `j.value("format", 0)` and `j.value("lang", "")` throw on a wrongly typed
  `maps.json`, outside any `try`. Local file only.
- **Decision (2026-11-22, the owner):** fix them in a batch (PR 3), one commit per finding. `format` and `lang` are read with `find` plus a type check; a wrongly typed cache is thrown away and rebuilt. No other `json::value()` call is left.
- **Status:** Fixed in PR #38, commit `92381e8`; first build `b76b291`.

### SEC-022

- **Found:** 2026-11-22 (approx.), the SEC-009 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Ui.cpp:2471`, the import preview's warnings.
- **Finding:** every warning is drawn each frame; a crafted 4 MB file can produce about 2 million
  (for example non-object entries). Older than the audited changes.
- **Decision (2026-11-22, the owner):** fix them in a batch (PR 3), one commit per finding. At most `kMaxWarnings` (200) are kept: the tree parser counts the rest while reading (so memory stays small too), and the folder reader is cut the same way; the last line says how many more there were. Test `TestWarningsCapped`.
- **Status:** Fixed in PR #38, commit `82e86e0`; first build `b76b291`.

### SEC-023

- **Found:** 2026-11-22 (approx.), the SEC-014 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Ui.cpp:1636`, `LinkSync`.
- **Finding:** with SEC-014, a 100 KB non-UTF-8 link file is repaired to about 300 KB. After an
  edit it's written back that size, past the 256 KB read limit, so the link stops syncing; a
  `.txt` export of it can't be imported again.
- **Decision (2026-11-22, the owner):** fix them in a batch (PR 3), one commit per finding. A link file over 256 KB once repaired is refused (the editor says why), a macro whose file text would be over 256 KB isn't written to its link (a warning, tried again after the next edit), and a `.txt` import over its limit once repaired is left out. Tests in `LinkSyncTests` and `ExchangeTests`. A typed macro over 256 KB can still be exported as `.txt` and then not imported; accepted, as no chat macro needs that much.
- **Status:** Fixed in PR #38, commit `973b231`; first build `b76b291`.

### SEC-024

- **Found:** 2026-11-22 (approx.), the SEC-014 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Storage.cpp:98` (`FreeFile`, from `ExportToFile`), `src/LinkSync.cpp:47`,
  `src/Ui.cpp:2401`.
- **Finding:** ImGui 1.80 passes a lone surrogate `WM_CHAR` into a text box, and `fs::u8path`
  then throws in the render callback. Needs a crafted keypress or a faulty input method.
- **Decision (2026-11-22, the owner):** fix them in a batch (PR 3), one commit per finding. Fixed with SEC-020: every `fs::u8path` uses `PathText::FromUtf8` (an encoded surrogate from ImGui becomes U+FFFD).
- **Status:** Fixed in PR #38, commit `7bc9411`; first build `b76b291`.

### SEC-025

- **Found:** 2026-11-22 (approx.), the SEC-014 adversarial reviewer. **Severity:** Low.
  **Where:** `src/Exchange.cpp`, folder import.
- **Finding:** 2,000 files of 256 KB can hold about 1.5 GB after repair; a `bad_alloc` in
  `FromPath` or `Apply` on the render thread isn't caught. Needs a crafted 512 MB folder.
- **Decision (2026-11-22, the owner):** fix them in a batch (PR 3), one commit per finding. A folder import reads at most `kMaxFolderBytes` (16 MB) of text, `FromPath` turns anything thrown into the package's error, and if `Apply` throws, the library is put back from the Undo import copy and the error window says nothing was imported. With SEC-023 each file stays within 256 KB after repair.
- **Status:** Fixed in PR #38, commit `7afa630`; first build `b76b291`.

### SEC-026

- **Found:** 2026-11-22 (approx.), the SEC-015 adversarial reviewer. **Severity:** Info.
  **Where:** `src/Ui.cpp:1404`, `InputString`.
- **Finding:** the editor maps only `\n`, `\r` and `\t`, and ImGui doesn't filter C1 characters,
  so a pasted U+0085 stays in the line (and is typed) until the library is next loaded. Needs the
  user's own paste. Fix: use `ChatLine::ControlsToSpaces` there.
- **Decision (2026-11-22, the owner):** fix them in a batch (PR 3), one commit per finding. `InputString` uses `ChatLine::ControlsToSpaces`.
- **Status:** Fixed in PR #38, commit `4716bfe`; first build `b76b291`.

### SEC-027

- **Found:** 2026-11-22 22:05, while fixing SEC-017 (a timing test of `Exchange::Apply`).
  **Severity:** High. **Where:** `src/Exchange.cpp` (`ApplyItems` → `AddNamed`) →
  `src/MacroTree.cpp` (`Tree::Add` → `CheckName`).
- **Finding:** every imported item is checked against all its new siblings, lower-casing each
  name again, so importing N macros into one folder costs about N². One import is capped at 5,000
  items (`kMaxImportItems`); measured in the unoptimised test build, those 5,000 took 19 s to
  apply (reading and planning took under 0.5 s). Release is faster but still freezes the game
  for a while, and the folder can already hold many more items. SEC-017's fix removed the id
  half of this cost, not the name half.
  The import preview had the same problem: on every clash `BuildPlan` found the existing item by
  scanning all its siblings (`Dest::Find`), and the plan is rebuilt every frame while the Import
  window is open, so re-importing a big file into the same folder froze the game before Import was
  even clicked.
- **Decision (2026-11-22):** fix, in the SEC-017/018 PR. `Apply` keeps a name index per
  destination folder (`NameIndex`) and adds with `Tree::AddKnownFree`, which skips the sibling
  scan; `Dest` keeps the existing items by lower-case name. Test `TestApplyMany`: 5,000 macros
  into one folder, again (all renamed), then with Replace.
- **Status:** Fixed in PR #35, commit `1247a73`; first build `f055144`.

### SEC-028

- **Found:** 2026-11-23 01:14 UTC, the quick audit (`/security-review`) of the import-flags PR
  (a lower-confidence note). **Severity:** High (a game freeze from an imported file, as
  SEC-027). **Where:** `src/Placeholders.cpp` (`Uses`, `HasPlaceholders`, both through
  `NameAt`).
- **Finding:** both called `NameAt` for every `{`, and each call scanned forward to the next `}`.
  A line full of `{` (an imported macro's message can be megabytes) cost about N² per call. The
  new `Uses` runs on every imported macro every frame while the Import window is open (the
  preview's {txt}/{json} marks), and `HasPlaceholders` every frame in the editor once such a
  macro is in the library, so either froze the game. `Expand` has the same loop, but runs on
  the sender's worker thread, once per send.
- **Decision (2026-11-23):** fix in the same PR, since that PR adds `Uses`. One shared pass
  (`AnyName`) finds the same names as `NameAt` (a `{` whose first `}` comes before any other
  `{`). Tests: edge cases matching `Expand`, and 2 MB of `{` under 2 s in the test build.
  `Expand` is left as it is (worker thread, not a freeze); open for the owner to revisit.
- **Decision (2026-11-23, owner):** fix `Expand` too. `NameAt` now carries the first `}` it
  found forward (the `{` it's asked about only moves forward), and `Expand` copies a run of `{`
  in one go (only the last can start a placeholder). Same output; tests add 2 MB of `{` through
  `Expand` and cases for the carried `}`. Timed in an optimized build: 2 MB in about 0.15 s,
  growing in step with the length.
- **Status:** Fixed for `Uses` and `HasPlaceholders` in PR #39, commit `83be7f6`, first build
  `aa37a53`. `Expand` fixed in PR #40, commit `f225118`, first build `ce420d7`.

### SEC-029

- **Found:** 2026-11-23 05:00 UTC, following up SEC-030 in the quick audit of
  `feature/todo-section-2` (`tools/run_audit.ps1 -Changed -Tests` and `/security-review`).
- **Severity:** Medium. **Where:** `src/MapNames.cpp`, `ThreadMain`.
- **Finding:** the map-names worker thread (single lookups, and since PR #45 the full map list:
  about 10 requests, parsing and saving `maps.json`) ran without a `try`. Anything thrown there,
  out of memory or an unexpected library error, would reach `std::terminate` and close the game,
  as SEC-002 did for the sender thread. The thread existed before PR #45, but that PR gives it
  much more to do.
- **Decision (2026-11-23):** fix. Each step (one wait, then one batch or the full list) runs
  inside `try`; a throw is logged, "loading" is cleared and the queued ids may be asked for
  again, and the thread carries on.
- **Status:** Fixed in **PR #45** (`feature/todo-section-2`), commit `0bdcc55`, first build
  `c52820b`. Release: not released yet.

### SEC-030

- **Found:** 2026-11-23 04:59 UTC, quick audit of `feature/todo-section-2` (clang-tidy
  `bugprone-exception-escape`).
- **Severity:** Info. **Where:** the implicit move constructors of `Exchange::Package`,
  `MacroTree::Tree` and `MapCatalog::Cache`.
- **Finding:** each holds a `std::set`, and MSVC's `std::set` allocates a new head node when
  moved, so the move can throw `bad_alloc`. `Package` and `Tree` were the same on `main`; they
  show up now because their headers changed.
- **Decision (2026-11-23):** accept. It only happens when out of memory, like any other
  allocation; none of these moves is in a `noexcept` function or a callback from the game, and
  the worker threads that move a `Cache` catch everything (SEC-029).
- **Status:** Accepted. Since 2026-11-23 each of the three has a
  `// NOLINT(bugprone-exception-escape): SEC-030` (`feature/security-scan-plan`), so CI's clang
  audit stays clean.

### SEC-031

- **Found:** 2026-11-23 04:59 UTC, quick audit of `feature/todo-section-2` (clang-tidy
  `cppcoreguidelines-pro-type-member-init`).
- **Severity:** Info. **Where:** `src/GameKeys.h`, `Bind::Use`.
- **Finding:** the field had no default value. Every entry in the table sets it, so nothing read
  an uninitialized pointer.
- **Decision (2026-11-23):** fix: `= nullptr`.
- **Status:** Fixed in **PR #45** (`feature/todo-section-2`), commit `0bdcc55`, first build
  `c52820b`.

### SEC-032

- **Found:** 2026-11-23 04:59 UTC, quick audit of `feature/todo-section-2` (clang-tidy
  `bugprone-narrowing-conversions`).
- **Severity:** Info. **Where:** `src/Ui.cpp`, `DrawMapPicker` (the results box height).
- **Finding:** a `size_t` (at most 6) added to a `float` without a cast. Harmless.
- **Decision (2026-11-23):** fix with an explicit `static_cast<float>`.
- **Status:** Fixed in **PR #45** (`feature/todo-section-2`), commit `0bdcc55`, first build
  `c52820b`.
- The same audit's `/security-review` of the branch found nothing else: the GW2 API responses
  and `maps.json` (ids checked, names at most 200 bytes and `SafeText`, file read up to 4 MB,
  the request language from a fixed list), map names drawn as `%s` arguments or labels (never as
  a format string), and `TR()` texts shared with worker threads (freed only after every thread
  is joined) were all fine. Not a security issue but fixed too: importing a whole library
  compared an error message's text to choose its own message (`Exchange::FromLibraryFile`); it
  now uses a flag (`Package::NotOurFormat`).

### SEC-033

- **Found:** 2026-11-24 03:34 UTC, quick audit of `feature/folder-filters` (diff review).
- **Severity:** High (can freeze the game). **Where:** `src/MacroTree.cpp`, `ParseFilter` (reading
  `"map_ids"`) and `ReachOf` (combining a node's filter with its folders').
- **Finding:** `"map_ids"` was read from a file without a limit (the editor allows 50, but a file
  could have any number: about 500,000 fit in a 4 MB import). With folder filters, `ReachOf`
  copies and sorts every map list on the way up, for every folder and macro drawn in the library
  tree, every frame (and for every hotkey and quick menu check). One crafted folder in an
  imported file, or a shared `macros.json`, would make every frame sort it once per item inside
  it, and the game would freeze. Before folder filters a macro's own huge list was only scanned,
  not sorted, once per frame.
- **Decision (2026-11-24):** fix it: read at most `kMaxMapIds` (50) ids per folder or macro, the
  same limit as the editor, with a warning naming the item. Tested in `tests/FilterTests.cpp`
  (`TestFolderFilterJson`).
- **Status:** Fixed in **PR #63** (`feature/folder-filters`), commit `5cf4246`, first build
  `3070952`.

### SEC-034

- **Found:** 2026-11-24 05:01 UTC, by claude-security area A (whole, `be4c6e8`, high effort;
  confirmed by all three verifiers).
- **Severity:** High (can freeze the game; claude-security rated it Medium on its own scale).
  **Where:** `src/MacroTree.cpp:289`, `ParseMacro` (reading `"lines"`).
- **Finding:** every other way lines come in stops at `kMaxLines` (50): the `.txt` import
  (`Exchange.cpp`, `AddTextMacro`), link files (`LinkSync.cpp`) and the raw editor. The JSON
  path (`Exchange::FromJsonText` → `Tree::FromJson` → `ParseMacro`) adds every object in
  `"lines"`, and `Exchange::Finish` only counts folders and macros. A 4 MB export can hold one
  macro with about 1.4 million lines (`{}` each); the preview shows "1 new macro". Selecting it
  draws every line every frame (the game freezes), every save writes over 100 MB of JSON on the
  render thread, it stays in the trash after a delete, and sending it would type every non-empty
  line into chat. Same class as SEC-033 (a list from a file without the editor's limit).
- **Suggested fix:** stop after `kMaxLines` lines in `ParseMacro`, with a warning naming the
  macro (counted against `kMaxWarnings`), which covers imports, `macros.json` and backups at
  once; test with a JSON macro of `kMaxLines + 5` lines.
- **Decision (2026-11-24):** fix it as suggested (owner), in `feature/security-lines-cap`.
- **Status:** Fixed in **PR #67** (`feature/security-lines-cap`), commit `7389e5d`, first build
  `7d9876e`. Tested in `tests/MacroTreeTests.cpp` (`TestJsonLineLimit`); in-game check J9.
- The same scan's panel rejected six candidates: a trashed item's `from_ids` list (any length)
  rebuilding that many folders on Restore (3 variants), and a `link_file` path loaded without
  `CheckPath` reaching `ShellExecuteW` or a write (3 variants). Both are real gaps in the code but
  can only be set by the user's own `macros.json`, `.bak` or backups: every import path empties
  the trash and clears link paths (`Exchange::FromJsonText`, `CopyMacro`).

### SEC-035

- **Found:** 2026-11-24 12:58 UTC, by claude-security area B (whole, `cd2868b`, high effort;
  reported twice, as F1 and F2, both confirmed by all three verifiers).
- **Severity:** High (can freeze the game and leave chat open with someone else's file;
  claude-security rated it Low on its own scale). **Where:** `src/SendPlan.cpp:110` and `:177`
  (`SendPlan::Build`), `src/Sender.cpp:146` (`TypeUtf8`).
- **Finding:** the only length check before typing measures `ChatLine::TypedText(l)`, which for
  a whisper is `"/w " + Message`: the recipient (`Line::WhisperTo`) never meets the 199-unit
  limit, and nothing on the way in limits it either (`MacroTree::ParseMacro`,
  `ChatLine::ParseLine`, link files; only the whole file is capped, 4 MB for JSON, 256 KB for
  `.txt`). `Build` queues the whole recipient as one Type step, and `TypeUtf8` sends one
  `WndProc_SendToGameOnly(WM_CHAR)` per UTF-16 unit inside a single call on the game's window
  thread. A shared macro with a recipient of "Bob" plus megabytes of filler imports fine (the
  editor's recipient box shows "Bob"); sending it stalls the game's message loop for as long as
  the typing runs, the worker's 1 s wait runs out, its Escape is dropped as too late, and chat is
  left open with junk in the recipient box. Nothing extra is typed into chat or executed (control
  characters are already spaces, a `/` from a placeholder is refused), and no modifier is held.
  Same class as SEC-033/SEC-034 (a field from a file without the editor's limit).
- **Suggested fix:** a recipient limit (`ChatLine::kMaxRecipientLength`, e.g. 64 UTF-16 units;
  GW2 account and character names are far shorter) checked in `SendPlan::Build` after
  placeholders are filled in, with a `SendPlanTests` case; optionally also cut `whisper_to` to it
  on load in `ParseMacro` and `ChatLine::ParseLine`.
- **Decision (2026-11-24):** fix it (owner), in `feature/security-whisper-cap`, with the limit
  set to the longest GW2 character name (19 characters, per the GW2 wiki) plus 10:
  `ChatLine::kMaxRecipientLength = 29` UTF-16 units. `SendPlan::Build` refuses a longer
  recipient after placeholders are filled in; `MacroTree::ParseMacro` (with a warning naming the
  macro) and `ChatLine::ParseLine` (`.txt` imports, link files, the raw editor) cut it to 29
  with `ChatLine::CutToUtf16`, which never splits a character; the line editor shows a red note.
- **Status:** Fixed in **PR #70** (`feature/security-whisper-cap`), commit `7f3167e`, first
  build `e1d33cf`. Tested in `tests/SendPlanTests.cpp`
  (`TestRecipientTooLong`), `tests/ChatLineTests.cpp` (`TestCutRecipient`) and
  `tests/MacroTreeTests.cpp` (`TestJsonRecipientLimit`); in-game check J9.
- The same scan's panel rejected one candidate (1 of 3): a language file in `localization\`
  changing the Examples folder's lines; no real impact, since the example lines are checked and
  sent like any other line.

### SEC-036

- **Found:** 2026-11-24 22:12 UTC, by claude-security area C (whole, `cac1442`, medium effort).
  The panel rejected both candidates (0 of 3) as **not security issues**, since only the
  addon's own unload reaches them, but every voter agreed the race is real; the owner chose to
  record it and fix it.
- **Severity:** Low (a correctness bug, not reachable from any file or web response, and it
  needs exact timing; the effect would be a hang when the addon unloads or the game closes).
  **Where:** `src/MapNames.cpp:291` (`MapNames::Shutdown`) with the wait in `ThreadStep`
  (`:217-218`); `src/Sender.cpp:406` (`Sender::Shutdown`) with the wait in `WorkerMain`
  (`:347`).
- **Finding:** both `Shutdown`s set their atomic `s_Stop` and call `notify_all` without taking
  the mutex the worker waits on (`s_Mutex`, `s_JobMutex`). The worker checks its predicate under
  that mutex and then blocks; if the store and the notify fall between the check and the block,
  the wakeup is lost. With no refresh due (`MapNames`) or no job queued (`Sender`) the wait has
  no timeout, nothing else notifies after unload has begun, and the `join` in `AddonUnload`
  waits forever. The comment at `src/Sender.cpp:408` ("every wait in the worker times out")
  doesn't hold for this idle wait.
- **Suggested fix:** set `s_Stop` while holding the worker's mutex (a small `lock_guard` scope
  around the store, then `notify_all` after it), in both `Shutdown`s; correct the comment.
- **Decision (2026-11-24):** fix it (owner), in `feature/security-shutdown-wakeup`.
- **Status:** Fixed in **PR #73** (`feature/security-shutdown-wakeup`), commit `134714d`,
  first build `b4815cd`. No unit test (`Sender` and `MapNames` need Nexus); in-game check L2
  (idle unload/load).

## Scan log

Every claude-security scan, for planning the next one ([security-audit.md](security-audit.md),
"The full audit"). Areas: **A** untrusted files, **B** input path, **C** network, **D**
everything else. **Usage** is the session's tokens and the share of the Pro plan's usage window
(`/usage` before and after), and how long it ran. The clang part isn't logged here: CI runs it
on every push.

| Date (UTC) | Area | Kind | Commit | Effort | Usage | Findings |
|---|---|---|---|---|---|---|
| 2026-11-22 01:59 | A-D (before the split) | whole | `028549c` | medium | not recorded | SEC-008 to SEC-026 |
| 2026-11-24 04:33 | A (with `Backups`, `PathText`) | whole | `be4c6e8` | high | $1.22 → $30.12 session cost (about $28.90; 33 agents, about 2.74M subagent tokens); weekly 73% → 85%; the 5-hour window reset during the run, so its share wasn't measured; 27 min (04:33-05:01) | SEC-034 |
| 2026-11-24 12:34 | B (with `Hotkey`) | whole | `cd2868b` | high | $0.44 → $18.05 session cost (about $17.60; 21 agents, about 1.48M subagent tokens); 5-hour window 13% → 70%; weekly 87% → 94%; 23 min (12:34-12:58) | SEC-035 |
| 2026-11-24 22:04 | C | whole | `cac1442` | medium | $0.40 → $6.31 session cost (about $5.90; 13 agents, about 0.56M subagent tokens); 5-hour window 0% → 19%; weekly 0% → 3% (just after the weekly reset); 8 min (22:04-22:12) | none (2 candidates rejected by the panel as not security issues; recorded as SEC-036 at the owner's request) |

The first release needs a whole-repository scan of every area, so the 2026-11-22 scan doesn't
cover it: it ran before `{txt}`/`{json}` were removed and before PR #45.

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
