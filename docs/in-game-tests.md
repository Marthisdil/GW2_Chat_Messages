# In-game tests

Things the unit tests can't cover. Run them after copying `build\Release\GW2_Chat_Messages.dll`
into `<GW2>\addons\`. Note the result next to each item (pass / fail + what happened), and check
the Nexus log for `Chat Messages` warnings.

## Before you start

- In Nexus > Options > Game Keybinds, make sure **Chat** (and, for commander tests, **Squad
  Broadcast**) match your in-game keys. The sender uses these; it never guesses a key.
- Create a test folder with these macros:
  - `Say` — one line: Say, `test 1`
  - `Two` — Map `test map` + Party `test party` (or Say twice if you're not in a party)
  - `Whisper` — Whisper to your own character name, `test whisper`
  - `Emote` — Current channel, `/wave`
  - `Long` — one Say line with exactly 196 characters (shows 199/199)
  - `Accents` — Say, `héllo wörld ñ ü – “quotes”`

## Phase 4: the sender

### Basic sending
1. Select `Say`, click **Send now**. The line appears in Say chat; chat closes afterwards.
   The status says "Sent 1 line."
2. `Two`: both lines arrive, in order, in the right channels.
3. `Emote`: your character waves.
4. `Accents`: every character arrives unchanged (tests `WM_CHAR` with non-ASCII text).
5. `Long`: sends in full; nothing is cut off.
6. Right-click a macro > **Send now** does the same as the button.

### Does the game take our keys? (the big unknowns)
7. Does the chat key press (GameBinds) open chat while the mouse is over the addon window?
8. Does `WM_CHAR` typing work at all? If chat opens but stays empty, the game ignores `WM_CHAR`
   and we need the clipboard fallback (docs/design.md).
9. Does our Enter send the message? If the text is typed but not sent, the status says
   "the game didn't take the message".
10. Leave some text in the chat box, close it (Escape keeps the text in GW2), then send `Say`.
    Does the leftover text get mixed into the message? (We can't clear it without Ctrl+A.)

### Whispers and Tab (the bhm problem)
11. `Whisper` to yourself: the whisper arrives.
12. Right after, press **Tab** in the open world: it must target the nearest enemy. Repeat a few
    times, and again after sending 5 whispers in a row.
13. Try typing `/w Name ` then the message without Tab in chat by hand: if GW2 accepts that, the
    sender can drop Tab entirely (docs/todo.md open question).

### Refusing to send (nothing should happen in game)
14. Start typing in chat yourself, then click **Send now** from the window: refused, "A text box
    is open in game". Your own text is untouched.
15. Hold Shift (or Ctrl), click **Send now**, keep holding for over half a second: refused with
    the modifier message; no chat opens.
16. Clear the Chat key in Nexus Game Keybinds: refused with the "set it in Nexus" message.
17. On a loading screen or character select: refused ("Not in game").
18. Click **Send now** twice quickly: the second click is ignored while the first macro sends.

### Recovery
19. While a multi-line macro sends, click into the game or press Escape yourself. The sender
    stops with a message, sends at most one Escape, and the game menu does **not** open.
20. After every test above: movement keys, skills, Tab targeting, Shift/Ctrl/Alt combos and
    the chat key all still work normally.
21. Unload the addon (Nexus > Addons) while a long macro is sending: the game doesn't freeze
    and unload finishes within about a second.

### Commander
22. As commander, a Squad line with **Broadcast** goes out as a squad broadcast.
23. Not commander: the same line is sent as normal squad chat.

## Phase 5: hotkeys

### Setting a hotkey
24. Select a macro, click **Set...**, press Ctrl+F1: the editor and the tree show "Ctrl+F1".
25. **Set...** then Escape: the capture is cancelled and the old hotkey stays.
26. **Set...** with the mouse over the addon window, press Mouse 4 (and Mouse 3, Mouse 5): are
    the mouse buttons recorded? (Nexus may keep mouse clicks over its windows for ImGui.)
27. Try Escape-with-Ctrl, plain Tab, plain Enter: refused with a message.
28. Give two macros the same hotkey: both show "Also set on"; pressing it shows an alert and
    sends nothing.
29. Close the window while capturing: the capture is cancelled.
30. Reload the addon: hotkeys are still there (saved in `macros.json` as text).

### Using a hotkey
31. With the library closed, press the hotkey: the macro sends. Release Ctrl quickly; holding it
    over half a second makes the send stop with the modifier message (by design).
32. Hold the hotkey down: it sends once (auto-repeat is ignored).
33. Open chat and type text containing the hotkey's key (e.g. a plain F-key or Ctrl+key): nothing
    is sent while chat is open.
34. Type in an addon text box (our message box, or another addon's): hotkeys don't fire.
35. Press the hotkey twice quickly: the second press is ignored while the first is sending.
36. Make a hotkey fail on purpose (e.g. a whisper macro with chat key unbound): an alert appears.
37. **Game input check:** after using hotkeys a lot, every key still works in game, including the
    hotkey's own key doing its normal in-game action, Tab targeting, and Shift/Ctrl/Alt combos.
38. Mouse 4/5 hotkeys while moving the camera or fighting: no stuck buttons, no lost clicks.

### Quick menu
39. Right-click the scroll icon: the menu lists the library as nested folders; empty folders
    are hidden. Hovering a macro shows its lines. Clicking it sends it, and the menu closes.
40. Set "Open Chat Messages quick menu" in Nexus Keybinds. Press it: the menu opens at the mouse
    pointer. Press it again: it closes. Click outside: it closes.
41. Open the pop-up while in action camera / with the mouse hidden: where does it appear, and can
    you still use it? (May need a "centre of screen" option.)
42. Send from the menu with the chat key unbound: an alert explains why.
43. A folder and a macro with the same name in one folder both work in the menu.
44. Game input check: after using the menu, left/right click, camera turning and Tab still work.

## Phase 5: filters

45. A macro with no filters ("Everywhere") is active in PvE, WvW, PvP and Armistice Bastion.
46. Untick PvE on a macro: in open world it's dimmed in the library, missing from the quick menu,
    and its hotkey does nothing. **Send now** still sends it.
47. Try to untick the last game mode: refused with a message.
48. "Add this map" adds the current map id; the macro is then active only on that map. Remove it
    with its "x" button.
49. Check what mode these report ("Active here (..., map N)"): Heart of the Mists (PvP lobby),
    Edge of the Mists, Armistice Bastion, a fractal, a raid, a strike mission, the home instance,
    a guild hall, and a story instance. Note any that land in the wrong mode.
50. Two macros with the same hotkey, one WvW-only and one PvE-only: no warning in the editor;
    the hotkey sends the right one in each mode.
51. Same hotkey on two macros that are both active in PvE: warning in the editor; pressing it in
    PvE shows the "set on 2 macros" alert and sends nothing.
52. On a map with no active macros the quick menu says "No macros for this map."

## Phase 6: placeholders

53. `/s It's {time} on {today}`: sends the real time and date. Is the weekday in your Windows
    language? (bhm uses the date format "Friday, 20.11.2026".)
