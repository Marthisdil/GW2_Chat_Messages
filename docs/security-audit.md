# Security audit: how to run it

How to check the code (including code written by Claude) for security bugs with two tools:

- **clang-tidy and clangd** (LLVM, `C:\Program Files\LLVM\bin`): fast and free to run. They
  compile our code the way the real build does and report bug patterns such as use after move,
  dangling pointers, unchecked results and data races.
- **claude-security** (a Claude Code plugin): AI agents that read the code like a reviewer. It
  finds what tools can't, such as a path check that can be bypassed or an imported file that
  can make the addon type something unexpected. Its findings are checked by other agents before
  they're reported, and it can write patch files for them. A full scan uses **a lot of
  tokens**, so it's kept for the cases below.

What to look at and the rules for fixes are in [security-review.md](security-review.md). Both
tools are pointed at it.

## When to run what

The version is a date (`YYYY.MM.DD.HHMM.Channel`), so "major" and "minor" are defined here:

| Kind | What it is | What happens |
|---|---|---|
| **Major release** | The first release; any **Beta** or **Rel** release; a channel change (Dev to Beta, Beta to Rel); or a Dev release that adds or changes a way untrusted data comes in (imports, `{txt}`, link files, the network, `{json}`) or the input path (`Sender`, `SendPlan`, `Hotkeys`) | **The full audit runs before the release, without asking.** The release waits until the findings are fixed or accepted by the owner. |
| **Minor release** | Any other release (a tagged Dev build) | Claude **asks** before the release: full audit, quick audit since the last release tag, or skip. |
| **Lesser change** | A pull request that changes code (`src/`, `tests/`, `tools/`, the `.vcxproj`, CI); docs-only PRs don't count | Claude **asks** when it opens the PR: quick audit of this PR, or skip. |

## The full audit

1. Build Release once, so the build stamp and `build\compile_commands.json` exist.
2. Run the clang part on everything:
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_audit.ps1 -Tests
   ```
   It prints a summary by check name. The full output is in `build\audit\clang-tidy.txt` and
   `build\audit\clangd.txt`, and every finding on one line in `build\audit\findings.txt`. Exit
   code 0 = nothing found, 1 = findings to review, 2 = it couldn't run.
3. Run the **claude-security** full scan: ask Claude, in this project, *"Run the full
   claude-security scan of this repository following docs/security-review.md, and write patches
   for what you find."* Claude starts the `claude-security` agent, which captures the commit,
   splits the code into parts, has research agents look for bugs and verifier agents vote on
   each one, and writes a report and a patch file per confirmed finding. It doesn't change your
   files: patches are separate files you choose to apply.
4. Go through the results together (see "Reading the results").
5. Fixes go on a `feature/security-<short-name>` branch, one finding per commit, with a test
   where the code has tests, then a PR as usual. Apply a claude-security patch with
   `git apply <file>.patch`, then build and run `tools\run_tests.ps1`.
6. Note the audit in the release notes: the commit it ran on and what was fixed or accepted.

## The quick audit (one PR, or the changes since a release)

1. The clang part, on the changed files only:
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_audit.ps1 -Changed -Tests
   ```
   `-Changed` compares the current branch with `main`; add `-Base <tag>` to compare with the
   last release tag instead. A changed header also audits its `.cpp`.
2. A security review of the diff: Claude Code's `/security-review` command reviews the
   pending changes on the current branch. Mention `docs/security-review.md` so it uses the
   project's scope and rules.
3. Fix on the same branch before merging, or note why a finding is accepted.

## Reading the results

- **Fix it** when it can happen with real input: anything reachable from an imported file, a
  `.txt` file, a typed path, a web response or a hand-edited `macros.json`, and anything that
  could leave a key held down or type into the game when it shouldn't.
- **Accept it** (with a short reason in the PR or the release notes) when it can't happen, for
  example a check that clang-tidy can't see across functions. Don't silence a check in
  `.clang-tidy` for one case; use `// NOLINT(check-name): reason` on that line.
- **Turn a check off** in `.clang-tidy` only when it's noise everywhere, with the reason in the
  comment at the top of that file.
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
