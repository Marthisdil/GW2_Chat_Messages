# GW2 Chat Messages

A [Nexus](https://raidcore.gg/Nexus) addon for Guild Wars 2: chat macros (one or more lines, each
sent to its own channel) kept in a user-built folder tree (max 6 folder levels; macros allowed at
root). Modelled on the Blish HUD module https://github.com/agaertner/bhm-chat-macros (v0.15.16 is
its `main` branch). **Read `docs/design.md` before feature work**: it holds the decisions, data
model, import/export rules and the phase plan.

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
- `src/Globals.h`: `g_Api`, the addon name, the version constants (bump them for every release),
  and `LogF`.
- `src/MacroTree.*`: the library data model (`Tree`, `Node` folder/macro, `Line`, channels) with the
  rules (6-folder depth, unique sibling names, move checks) and JSON (de)serialization. No Nexus or
  ImGui dependency. Tested by `tests/MacroTreeTests.cpp`; run `tools/run_tests.ps1` (CI runs it too).
  Every tree rule change needs a test.
- `src/ChatLine.*`: the raw text format (parse and serialize, with English, German, French and
  Spanish commands), the text typed into chat, channel colours, and the 199-unit length (UTF-16
  units, as the game counts). No Nexus or ImGui dependency. Tested by `tests/ChatLineTests.cpp`.
- `tests/`: `TestMain.cpp` runs every `Run*Tests()`, and `TestCheck.h` has `CHECK`. Add new test
  files and their sources to `tools/run_tests.ps1`.
- `src/Storage.*`: the addon folder, atomic JSON writes (temp + rename, `.bak` of the previous
  file), and `LoadMacros`/`SaveMacros`. An unreadable `macros.json` is renamed aside, never
  overwritten. Don't name functions `LoadLibrary` etc.: `windows.h` macros clash.
- `src/Hotkey.*`: the hotkey combo (key + Ctrl/Alt/Shift), its text form for `macros.json`, and
  which keys are refused. No Nexus or ImGui dependency. Tested by `tests/HotkeyTests.cpp`.
- `src/Hotkeys.*`: the WndProc hook that reads presses and captures a new hotkey. It only queues
  presses; the render thread (`Ui.cpp`) matches them to macros and starts the sender.
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
  `PNG`. The icon is a scroll; regenerate it with `tools/draw_chat_icon.ps1 -OutDir resources\icons`.
  The other icon concepts that were considered are in `docs/icon-suggestions/` (README + SVGs).
- `vendor/`: submodules `nexus`, `mumble`, `imgui` (tag v1.80) and `json` (nlohmann v3.11.3,
  shallow). Include json as `<nlohmann/json.hpp>`.
- `GW2_Chat_Messages.sln` / `.vcxproj`: VS project (v143, x64 only). Add new files to the
  `.vcxproj`.
- `.github/workflows/build.yml`: CI builds Debug and Release, checks the `GetAddonDef` export,
  and uploads the DLL.
- Addon data lives in `<GW2>\addons\Chat Messages\`. Nexus does not create this folder.

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
- Bump the version for every release; GitHub release tags use `1.2.3` or `v1.2.3`.

## Building (local notes)

- The repo is in Dropbox. Compile-time PDB writes (`/Zi`) fail under `C:\Dropbox` with
  `C1090 PDB API call failed`. Use `DebugInformationFormat=OldStyle` (`/Z7`) for Debug.
- Locally only the VS 2026 v145 toolset is installed; CI (windows-2022) uses v143. Build locally with
  `/p:PlatformToolset=v145`.
- Local build: `& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" GW2_Chat_Messages.sln /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145`
- To test, copy `build\Release\GW2_Chat_Messages.dll` into `<GW2>\addons\`.

## Git workflow

- **Never push to `main`.** Work on a branch named `feature/<short-name>`.
- Commit messages are imperative and under 72 characters.
- Update `CHANGELOG.md` for every user-visible change.
- Do not commit build output (`.dll`, `.pdb`, `.obj`, `.exe`, ...).
- README keeps an "AI Disclosure" section as its last section; prose is first person singular ("I").
- After each merge and cleanup, ask the user whether to build or update the restore package
  (`C:\Dropbox\~ Github\GW2_Chat_Messages_restore_v<version>_<sha>.zip`). If they say yes, build it
  and test-restore it before doing anything else.
