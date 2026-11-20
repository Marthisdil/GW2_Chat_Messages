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
