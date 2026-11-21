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
