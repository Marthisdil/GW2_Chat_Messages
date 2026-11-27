# GW2 Chat Messages

A [Nexus](https://raidcore.gg/Nexus) addon for Guild Wars 2: chat macros (one or more lines, each
sent to its own channel) kept in a user-built folder tree (max 6 folder levels; macros allowed at
root). Modelled on the Blish HUD module https://github.com/agaertner/bhm-chat-macros (v0.15.16 is
its `main` branch). **Read `docs/design.md` before feature work**: it holds the decisions, data
model, import/export rules and the phase plan.

**Wiki, with every change:** a user-visible change updates the right `docs/wiki/` pages in the
same PR (a new page also goes into `_Sidebar.md` and gets a link from `Home.md`), and the PR
description says which pages changed, or "Wiki: no change". After the merge, the cleanup
publishes them (below).
`docs/wiki/` is the source of the GitHub wiki (https://github.com/Marthisdil/GW2_Chat_Messages/wiki; flat pages, links
written as `[text](Page-Name)` without `.md`, `_Sidebar.md`). To publish: clone
`https://github.com/Marthisdil/GW2_Chat_Messages.wiki.git` (branch `master`), copy the pages in
(delete a page there that's gone from `docs/wiki/`), commit as `Publish the wiki from docs/wiki
at <short sha of main>` and push, then check the pages, the sidebar and their links. Wiki
pages link images as `../images/name.png`; when publishing, rewrite those to
`https://raw.githubusercontent.com/Marthisdil/GW2_Chat_Messages/main/docs/images/name.png`. Never
edit the wiki on GitHub directly: the next publish overwrites it.

**Security reviews and `claude-security` scans:** read `docs/security-review.md` first (scope,
untrusted inputs, patch rules; `vendor/` is out of scope). How to run them (clang-tidy/clangd via
`tools\run_audit.ps1`, claude-security, `/security-review`) is in `docs/security-audit.md`.
Record every finding in `docs/security-findings.md` (next `SEC-` id; found date/time UTC, by
what, severity, decision, status; when fixed, the PR, commit and first build; when released,
the version). Never delete a finding: close it as Accepted or Not a bug with the reason.
**When:** before a **major release** (first release, any Beta/Rel, a channel change, or a Dev
release that changes an untrusted input or the input path: see that doc's table) the full
audit is required: a whole-repository scan for the first release and a channel change, a
changes-only scan per area otherwise. It's split into areas A-D, one per session (Pro plan):
when a release is mentioned, read the Scan log in `docs/security-findings.md` and propose the
schedule without asking whether to scan; log each scan there with its usage (`/usage` before
and after). Hold the release until every area is covered and findings are fixed or accepted by
the user. CI runs the clang part (`run_audit.ps1 -Tests`) on every push; keep it clean.
Before a **minor release** (any other tagged release), ask: full audit, quick audit since the
last tag, or skip. When opening a PR that changes code (not docs-only), ask: code review and
quick audit, code review only, or skip (see "Code review" under Git workflow).

**Advanced options (owner, 2026-11-25):** for every new feature or idea (built or added to
`docs/todo.md`), say whether it should go behind the **Advanced options** checkbox, and why
(rare need, surprising behaviour, close to ArenaNet's macro policy, or not per-macro), and record
the decision with the idea. A feature behind it is hidden and doesn't work while the box is off
(a macro that uses one is blocked; its settings are kept; see `docs/design.md`, "Advanced
options"). Every UI design (layouts, the welcome tour, screenshots, in-game checks) must work
both ways: with the box off (the default) and on. The current list is in `docs/design.md`.

`docs/todo.md` collects ideas and brainstorming for later discussion. When the user says "add X
to the todo list", add it there (don't start building it). Move agreed items into
`docs/design.md`.

## Input safety (top priority: never break normal game input)

- Never use `SendInput`/`keybd_event`. Send only to the game window (`WndProc_SendToGameOnly` or
  `PostMessage` to the game HWND). Type text with `WM_CHAR`; don't use the clipboard unless that's proven necessary.
- Open chat with `GameBinds_Press/Release` (`GB_UiChatFocus`, `GB_UiSquadBroadcastChatFocus`);
  check `GameBinds_IsBound` first. Never guess keys.
- Never release or press the user's physical modifiers. Any modifier we press is released in a
  scope guard.
- Don't send while `IsTextboxFocused` is already set, the game isn't focused, or `!IsGameplay`.
  Verify every step with bounded waits; on failure, press Escape once (only if the box is still
  focused) and abort.
- Avoid Tab. If whispers need it, only while the whisper box is verified focused.
- One macro at a time on a sender thread; ignore triggers while busy; join on unload.
- Macro hotkeys are built in (`src/Hotkeys.*`): read in a WndProc hook, **never blocked or
  consumed**, ignored while any text box is focused. Only the library toggle and the quick-menu
  pop-up are Nexus InputBinds, registered with `"(null)"` (no default key).

## Nexus references

- Nexus addon manager: https://raidcore.gg/Nexus
- **Addon development guide:** https://raidcore.gg/gw2/guides/tools/nexus/addon-development
- Nexus API header (`Nexus.h`): https://github.com/RaidcoreGG/RCGG-lib-nexus-api
- MumbleLink header (`Mumble.h`): https://github.com/RaidcoreGG/RCGG-lib-mumble-api
- Sister project using the same stack: `C:\Dropbox\~ Github\GW2_WvW_Info` (its `CLAUDE.md`,
  `src/entry.cpp` and `.vcxproj` are a working reference).

## Tech stack

- **Language:** C++17 or newer.
- **Output:** 64-bit Windows DLL. x64 only; GW2 cannot load 32-bit DLLs.
- **Toolchain:** Visual Studio ("Desktop development with C++"). Link the C++ runtime statically for
  release builds (`/MT`).
- **Nexus API:** `Nexus.h` from RCGG-lib-nexus-api as a git submodule under `vendor/nexus`.
  Target `NEXUS_API_VERSION` 6.
- **Dear ImGui:** exactly **v1.80**, regular branch (not docking), as a submodule pinned to tag
  `v1.80`. Compile `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`. Do not
  include the Win32 or DX11 backends. Do not change `imconfig.h` in ways that alter struct layout.
  Never bump the version.
- **Map/player context:** MumbleLink via `DataLink_Get(DL_MUMBLE_LINK)` (RCGG-lib-mumble-api).
- `vendor/` submodules are never edited in place.

## Layout

- `src/entry.cpp`: `GetAddonDef`, `AddonLoad`/`AddonUnload`, render/options/keybind callbacks.
  The addon `Signature` is `0x523CBA0E`. Never change it.
- `src/Version.h`: the version (`YYYY.MM.DD.HHMM.Dev|Beta|Rel`, date and time both UTC), written by
  `tools\set_version.ps1`; never edit it by hand. The `.rc` uses it for the DLL's version info.
- `src/BuildStamp.*`: the build stamp (git short SHA, `+changes` for uncommitted edits, UTC build
  time), from `BuildStampGen.h`, which `tools\build_stamp.ps1` writes into `build\obj\<config>\gen\`
  before every build (a pre-build step in the `.vcxproj`; never committed). Shown in the Nexus log,
  the options page, error reports and the DLL's Properties (Comments). Only `BuildStamp.cpp` and the
  `.rc` include the generated header.
- `src/Globals.h`: `g_Api`, the addon name, the version constants (from `Version.h`),
  and `LogF`.
- `src/MacroTree.*`: the library data model (`Tree`, `Node` folder/macro, `Line`, channels) with the
  rules (6-folder depth, unique sibling names, move checks) and JSON (de)serialization. No Nexus or
  ImGui dependency. Tested by `tests/MacroTreeTests.cpp`; run `tools/run_tests.ps1` (CI runs it too).
  It also holds the filter rules (`GameModeOfMapType`, `ReachOf`, `IsActive`, `NeverActive`,
  `FiltersOverlap`; a folder's filter adds to everything inside it), tested by
  `tests/FilterTests.cpp`, and the trash bin (`MoveToTrash`, `Restore`, saved under `"trash"`),
  tested by `tests/TrashTests.cpp`. A macro's note (`Node::Note`, never sent, cleaned by
  `Tree::CleanNote`: 10 lines, 1000 bytes) and `Tree::Duplicate` (no hotkey, favourite or link
  file) are tested in `tests/MacroTreeTests.cpp`.
  Every tree rule change needs a test. The saved-file rules (`docs/design.md`, "Saved files"):
  `kFormatVersion`, fields from a newer version kept (`Extra`, size-limited), an unknown channel
  kept and never sent (`Line::UnknownChannel`), a newer file read-only (`FromNewerVersion`),
  tested by `tests/FormatTests.cpp` with a sample file per format in `tests/samples/`.
- `src/ChatLine.*`: the raw text format (parse and serialize, with English, German, French and
  Spanish commands), the text typed into chat, channel colours, and the 199-unit length (UTF-16
  units, as the game counts). No Nexus or ImGui dependency. Tested by `tests/ChatLineTests.cpp`.
- `tests/`: `TestMain.cpp` runs every `Run*Tests()`, and `TestCheck.h` has `CHECK`. Add new test
  files and their sources to `tools/run_tests.ps1`.
- `src/Storage.*`: the addon folder, atomic JSON writes (temp + rename, `.bak` of the previous
  file), and `LoadMacros`/`SaveMacros`. An unreadable `macros.json` is renamed aside, never
  overwritten; a newer one is never saved over, and an older one is copied to
  `macros.json.v<N>.bak` before its first save. `settings.json` has `"format"`
  (`kSettingsFormat`). Don't name functions `LoadLibrary` etc.: `windows.h` macros clash.
- `src/Hotkey.*`: the hotkey combo (key + Ctrl/Alt/Shift), its text form for `macros.json`, and
  which keys are refused (`Refusal`; plain Mouse 3/4/5 are kept on load but never fire:
  `KeepOnLoad`, `Fires`). `ToString` is the stored (English) form; `Display` is what the user
  sees, translated. No Nexus or ImGui dependency. Tested by `tests/HotkeyTests.cpp`.
- `src/GameKeys.*`: GW2's default keybinds (from the GW2 wiki), so the hotkey editor can warn
  about a shared key. No Nexus or ImGui dependency. Tested by `tests/GameKeysTests.cpp`.
- `src/Search.*`: the library's search box: what matches (names, line text, whisper recipients,
  hotkeys, notes; case and accents ignored) and which tree nodes show, plus the "only active here" filter (`Where`), also used by the
  quick menu search. No Nexus or ImGui dependency.
  Tested by `tests/SearchTests.cpp`.
- `src/Hotkeys.*`: the WndProc hook that reads presses and captures a new hotkey. It only queues
  presses; the render thread (`Ui.cpp`) matches them to macros and starts the sender.
- `src/Examples.*`: the starter library (the Examples folder added on the first run and by
  "Add the example macros"); its names and lines are `TR` texts. Its top folder has the reserved
  id `MacroTree::kReservedExamplesId`, which nothing else may ever get: never hand it out
  except through `Tree::ClaimReservedId`. No Nexus or ImGui dependency. Tested by
  `tests/ExamplesTests.cpp`.
- `src/Backups.*`: backups on the user's schedule (or a weekly safety copy while they're off):
  the file names (`macros_backup_YYYY-MM-DD_HH-MM-SS_UTC.json`, `macros_safety_...`), when one is
  due, which old copies go, and the `"backups"` setting. No Nexus or ImGui dependency. Tested by
  `tests/BackupsTests.cpp`. `Storage` reads and writes the files in `<addon>\backups\` (only
  names `ParseFileName` accepts); `Ui.cpp` checks the schedule once a minute and draws the
  options page's Backups and Restore tabs.
- `src/QuickLists.*`: the quick menu's Favourites and Recently sent lists (`Recent`, `Build`) and
  the `"recently_sent"` / `"recent_count"` settings. No Nexus or ImGui dependency. Tested by
  `tests/QuickListsTests.cpp`. The favourite mark itself is `Node::Favourite` (`MacroTree`).
- `src/Advanced.*`: the "Advanced options" checkbox's rules: which macros use a feature behind it
  (`Uses`, `Count`), which can't be sent while it's off (`Blocked`: linked macros), the cooldown
  in effect (`CooldownSec`), and the `"advanced_options"` setting (`FromSettings`, off unless
  saved as true). `Ui.cpp` hides the features while it's off. Settings are always kept. A new feature behind the box: add its
  rule here, and hide it in the UI. No Nexus or ImGui dependency. Tested by `tests/AdvancedTests.cpp`.
- `src/EditorLayout.*`: the macro editor's folding sections (which are open, saved as
  `"editor_sections"` in `settings.json`) and the note's one-line summary (`ShortNote`). No Nexus
  or ImGui dependency. Tested by `tests/EditorLayoutTests.cpp`. The layout itself is in `Ui.cpp`
  (`docs/design.md`, "Macro editor layout").
- `src/Welcome.*`: the first-run welcome (`Decide`: a real first run only, `"welcome_shown"`)
  and the library tour's steps (`Steps`, `TX` texts, `NeedsMacro`). `src/ReleaseNotes.*`: "What's
  new" after an update, from `CHANGELOG.md` built in as resource type `NOTES` (`Parse`, `Since`,
  `Decide`, `ToBlocks`; `"notes_seen"`, `"show_update_notes"`). No Nexus or ImGui dependency.
  Both tested by `tests/WelcomeTests.cpp`. `Ui.cpp` draws them: tour targets call `TourMark`
  after they're drawn (`docs/design.md`, "First-run welcome and tour", "Update notes").
- `src/Cooldown.*`: the hotkey cooldown and the 1-second guard after a send (`Tracker`), per
  macro; only hotkeys wait. No Nexus or ImGui dependency. Tested by `tests/CooldownTests.cpp`.
- `src/Placeholders.*`: fills in `{time}`, `{random}`, `{map}` and the other placeholders. The
  clock, random numbers and the map name come in through a `Context`. No Nexus or ImGui
  dependency. Tested by `tests/PlaceholderTests.cpp`. `{txt}` and `{json}` were removed
  (2026-11-23): a line that still uses one is refused, never sent as typed (`UsesRemoved`). Don't
  add a placeholder that reads files or fetches an address written in a macro.
- `src/Http.*`: a small blocking WinHTTP GET (worker threads only), only for addresses built into
  the addon. `src/MapNames.*`: GW2 API map names cached in `maps.json`, looked up on a background
  thread, plus the API's full map list (fetched weekly, on a language change, after a game
  update (MumbleLink's `BuildID`, `MapNames::Check`, and once more a day later) and on request) and
  the maps visited (saved in `settings.json`, `visited_maps`: `maps.json` is only a cache). Link
  `winhttp.lib`. `src/MapCatalog.*`: its rules (parsing the API, the `maps.json` format, the map
  picker's search, visited maps, when a game build makes a refresh due: `NoteBuild`,
  `ListFetched`), no Nexus or ImGui dependency,
  tested by `tests/MapCatalogTests.cpp`.
- `src/PathText.*`: paths to and from UTF-8 without throwing (a bad sequence becomes U+FFFD).
  Never call `path::u8string()`, `path::string()` or `fs::u8path()`: use `PathText::Utf8` and
  `PathText::FromUtf8`. No Nexus, ImGui or windows.h dependency. Tested by
  `tests/PathTextTests.cpp`.
- `src/Exchange.*`: import and export: the export format, the Export window's ticks (`Pick`,
  `ExportPickedJson`), reading .json/.txt/folders into a package, the import preview
  (`BuildPlan`) and `Apply`. No Nexus or ImGui dependency. Tested by
  `tests/ExchangeTests.cpp`. Exports go to `<addon>\exports\`; the Import window lists
  `<addon>\imports\`. "A whole library" (`FromLibraryFile`, `WrapInFolder`) imports someone's
  `macros.json` into one new top-level folder.
- `src/LinkSync.*`: link files (a macro kept in step with a `.txt` file both ways): path rules
  and the sync decision. No Nexus or ImGui dependency. Tested by `tests/LinkSyncTests.cpp`. The
  UI (`Ui.cpp`) polls once a second and does the reads and writes.
- `src/ErrorReport.*`: the error window's queue and its clipboard text. No Nexus or ImGui
  dependency. Tested by `tests/ErrorReportTests.cpp`. Errors in `Ui.cpp` go through `ShowError`
  (what the user did + the error); never show an error inline instead.
- `src/TextSize.*`: the text size rules (which Nexus fonts the dropdown lists, the scale, window
  rescaling). No Nexus or ImGui dependency. Tested by `tests/TextSizeTests.cpp`.
  `src/UiFont.*`: the ImGui side: `Push`/`Pop` the chosen font around our windows, `Px()` for
  fixed sizes, `WindowFit` for window sizes, `Row` for button rows that wrap. Never hard-code a
  pixel size in `Ui.cpp`: use `Px()` or measure the text. Our windows push `TextWrapPos(0)`.
- `src/Localization.*`: language files (`Catalog`, `ParseCatalog` and its safety checks: size,
  depth, ids, `SafeText`, matching `%`-codes) and the lookup. **The English text is the key:**
  wrap every new UI text in `T("...")` (literals only; render thread only), mark texts in
  tables with `TX("...")` and look them up with `Localization::Lookup`, and format with
  `Fmt(T("...%s..."), ...)`. Messages built in a core module or on a worker thread use
  `TR("...")` (literals only, any thread; whole sentences with `%`-codes, never English pieces
  glued together). Never compare a message's text to decide something. Window and pop-up titles
  use `"Text###stableid"`. Then run
  `tools\localization.ps1 -Write` (updates `localization/en.json`, `template.json` and the
  other languages; `run_tests.ps1` fails if they're stale) and translate the new texts in
  `de.json`, `fr.json` and `es.json` (the tests require them complete). No Nexus or ImGui
  dependency. Tested by `tests/LocalizationTests.cpp`.
  `src/Languages.*`: loads the built-in languages (resource type `LOCALE`) and
  `<addon>\localization\*.json`, follows Nexus' language, gives Nexus the keybind names, and
  saves the `language` setting. The localization folder is read only after its own opt-in
  (`Languages::UseFolder`, the `"own_language_files"` setting: `Localization::OwnFilesFromSettings`),
  not Advanced options. Language files have a `"Format"` (`Localization::kFormat`; no field = 1);
  a newer one is used for what this version understands (`FromNewer`), unknown fields are
  ignored, and a text this version doesn't use counts as `Unused`, not a problem. Tested by
  `tests/LocalizationTests.cpp` with `tests/samples/language_format1.json`. Language files are an untrusted input
  (`docs/security-review.md`). `CONTRIBUTING.md` is the translators' guide.
- `src/SendPlan.*`: turns a macro's lines into the exact send steps (open chat, wait, type, Tab
  for whispers, Enter, wait) and validates them. No Nexus or ImGui dependency. Tested by
  `tests/SendPlanTests.cpp`.
- `src/Sender.*`: runs a plan on its own thread. Keys and text are executed on the game's window
  thread via a private posted message handled in our WndProc hook (`WndProc_SendToGameOnly`).
  In-game checks live in `docs/in-game-tests.md`.
- `src/Ui.*`: the library window (tree + details pane), popups, and debounced saving. The tree is
  only touched on the render thread. Never change the tree while drawing it: queue an `Action`
  and apply it after. Open popups at the main window's ID level, not from inside a child window.
- `src/resource.h` + `src/GW2_Chat_Messages.rc`: Quick Access icons embedded as resource type
  `PNG`, the built-in language files (`localization/en|de|fr|es.json`) as type `LOCALE`, and
  `CHANGELOG.md` as type `NOTES` (for "What's new"). The icon is a speech bubble with a lightning bolt (the "quick bubble"); regenerate it with `tools/draw_chat_icon.ps1 -OutDir resources\icons`.
  The other icon concepts that were considered are in `docs/icon-suggestions/` (README + SVGs).
- `vendor/`: submodules `nexus`, `mumble`, `imgui` (tag v1.80) and `json` (nlohmann v3.11.3,
  shallow). Include json as `<nlohmann/json.hpp>`.
- `GW2_Chat_Messages.sln` / `.vcxproj`: VS project (v143, x64 only). Add new files to the
  `.vcxproj`.
- `.github/workflows/build.yml`: CI builds Debug and Release, checks the `GetAddonDef` export,
  and uploads the DLL.
- Addon data lives in `<GW2>\addons\GW2_Chat_Messages\`. Nexus does not create this folder.

## Nexus addon rules

- `GetAddonDef()` must have no side effects and must not call any API functions. Return a pointer to
  a static `AddonDefinition_t`.
- The addon `Signature` is fixed once chosen. Never change it and never use 0.
- In `AddonLoad`, call `ImGui::SetCurrentContext(api->ImguiContext)` and
  `ImGui::SetAllocatorFunctions(api->ImguiMalloc, api->ImguiFree)`. Initialize data before
  registering any callbacks, because callbacks can fire on other threads.
- Never call `ImGui::CreateContext`, `NewFrame`, or `Render`.
- `AddonUnload` must undo everything `AddonLoad` did: deregister renders, keybinds, events, WndProc,
  and Quick Access entries. Release fonts, join threads, and save settings.
- Keep `DllMain` empty, or use it only to store the module handle.
- Keybind handlers run on their own thread: only set atomics there, consumed by the render thread.
  Worker threads must not touch ImGui.
- Never keep an `ImFont*`; Nexus sends nullptr before atlas rebuilds.
- `Paths_GetAddonDirectory(...)` returns the addon folder path, but Nexus does not create it.
- Nexus has no "open my options" API and no shared GW2 API key (as of API v6).
- The GitHub updater ships only the DLL, so assets (icons etc.) must be embedded as resources.
- Versions are `YYYY.MM.DD.HHMM.Channel` (date and time both UTC; Channel `Dev` until initial development is done,
  then `Beta` or `Rel`). For every release run `tools\set_version.ps1 -Channel Dev` and commit
  `src/Version.h` and `CHANGELOG.md` (it moves the `[Unreleased]` notes under the new version,
  with the channel in the heading, which "What's new" shows after the update: Rel builds show
  Rel releases only, Beta builds Beta and Rel, Dev builds all). The GitHub release tag is the numbers only (`2026.11.21.1412`; Nexus ignores
  suffixed tags); Dev and Beta releases are GitHub pre-releases.

## Building (local notes)

- **Screenshots, at every build:** before building, run
  `powershell -NoProfile -ExecutionPolicy Bypass -File tools\update_screenshots.ps1`. It puts any
  new PNG from `docs\images\` into its placeholders in README.md and `docs/wiki/` and marks its
  row in `docs/screenshots.md` "Captured", moving it to the "Done" table. Tell the user what was captured, and ask about any file
  it lists as "Not in the list". Captured images go into the current branch's PR.
- **In-game tests and screenshots, after every build or update** (owner, 2026-11-23): check
  what the build changed against `docs/in-game-tests.md` and `docs/screenshots.md`, and update
  both lists if a check or a screenshot is missing or out of date (in the branch's PR; on
  `main`, in a small docs PR). In the build or cleanup report, name the in-game checks that
  are new or need re-running with this build, and the screenshots still needed for it.
- **Screenshot list and in-game tests, with every change:** in the same PR, add a row to
  `docs/screenshots.md` (plus a placeholder `<!-- screenshot: name.png (docs/screenshots.md) -->`
  and a "Screenshot to come" line) for each new screen, and rewrite any row whose screen changed
  (back to "Needed" if its picture is now out of date). Also add, change or remove the checks in
  `docs/in-game-tests.md` that the change affects, and keep its "Progress" table current with the
  user's results.
- **Done sections (owner, 2026-11-25):** both lists show only what's still to do at the top.
  An in-game check that passes (or fails, is fixed and passes its re-check) moves, text and
  Progress row, to "Done (passed checks)" at the bottom of `docs/in-game-tests.md`. A captured
  picture with no retake pending is in the "Done" table at the bottom of `docs/screenshots.md`
  (under "Retakes"); one waiting on a retake stays in the list as "Captured, retake". When a
  change means a done check must run again or a done picture is out of date, move it back up
  (the check into its session and the Progress table; the picture's row to "Needed").
- The repo is in Dropbox. Compile-time PDB writes (`/Zi`) fail under `C:\Dropbox` with
  `C1090 PDB API call failed`. Use `DebugInformationFormat=OldStyle` (`/Z7`) for Debug.
- Locally only the VS 2026 v145 toolset is installed; CI (windows-2022) uses v143. Build locally with
  `/p:PlatformToolset=v145`.
- Local build: `& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" GW2_Chat_Messages.sln /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145`
- To test, copy `build\Release\GW2_Chat_Messages.dll` into `<GW2>\addons\`.
- **clangd:** `tools\compile_commands.ps1` writes `build\compile_commands.json` (clang-cl commands
  from the `.vcxproj`'s Release settings, plus the test files listed in `tools\run_tests.ps1`).
  The pre-build step runs it, and it only rewrites the file when it's missing or older than those
  files (`-Force` rewrites anyway). It never fails the build. clangd finds the file in `build\` by
  itself, and `.gitignore` keeps `compile_commands.json` and `.cache/` out of git. clangd is at
  `C:\Program Files\LLVM\bin\clangd.exe`. Its `--check` "errors" from `tweak: ExtractFunction` are
  refactoring self-tests, not compile errors.

## Git workflow

- **Never push to `main`.** Work on a branch named `feature/<short-name>`.
- Commit messages are imperative and under 72 characters.
- Update `CHANGELOG.md` for every user-visible change.
- **Saved files (owner, 2026-11-26):** every PR that changes what's saved (`macros.json`,
  `settings.json`, exports) says in its description whether the change is **additive** or
  **breaking** (`docs/design.md`, "Saved files"). A breaking one raises
  `MacroTree::kFormatVersion`, adds the upgrade step and a `tests/samples/macros_format<N>.json`.
  Never give a setting a new meaning under the same name.
- Do not commit build output (`.dll`, `.pdb`, `.obj`, `.exe`, ...).
- **Code review (owner, 2026-11-24):** on a PR that changes code, after the build and
  `run_tests.ps1` pass, run `/code-review` on the branch: **medium**, or **high** when it
  changes the input path (`Sender`, `SendPlan`, `Hotkeys`, `Placeholders`). Never `max` or
  `ultra` unless the owner asks (ultra is billed). Check every finding before acting on it, fix
  the real ones on the same branch, and put the result in the PR body ("Code review (medium):
  2 findings fixed" or "none"), next to the audit line. Docs-only PRs get neither.
- **Wait for CI (owner, 2026-11-25):** never ask whether a PR is merged while its CI is still
  running or failing. All green: ask. Still running (normal runs take 7-12 minutes): say so and
  check again when the user asks. Failing: fix it on the branch first. The build job stops
  itself after 30 minutes (`timeout-minutes` in `.github/workflows/build.yml`); a run that hit
  it or hangs is re-run (`gh run rerun`) or cancelled only on the user's OK.
- **`/simplify`:** not on the existing code, and not on every PR. Only on a larger feature PR,
  before `/code-review`, and never on `Sender`, `SendPlan`, `Hotkeys`, `Placeholders` or
  `vendor/` without the owner's OK (it may "simplify" a safety rule away). Its edits go in their
  own commit and must keep behaviour the same: no change to `T("...")` texts, popup IDs, JSON
  keys, the Signature, or the render-thread `Action` queue. Then the build, the tests and, for
  `Ui.cpp`, the in-game checks of the screens it touched (named in the PR). The `Ui.cpp`
  cleanup PR waits until the in-game testing is finished (`docs/todo.md`).
- **Private test notes:** `docs/testing results.txt` stays untracked. A PreToolUse hook
  (`.claude/settings.json`, `.claude/hooks/guard_testing_results.py`) blocks any git command
  that names it, a `git add` of everything (`-A`, `.`, `docs`, ...), and a commit while it's
  staged. Stage files by name.
- README keeps an "AI Disclosure" section as its last section; prose is first person singular ("I").
- **Wiki at every merge cleanup:** the wiki repo's last commit message ends with the main commit
  it was published from (`Publish the wiki from docs/wiki at <sha>`). If
  `git diff --quiet <sha> HEAD -- docs/wiki` shows changes, publish `docs/wiki/` (steps at the
  top), check the changed pages on GitHub, and say so in the cleanup report.
- After each merge and cleanup, ask the user whether to build or update the restore package
  (`C:\Dropbox\~ Github\GW2_Chat_Messages_restore_v<version>_<sha>.zip`). If they say yes, build it
  and test-restore it before doing anything else.
