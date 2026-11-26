# Advanced options

Some features are useful to a few players and only get in the way of everyone else. They're kept
behind one checkbox, **Advanced options**, on the addon's options page (Nexus > Options, the Chat
Messages page, **General** tab). The **[?]** next to it opens this page.

<!-- screenshot: options-advanced.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Advanced options checkbox on the options page.*

## What the checkbox does

- **On:** the advanced features show in the library and work like any other.
- **Off** (the default for a new install): they're hidden, and they don't work. A macro that
  uses one:
  - can't be sent: its hotkey does nothing (a Nexus alert says why), it's faded and can't be
    clicked in the quick menu, and **Send now** explains instead of sending;
  - is dimmed in the library, and its editor says why it can't be sent;
  - keeps all its settings, so nothing is lost. Its hotkey is kept too, and still counts when
    the editor warns that another macro uses the same key.

<!-- screenshot: advanced-off-macro.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: a linked macro in the library while Advanced options is off.*

Turn the box back on and everything works again where it left off.

If you used an advanced feature before this checkbox existed (a macro linked to a text file),
the box starts ticked, so nothing stops working after an update.

## Why some features are hidden

The addon is meant to be easy to pick up: make a macro, give it a hotkey, send it. Every extra
section in the macro editor is one more thing to read and understand, and a few features also
behave in ways that can surprise you if you didn't set them up on purpose (a text file that
changes a macro while you play, for example). Keeping them behind one box means:

- new players see a short, simple editor;
- nothing advanced happens unless you asked for it;
- players who want more turn on one box and get all of it.

## Features behind the checkbox

- [Text file links](#text-file-links): keep a macro in step with a `.txt` file.

## Text file links

A macro can be kept in step with a `.txt` file, so you can edit it in Notepad (or any text
editor) while the game runs. The file uses the same format as the Raw text tab: one chat line per
row.

Tick **Advanced options** first. Then open **Text file link** in the macro editor, type a file
name and click **Link**:

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
