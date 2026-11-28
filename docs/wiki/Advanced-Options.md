# Advanced options

Some features are useful to a few players and only get in the way of everyone else. They're kept
behind one checkbox, **Advanced options**, on the addon's options page (Nexus > Options, the Chat
Messages page, **General** tab). The **[?]** next to it opens this page.

<!-- screenshot: options-advanced.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Advanced options checkbox on the options page.*

## What the checkbox does

- **On:** the advanced features show and work like any other.
- **Off** (the default): they're hidden, and they don't work. Nothing you set with them is
  deleted, so turning the box back on brings everything back where it left off. Each feature
  below says what "doesn't work" means for it. Under the checkbox, the options page also counts
  your macros that use one (linked to a text file, or with a cooldown).

A macro linked to a text file can't be sent at all while the box is off:
- its hotkey does nothing (a Nexus alert says why), it's faded and can't be clicked in the quick
  menu, and **Send now** explains instead of sending;
- it's dimmed in the library, and its editor says why it can't be sent;
- it keeps all its settings. Its hotkey is kept too, and still counts as in use: if another macro
  active in the same place has the same key, the key sends nothing there (as the editor warns).

<!-- screenshot: advanced-off-macro.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: a linked macro in the library while Advanced options is off.*

Turn the box back on and everything works again where it left off.

## Why some features are hidden

The addon is meant to be easy to pick up: make a macro, give it a hotkey, send it. Every extra
section in the macro editor is one more thing to read and understand, and a few features also
behave in ways that can surprise you if you didn't set them up on purpose (a text file that
changes a macro while you play, for example). Keeping them behind one box means:

- new players see a short, simple editor;
- nothing advanced happens unless you asked for it;
- players who want more turn on one box and get all of it.

## Features behind the checkbox

| Feature | While the box is off | Described in |
|---|---|---|
| **Text file links**: keep a macro in step with a `.txt` file | Hidden. A linked macro can't be sent, and isn't kept in step with its file. | [below](#text-file-links) |
| **Cooldown**: how long a macro's hotkey waits before it sends again | Hidden and ignored: the hotkey only waits the usual 1 second. The editor shows the cooldown greyed out, and keeps it. | [Sending macros](Sending-Macros#cooldown) |
| **Raw text** tab: edit a macro's lines as plain text | Hidden: the editor shows the lines only. | [Editing macros](Editing-Macros#the-raw-text-tab) |
| Import **A whole library**: someone else's `macros.json` into a folder of its own | Hidden: the Import window offers **Macros** and **From Blish HUD** only. | [Import and export](Import-and-Export#importing-a-whole-library) |
| Import **Keep hotkeys** | Hidden and off: imported macros come without hotkeys. | [Import and export](Import-and-Export#2-choose-where-it-goes) |
| **Refresh map list** button | Hidden. The map list still refreshes by itself (first run, weekly, on a language change). | [Filters](Filters#the-map-list) |

Your own language files used to be behind this box too. They now have their own checkbox under
the Language dropdown, so you can use a language file without the other advanced features (see
[Languages](Languages#more-languages)).

Features added later that fit here (for example timed or scheduled lines) will be behind the
checkbox too, and listed here when they're built.

## Text file links

A macro can be kept in step with a `.txt` file, so you can edit it in Notepad (or any text
editor) while the game runs. The file uses the same format as the Raw text tab: one chat line per
row.

Tick **Advanced options** first. Then open **Advanced** at the bottom of the macro editor; under
**Text file link: none**, type a file name and click **Link**:

- A name alone, like `callouts.txt`, goes in `<GW2>\addons\GW2_Chat_Messages\links\`. A full path,
  like `C:\Users\Me\Documents\raid.txt`, works too. The name must end in `.txt`.
- If the file already has lines, they **replace** the macro's lines. If it doesn't exist (or is
  empty), it's created from the macro's lines.

From then on, changes go both ways and show up within a second:

- Save the file in your editor: the macro gets its lines.
- Edit the macro in the addon: the file is rewritten.
- If both changed at once, the file wins.
- While the game is closed you can edit the file; the macro picks it up when the addon loads.
- While **Advanced options** is off, nothing is kept in step. Turn it back on and the newer
  side wins as above: a file you edited meanwhile replaces the macro's lines, otherwise the
  macro's lines are written to the file. After a restart of the game the file wins, as at every
  start.

**Open file** opens it in your text editor, **Open folder** shows it in Explorer, and **Unlink**
stops the syncing (neither the macro nor the file is deleted). If the file is deleted or moved,
the editor says so; the macro keeps its lines, and **Recreate the file** writes it again. A file
can hold up to 50 lines for one macro, and at most 256 KB.

Save the file as UTF-8 (Notepad's default). In a file saved another way (ANSI, say), characters
like `ä` or `é` can't be read: the macro gets `�` in their place (the font may show `?`). The file
itself is left as it is until you edit the macro in the addon, which writes it back as UTF-8, with
the `�`. Each `�` takes three bytes, so a file that would be over 256 KB that way isn't read (the
editor says why). A macro too big for its file isn't written to it either; the editor says so.

Two macros can't be linked to the same file. Links aren't included in exports.

<!-- screenshot: link-file.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Text file link section of a linked macro.*
