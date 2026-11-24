# Sending macros

There are three ways to send a macro. All of them type the lines into chat the same way.

## Send now

**Send now** in the macro editor (or in a macro's right-click menu) sends it straight away.
It always works, even where the macro's [filters](Filters) say it isn't active.

The editor shows the progress ("Preparing...", "Sending line 2 of 3...") and the result.

<!-- screenshot: send-status.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the status line after Send now.*

### Preview

**Preview**, next to **Send now**, shows what Send now would type, without sending anything:

- each line with its channel, as it goes into the chat box (`/s hello`, `/g3 see you`), with
  [placeholders](Placeholders) filled in;
- for a whisper, `/w Name`, then Tab, then the message;
- for a squad broadcast (when you're the commander), just the message.

It runs the same checks as a real send. If one fails (a line too long once filled in, a whisper
with no recipient, a placeholder that can't be filled in), the error window says why, exactly as
Send now would, and nothing is shown.

The preview stays under the buttons until you change the macro's lines, pick another macro or
click **Close preview**. `{random}` and the time are filled in again when you really send, so
they can differ. `{map}` only uses map names the addon already knows: the first time on a map,
the preview may ask you to try again in a moment while the name is looked up.

<!-- screenshot: send-preview.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the preview under Send now.*

## Hotkeys

Each macro can have its own hotkey. In the macro editor, next to **Hotkey:**, click **Set...**
(or **Change...**) and press the key you want.

- You can add **Ctrl**, **Alt** or **Shift**. **Mouse 3** (middle), **Mouse 4** and **Mouse 5**
  work too, but only together with Ctrl, Alt or Shift (for example **Ctrl+Mouse 4**): on their
  own they're often dodge or a skill, so they'd send the macro by accident.
- Escape cancels. **Clear** removes the hotkey.
- Not allowed: left and right mouse buttons, Escape, a modifier key on its own, plain Mouse 3/4/5,
  and plain Tab, Enter, Space or Backspace.
- **Why never the left or right mouse button?** Not even with Ctrl, Alt or Shift. You click them
  all the time (to move the camera, target, use the UI), and in GW2 Ctrl+click and Shift+click
  already do things: they put chat links into chat and target or call targets. A macro on them
  would go off by accident, so the addon refuses them as a safety measure.
- A plain mouse hotkey set with an older version of the addon is kept but **doesn't work any
  more**: the tree shows it as `[Mouse 4: off, needs Ctrl/Alt/Shift]` and the editor in red.
  Click **Change...** to set a new one.
- **The key still does its normal job in game.** The addon only listens; it never blocks a key.
  Pick a key you don't use in game. Plain letters and numbers get a warning, because they're
  usually game keys; adding Ctrl, Alt or Shift is safer.
- **GW2's default keys:** if the hotkey is one of the game's default keybinds, the editor says
  which, for example `In GW2's default keys, F1 is "Profession skill 1"`, or Ctrl+T is "Call
  target" and Alt+1 a squad marker. Pressing it would do both. The addon can't read your own
  keybinds, so it only knows the defaults (from the GW2 wiki, PC with a US keyboard): if you've
  changed that key in the game, you can ignore the warning.

<!-- screenshot: hotkey-game-key.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the hotkey row with the yellow "In GW2's default keys" note.*
- Hotkeys don't fire while you're typing in chat or in any text box, or outside gameplay
  (loading screens, character select).
- A hotkey only sends a macro that's **active where you are** (see [Filters](Filters)). So two
  macros can share a key if they're never active in the same place, for example one for WvW and
  one for PvE.
- If a key is set on two macros that are both active, it sends nothing and shows an alert. The
  editor warns you about this ("Also set on: ...").
- If a hotkey send fails, a Nexus alert says why, since the library may be closed, and the
  [error window](Safety-and-Troubleshooting) opens (without taking focus) with the details.

<!-- screenshot: hotkey-set.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: a macro's hotkey row.*

### Cooldown

Under the hotkey, **Cooldown:** sets how many seconds the hotkey waits before it sends that
macro again: 0 (off, the default) up to 600 (10 minutes).

- It counts from when the macro was last sent, **from anywhere**: a hotkey, the quick menu or
  **Send now**.
- **Only the hotkey waits.** Send now and the quick menu always send, because you picked the
  macro on purpose.
- The first press during the cooldown shows one Nexus alert, for example `Say: cooldown, wait
  3 s.`; more presses during the same cooldown are ignored without an alert.
- While it runs, the editor shows how long is left, for example `(12 s left)`.
- If the send failed, there's no cooldown, so you can try again.
- Even with no cooldown, a hotkey ignores presses for **1 second** after its macro was sent, so
  a bouncing key or a double press doesn't send it twice.
- The cooldown is saved with the macro and comes along in exports. The time left isn't saved:
  after a reload, the hotkey works at once.

<!-- screenshot: hotkey-cooldown.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Cooldown row with "(12 s left)".*

## Quick menu

The quick menu shows your library as nested menus, so you can pick a macro with the mouse.

- **Right-click the scroll icon** in the Nexus bar, or
- set a key for **Open Chat Messages quick menu** in Nexus > Options > Keybinds, to open it as a
  pop-up.

It lists only the macros that are active where you are, and hides folders with none. Macros are
coloured by channel, show their hotkey, and preview their lines when you hover over them. The
bottom item opens or hides the library.

### Favourites and recently sent

Above the folders, the quick menu lists:

- **Favourites:** the folders and macros you marked as a favourite, in library order, on
  **every map**, whatever their filters. Mark one with the **Favourite** box (in a macro's editor
  or a folder's details), or right-click it in the library > **Favourite** (click again to
  unmark it). Favourites have a `*` after their name in the library.
  - A favourite that isn't [active where you are](Filters) is **faded**. Clicking it still sends
    it; its hotkey still follows its filters.
  - A favourite **folder** opens as a sub-menu with everything inside it, whatever the filters
    (what isn't active here is faded).
  - You can have up to **10** favourites (folders and macros together). To mark another,
    untick one first: the addon tells you if you try.
- **Recently sent:** the macros you sent last, newest first, from anywhere (a hotkey, the quick
  menu or Send now). A favourite isn't listed here again. It shows 5 by default; set 0 to 10, or
  **Clear the list**, on the addon's options page (General tab), under **Recently sent in the quick
  menu**. The list is kept when you restart the game.

Recently sent only shows macros that are active where you are. Both lists show each item with
its folders in brackets, and deleted macros drop out. A macro you export or import isn't a favourite on the other side:
favourites are your own.

<!-- screenshot: quick-menu-favourites.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the quick menu with Favourites and Recently sent above the folders.*

**Searching the menu:** type in the box at the top to swap the folders for a flat list of the
macros active here that match (by name, line text, whisper recipient or hotkey), each with its
folders in brackets. Click one, or press **Enter** to send the first. When you open the menu with
its key, you can type at once; in the right-click menu, click the box first. The box is empty
again each time the menu opens.

<!-- screenshot: quick-menu-search.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the quick menu with a search typed and its flat result list.*

<!-- screenshot: quick-menu.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the quick menu with a macro's line preview.*

## One at a time

Only one macro is sent at a time. A trigger that comes in while a macro is still being sent is
ignored.

## While a macro is sent

- Don't type or press keys until it's done. If you open chat yourself, click away or press
  Escape, the macro stops with a message.
- Don't hold Shift, Ctrl, Alt or the Windows key: the addon waits briefly for them to be
  released, and otherwise doesn't send.

See [Safety and troubleshooting](Safety-and-Troubleshooting) for the messages you may see.
