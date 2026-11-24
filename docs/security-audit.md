# Security audit: how to run it

How to check the code (including code written by Claude) for security bugs with two tools:

- **clang-tidy and clangd** (LLVM, `C:\Program Files\LLVM\bin`): fast and free to run. They
  compile our code the way the real build does and report bug patterns such as use after move,
  dangling pointers, unchecked results and data races. **CI runs them on all of `src/` and
  `tests/` for every push and pull request** (the "Clang audit" step), so this half of the full
  audit is always current.
- **claude-security** (a Claude Code plugin): AI agents that read the code like a reviewer. It
  finds what tools can't, such as a path check that can be bypassed or an imported file that
  can make the addon type something unexpected. Its findings are checked by other agents before
  they're reported, and it can write patch files for them. A full scan uses **a lot of
  tokens**, so it's kept for the cases below. The owner is on the Pro plan, so a full scan is
  split into areas and spread over several sessions (see "The full audit").

What to look at and the rules for fixes are in [security-review.md](security-review.md). Both
tools are pointed at it.

## When to run what

The version is a date (`YYYY.MM.DD.HHMM.Channel`), so "major" and "minor" are defined here:

| Kind | What it is | What happens |
|---|---|---|
| **Major release** | The first release; any **Beta** or **Rel** release; a channel change (Dev to Beta, Beta to Rel); or a Dev release that adds or changes a way untrusted data comes in (imports, link files, the network, placeholders) or the input path (`Sender`, `SendPlan`, `Hotkeys`) | **The full audit is required.** The **first release** and a **channel change** get a **whole-repository** scan (every area); any other major release gets a **changes-only** scan of each area since that area's last logged scan. As soon as a release is mentioned, Claude reads the scan log and proposes a schedule (one area per session); it doesn't ask whether to scan. The release waits until every area is covered (see "The full audit") and the findings are fixed or accepted by the owner. |
| **Minor release** | Any other release (a tagged Dev build) | Claude **asks** before the release: full audit, quick audit since the last release tag, or skip. |
| **Lesser change** | A pull request that changes code (`src/`, `tests/`, `tools/`, the `.vcxproj`, CI); docs-only PRs don't count | Claude **asks** when it opens the PR: quick audit of this PR, or skip. |

## The full audit

### The areas

The claude-security scan is split into four areas, riskiest first, so that one area fits in a
session (and in a Pro plan usage window). Each area has its own effort level:

| Area | Covers | Effort |
|---|---|---|
| **A. Untrusted files** | `Exchange` (imports), `LinkSync`, `Storage`, `MacroTree` (JSON), `Localization`, `Languages`, `Backups` (backup names and files), `PathText` (paths read from disk) | high |
| **B. Input path** | `Sender`, `SendPlan`, `Hotkeys`, `Hotkey`, `Placeholders`, `ChatLine` | high |
| **C. Network** | `Http`, `MapNames`, `MapCatalog` | medium |
| **D. Everything else** | `Ui`, `entry`, the other `src/` modules, `tests/`, `tools/`, `.github/workflows/` | low |

A new module joins the area that matches what it does; add it to this table in the same PR.

### Whole-repository or changes-only

- **Whole-repository** (the first release, a channel change): every area is scanned in full,
  at its effort level.
