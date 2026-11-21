# Security review guide

Read this before a security review or a `claude-security` scan of this repository. It says what
to scan, where untrusted data comes in, which bug classes matter here, and how patches must look.
`CLAUDE.md` holds the project rules; they win over any suggested fix.

## Scope

- **Scan:** `src/`, `tests/`, `tools/` (PowerShell), `.github/workflows/`.
- **Don't scan:** `vendor/` (pinned third-party submodules: Nexus and Mumble headers, ImGui
  v1.80, nlohmann json; never edited in place, and ImGui is never bumped), `build/`, `docs/`.
  Do report it when *our* code misuses a vendor API.
- **Tools:** clangd and `clang-tidy` (`C:\Program Files\LLVM\bin`) read
  `build\compile_commands.json` (written by `tools\compile_commands.ps1`) and `.clang-tidy`.
  `tools\run_audit.ps1` runs both (`-Changed` for a branch's files); its findings are in
  `build\audit\findings.txt`. Prefer targeted runs on the files below over full rebuilds. The
  unit tests are `tools\run_tests.ps1`. How and when audits run: `docs/security-audit.md`.
- **Build:** C++17 (no C++20: no `std::span`, no `<format>`), x64, MSVC v143 in CI (VS 2022) and
  v145 locally. A patch must build on both.

## Where untrusted data comes in (highest risk first)

1. **Imports from other players** (`src/Exchange.*`): `.json` exports, a whole `macros.json`,
   `.txt` files, folders of them, and pasted clipboard text. Check parsing, nesting depth (6
   folders), names (`kMaxNameLength`), line counts (`kMaxLines`), and the size limits in
   `Exchange.h` (`kMaxJsonBytes`, `kMaxTextBytes`, `kMaxFolderFiles`, `kMaxImportItems`). Link
   file paths and the trash bin must never come in through an import.
2. **File paths:**
   - `{txt file}` (`src/Placeholders.*`) may only read inside the addon folder: no `..`, no
     absolute paths, no drive letters, no tricks with `\\?\`, UNC paths, short names or links.
   - Link files (`src/LinkSync.*`): the user may type an absolute path (allowed), but no path may
     come from an imported file.
   - `exports\`, `imports\`, `links\` and the atomic writes in `src/Storage.*` (temp + rename,
     `.bak`): names built from macro or folder names must stay inside their folder.
3. **The network** (`src/Http.*`, `src/MapNames.*`, `{json path url}`): the URL comes from a
   macro the user wrote (or imported). Responses are untrusted: size cap (`kDefaultMaxBytes`),
   JSON parsing, and what the value becomes once typed into chat. `maps.json` is a cache and
   may be corrupt.
4. **Text typed into the game** (`src/ChatLine.*`, `src/SendPlan.*`, `src/Sender.*`): a value
   from a placeholder or a file must not add a newline, an extra Enter, a Tab or a channel
   command it didn't have. Check UTF-8 to UTF-16 conversion and the 199-unit limit.
5. **Our own files** (`macros.json`, `settings.json`, `.bak`): may be hand-edited or damaged; an
   unreadable file is renamed aside, never overwritten.

## Bug classes that matter here

- **Game input safety (top priority):** see "Input safety" in `CLAUDE.md`. Report any path that
  could press or release the user's physical modifiers, leave a key down, use `SendInput`,
  `keybd_event` or the clipboard for typing, type while a text box is focused, or block or
  consume a hotkey. A "fix" that breaks one of these rules is worse than the bug.
- **Threads:** the render thread (owns the tree and ImGui), the sender thread, the WndProc hook,
  Nexus keybind callbacks and the HTTP/map-name workers. Look for shared state without a lock or
  atomic, ImGui calls off the render thread, the tree changed while drawn (it must go through a
  queued `Action`), and threads not joined in `AddonUnload`.
- **Nexus lifetime:** a stored `ImFont*`, callbacks left registered after unload, work in
  `GetAddonDef` or `DllMain`.
- **Memory and bounds:** dangling `std::string_view` or `c_str()`, use after move, iterators
  broken by tree changes, unchecked indexes, signed/unsigned size maths, and buffer sizes in
  Win32 calls (`MultiByteToWideChar`, WinHTTP reads).
- **Win32 results:** unchecked `BOOL`/`HRESULT`/handles, handles not closed (WinHTTP),
  `INVALID_HANDLE_VALUE` vs `nullptr`.
- **`tools/*.ps1` and CI:** paths with spaces (the repo is under `C:\Dropbox\~ Github`), and
  nothing that runs untrusted input.

## Not worth reporting

- `std::mt19937_64` for macro IDs (`MacroTree.cpp`) and `{random}` (`Sender.cpp`): neither needs
  to be unpredictable.
- `ShellExecuteW` in `Storage::OpenFolder`: opens our own addon folder.
- No sockets, named pipes, registry, `CreateProcess` or `LoadLibrary` are used; there are no
  secrets or API keys in the code.

## Patch rules

- Fix the root cause with the standard library and RAII (`std::unique_ptr`, small scope guards,
  checked `.at()` or explicit size checks). Don't add a library: a new dependency is the
  owner's decision, and would be a submodule under `vendor/`.
- One finding per patch, small, with a test when the code has one (`tests/*Tests.cpp`; tree rule
  changes always need a test).
- Follow `CLAUDE.md`: work on a `feature/` branch, never push to `main`, don't change the addon
  `Signature`, `imconfig.h` or anything under `vendor/`, and update `CHANGELOG.md` for
  user-visible changes.
