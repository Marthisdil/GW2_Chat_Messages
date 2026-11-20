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
