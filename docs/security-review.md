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
   - Link files (`src/LinkSync.*`): the user may type an absolute path (allowed), but no path may
     come from an imported file.
   - `exports\`, `imports\`, `links\` and the atomic writes in `src/Storage.*` (temp + rename,
     `.bak`): names built from macro or folder names must stay inside their folder.
3. **The network** (`src/Http.*`, `src/MapNames.*`): only the GW2 API's map names, at an
   address built into the addon (`{json}`, which took an address from a macro, was removed on
   2026-11-23). Responses are untrusted: size cap (`kDefaultMaxBytes`), JSON parsing, and what a
   map name becomes once typed into chat. `maps.json` is a cache and may be corrupt. Since
   2026-11-23 it also holds the API's full map list and the visited maps (format 2), parsed by
   `src/MapCatalog.*`: read up to 4 MB, at most 20,000 maps and 200 visited ids, names and regions
   at most 200 bytes and passing `Localization::SafeText`; the map list is fetched in batches of
   200 ids (`/v2/maps?ids=...&lang=` with an `ApiLang` code only).
4. **Text typed into the game** (`src/ChatLine.*`, `src/SendPlan.*`, `src/Sender.*`): a value
   from a placeholder must not add a newline, an extra Enter, a Tab or a channel
   command it didn't have. Check UTF-8 to UTF-16 conversion and the 199-unit limit.
5. **Language files** (`src/Localization.*`, `src/Languages.*`): `.json` files in
   `<addon>\localization\`, likely written by other people and passed around. Their texts are
   drawn in our windows and many are **printf format strings** (`Fmt(T("...%s..."), ...)`), so
   a translation must never change the format codes: check `Placeholders`/`SamePlaceholders`
   (same codes, same order, no `%n`, `%*`, `%1$`). Also check the size and depth limits in
   `Localization.h`, the `Identifier` and `DisplayName` rules, `SafeText` (UTF-8, control
   characters, bidi marks, `##`), that only keys known from English are used, the folder rules
   in `Languages.cpp` (regular files only, no links, at most `kMaxFiles`), and that a text can't
   change an ImGui ID (the `##` part is kept from the English literal). The built-in files go
   through the same checks.
6. **Our own files** (`macros.json`, `settings.json`, `.bak`, `backups\*.json`): may be hand-edited
   or damaged; an unreadable file is renamed aside, never overwritten. Backups are read (capped at
   32 MB) only under names `Backups::ParseFileName` accepts, and a restore checks the whole file
   before the library changes.

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