54. `{random 1 6}` many times: results 1-6, both ends included.
55. Put `names.txt` with a few lines in `<GW2>\addons\Chat Messages\`: `{txt names.txt}` sends a
    random line; `{txt names.txt 2}` sends the second non-blank line. Accented letters arrive
    correctly (save the file as UTF-8).
56. `{txt missing.txt}`: nothing is sent; the status says the file wasn't found.
57. `{smile} hello`: sent exactly as typed.
58. A waypoint chat link in a message (e.g. `/s meet at [&BDAEAAA=]`): arrives as a clickable
    waypoint link.
59. A line that's under 199 characters as typed but over 199 once filled in (e.g. a long
    `{txt}` line): refused with the "too long" message, nothing sent.

### Map names and {json}
60. Change maps a few times, then open a macro's "Where it's active": "Add this map" shows the
    map's name, and `maps.json` appears in the addon folder.
61. `/s Hello from {map}`: sends the map's name. Try a map you've never been to since loading.
62. Disconnect from the internet, delete `maps.json`, reload, enter a new map: `{map}` fails with
    a "couldn't be reached" message and nothing is sent. The game doesn't stutter.
63. `{json id https://api.guildwars2.com/v2/build}`: sends the current game build number.
64. `{json nope https://api.guildwars2.com/v2/build}`: nothing is sent; the message says the path
    wasn't found.
65. While "Preparing..." shows, start typing in chat yourself: the send stops ("A text box is
    open") and **your chat box stays open with your text** (the Escape fix).
66. Unload the addon right after sending a `{json}` macro: unload finishes within a few seconds.

## Phase 7: import and export
67. Right-click a macro > Export > Save as a .json file, then again: two files appear in
    `exports\` (`Name.json`, `Name (2).json`), and the status line names the file.
68. Export > Save as a .txt file and Copy the lines as text: the raw lines, one per row.
69. Export all, then Import... > Paste from clipboard after "Copy to the clipboard" on a folder:
    the preview shows the folder as "goes into the existing folder" and its macros as renamed.
70. Put the export in `imports\`, open Import...: it's listed; click it, pick a folder, change a
    conflict to Replace for one macro and Skip for another; the preview matches what Import does.
71. Undo import: the library is exactly as before (names, hotkeys, filters, selection cleared).
72. Import a folder of .txt files with sub-folders, into a folder at level 5: the too-deep folders
    are left out, or with the checkbox ticked their macros land one level up.
73. Import with "Keep hotkeys" unticked: imported macros have no hotkey.
74. Type a path with quotes (Explorer's "Copy as path") and press Enter: it loads.
75. Open imports folder: Explorer opens; returning to the game, keys and Tab still work.
76. While the Import window is open, hotkeys still work and typing in its path box doesn't
    trigger them.