- **Changes-only** (any other major release): each area whose files changed since its last
  logged scan gets a claude-security scan of just those changes (the range from the logged
  commit to the release commit, on the area's files). An area with no changes is already
  covered. The quick audits on each PR don't replace this.
- An area is **covered** for a release when its last logged scan (whole or changes-only) is on
  a commit at or after the last change to its files, and every finding from it is Fixed or
  Accepted.

### The scan log

Every claude-security scan gets a row in the **Scan log** in
[security-findings.md](security-findings.md):
- the date and the area;
- whole or changes-only (with the range);
- the commit and the effort level;
- **how much it used:** the session's tokens and the share of the Pro plan's usage window, from
  Claude Code's `/usage` before and after the scan, and how long it ran;
- the findings (`SEC-` ids, or "none").

The usage column tells us how big a session each area needs, so the next scan can be planned
around it.

### Scheduling

- When a release is mentioned, Claude lists the areas that aren't covered for it and proposes an
  order (A and B first). Start well before the release date, at the start of a usage window,
  not on release day.
- One area per session. Development goes on between sessions: an area changed after its scan
  only needs a changes-only scan of those changes later.
- If the usage window runs out during a scan, log the area as unfinished and scan it again from
  the start in the next session.

### Steps for one area

1. Check that the clang part is clean: CI's "Clang audit" step on the commit, or locally (after
   one Release build, which writes the build stamp and `build\compile_commands.json`):
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_audit.ps1 -Tests
   ```
   It prints a summary by check name. The full output is in `build\audit\clang-tidy.txt` and
   `build\audit\clangd.txt`, and every finding on one line in `build\audit\findings.txt`. Exit
   code 0 = nothing found, 1 = findings to review, 2 = it couldn't run.
2. Note the usage before the scan (`/usage`).
3. Run the **claude-security** scan of the area: ask Claude, in this project, *"Run the
   claude-security scan of area A (whole) following docs/security-review.md and
   docs/security-audit.md, and write patches for what you find"* (or "area A, changes-only
   since `<commit>`"). Claude starts the `claude-security` agent with the area's files as the
   scope, at the area's effort level. It captures the commit, has research agents look for bugs
   and verifier agents vote on each one, and writes a report and a patch file per confirmed
   finding. It doesn't change your files: patches are separate files you choose to apply.
4. Note the usage after the scan and add the Scan log row.
5. Go through the results together (see "Reading the results"), and record every finding that
   isn't noise in [security-findings.md](security-findings.md): when it was found, the decision
   and, later, the PR and build that fixed it.
6. Fixes go on a `feature/security-<short-name>` branch, one finding per commit, with a test
   where the code has tests, then a PR as usual. Apply a claude-security patch with
   `git apply <file>.patch`, then build and run `tools\run_tests.ps1`.
7. When every area is covered, note the audit in the release notes: the commit each area was
   scanned on and what was fixed or accepted.

## The quick audit (one PR, or the changes since a release)

1. The clang part, on the changed files only (CI also runs it on everything):
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_audit.ps1 -Changed -Tests
   ```
   `-Changed` compares the current branch with `main`; add `-Base <tag>` to compare with the
   last release tag instead. A changed header also audits its `.cpp`.
2. A security review of the diff: Claude Code's `/security-review` command reviews the
   pending changes on the current branch. Mention `docs/security-review.md` so it uses the
   project's scope and rules.
3. Fix on the same branch before merging, or note why a finding is accepted. Either way,
   record it in [security-findings.md](security-findings.md).

## Reading the results

- **Fix it** when it can happen with real input: anything reachable from an imported file, a
  `.txt` file, a typed path, a web response or a hand-edited `macros.json`, and anything that
  could leave a key held down or type into the game when it shouldn't.
- **Accept it** (with a short reason in the PR or the release notes) when it can't happen, for
  example a check that clang-tidy can't see across functions. Don't silence a check in
  `.clang-tidy` for one case; use `// NOLINT(check-name): reason` on that line.
- **Turn a check off** in `.clang-tidy` only when it's noise everywhere, with the reason in the
  comment at the top of that file and a row in the "Tuning decisions" table of
  [security-findings.md](security-findings.md).
- clangd lines with `tweak: ... FAIL` are clangd testing its own refactorings, not problems in
  the code; `run_audit.ps1` leaves them out.
- A claude-security patch is a suggestion: check it against `CLAUDE.md` (input safety rules, no
  new libraries, C++17, nothing in `vendor/`) before applying it.

## clangd while editing

The clangd plugin and editors with clangd use the same `build\compile_commands.json` and
`.clang-tidy`, so the same warnings show up while editing. If clangd reports missing headers,
build Release once (the build stamp header comes from the build) or run
`tools\compile_commands.ps1 -Force`. clangd must be on PATH (`C:\Program Files\LLVM\bin`) for
the plugin to find it.
