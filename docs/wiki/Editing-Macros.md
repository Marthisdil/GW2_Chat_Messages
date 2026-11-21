# Editing macros

Select a macro in the library to edit it on the right. Changes are saved automatically.

A macro has a **title**, one or more **lines**, an optional **hotkey** (see
[Sending macros](Sending-Macros)) and **where it's active** (see [Filters](Filters)).

## Lines

Each line is one chat message, sent to its own channel. When the macro is sent, the lines go
out one after another, in order. A macro can have up to 50 lines.

There are two ways to edit lines. Switching between them loses nothing.

### The "Lines" tab

Each line has a channel dropdown and a message box, with arrow buttons to change its order and a **Remove** button.
**Add line** adds one at the end.

<!-- screenshot: macro-editor-lines.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Lines tab.*

| Channel | Command | Notes |
|---|---|---|
| Current | (none) | Whichever channel chat opens on. Game commands like `/wave` go here. |
| Say | `/s` | |
| Map | `/m` | |
| Party | `/p` | |
| Squad | `/d` | Has a **Broadcast** box, see below. |
| Team | `/t` | PvP and WvW team chat. |
| Guild | `/g` | Your represented guild. |
| Guild 1–6 | `/g1` … `/g6` | A guild slot. |
| Whisper | `/w` | Has a **Recipient** box. |
| Reply | `/r` | Replies to the last whisper. |
| Emote | `/e` | |

- **Whisper:** type a character or account name in **Recipient**.
- **Squad broadcast:** tick **Broadcast** on a squad line. It's only sent as a broadcast while
  you're the commander; otherwise it's a normal squad message. It needs the Squad Broadcast chat
  key (see [Installation](Installation)).

### The "Raw text" tab

Type one message per row, the way you'd type it in game:

```
/s Hello everyone!
/w [Some Name] Thanks for the help.
/d ! Stack on the commander
/g2 Guild missions in 5 minutes
Just a line in the current channel
```

<!-- screenshot: macro-editor-raw.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Raw text tab and its "Will send" list.*

- `/w [Name] message` whispers to Name.
- `/d ! message` is a squad broadcast.
- A row without a known channel command goes to the current channel, as typed.
- German, French and Spanish commands work too (`/sagen`, `/karte`, `/dire`, `/carte`, `/mapa`,
  `/gruppe`, `/groupe`, `/grupo`, ...).
- Below the box, **Will send:** shows how the rows were understood.

## The 199-character limit

GW2 chat takes at most 199 characters per message, counting the channel command. The editor
shows the count for every line, and a line that's too long is refused. If a line has
placeholders, the count is shown with "~", because the real length is only known when it's sent.

## Chat links

Waypoint, item and other chat links (like `[&BDAEAAA=]`) work as they are: paste them into the
message, and the game turns them into links when it's sent.

## Text file links

A macro can be kept in step with a `.txt` file, so you can edit it in Notepad (or any text
editor) while the game runs. The file uses the same format as the Raw text tab: one chat line per
row.

Open **Text file link** in the macro editor, type a file name and click **Link**:

- A name alone, like `callouts.txt`, goes in `<GW2>\addons\GW2_Chat_Messages\links\`. A full path,
  like `C:\Users\Me\Documents\raid.txt`, works too. The name must end in `.txt`.
- If the file already has lines, they **replace** the macro's lines. If it doesn't exist (or is
  empty), it's created from the macro's lines.

From then on, changes go both ways and show up within a second:

- Save the file in your editor: the macro gets its lines.
- Edit the macro in the addon: the file is rewritten.
- If both changed at once, the file wins.
- While the game is closed you can edit the file; the macro picks it up when the addon loads.

**Open file** opens it in your text editor, **Open folder** shows it in Explorer, and **Unlink**
stops the syncing (neither the macro nor the file is deleted). If the file is deleted or moved,
the editor says so; the macro keeps its lines, and **Recreate the file** writes it again. A file
can hold up to 50 lines for one macro, and at most 256 KB.

Two macros can't be linked to the same file. Links aren't included in exports.

<!-- screenshot: link-file.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Text file link section of a linked macro.*

## Placeholders

Words in `{ }` like `{time}` or `{map}` are filled in when the message is sent. Hover over
**Placeholders (?)** in the editor for the list, or see [Placeholders](Placeholders).
