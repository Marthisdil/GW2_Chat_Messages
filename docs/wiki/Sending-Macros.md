# Sending macros

There are three ways to send a macro. All of them type the lines into chat the same way.

## Send now

**Send now** in the macro editor (or in a macro's right-click menu) sends it straight away.
It always works, even where the macro's [filters](Filters) say it isn't active.

The editor shows the progress ("Preparing...", "Sending line 2 of 3...") and the result.

<!-- screenshot: send-status.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the status line after Send now.*

## Hotkeys

Each macro can have its own hotkey. In the macro editor, next to **Hotkey:**, click **Set...**
(or **Change...**) and press the key you want.

- You can add **Ctrl**, **Alt** or **Shift**. **Mouse 3** (middle), **Mouse 4** and **Mouse 5**
  work too, but only together with Ctrl, Alt or Shift (for example **Ctrl+Mouse 4**): on their
  own they're often dodge or a skill, so they'd send the macro by accident.
- Escape cancels. **Clear** removes the hotkey.
- Not allowed: left and right mouse buttons, Escape, a modifier key on its own, plain Mouse 3/4/5,
  and plain Tab, Enter, Space or Backspace.
- A plain mouse hotkey set with an older version of the addon is kept but **doesn't work any
  more**: the tree shows it as `[Mouse 4: off, needs Ctrl/Alt/Shift]` and the editor in red.
  Click **Change...** to set a new one.
- **The key still does its normal job in game.** The addon only listens; it never blocks a key.
  Pick a key you don't use in game. Plain letters and numbers get a warning, because they're
  usually game keys; adding Ctrl, Alt or Shift is safer.
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

## Quick menu

The quick menu shows your library as nested menus, so you can pick a macro with the mouse.

- **Right-click the scroll icon** in the Nexus bar, or
- set a key for **Open Chat Messages quick menu** in Nexus > Options > Keybinds, to open it as a
  pop-up.

It lists only the macros that are active where you are, and hides folders with none. Macros are
coloured by channel, show their hotkey, and preview their lines when you hover over them. The
bottom item opens or hides the library.

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
