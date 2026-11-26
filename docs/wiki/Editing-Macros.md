# Editing macros

Select a macro in the library to edit it on the right. Changes are saved automatically.

A macro has a **title**, one or more **lines**, an optional **hotkey** (see
[Sending macros](Sending-Macros)), a **Favourite** box (favourites are at the top of the
[quick menu](Sending-Macros#favourites-and-recently-sent)), **where it's active** (see
[Filters](Filters)) and a **note**.

## The editor

From top to bottom:

- **The header:** the macro's name, with **Send now**, **Preview** (see
  [Sending macros](Sending-Macros)) and **...** on the same row. **...** has **Rename**,
  **Duplicate** and **Delete**. Under the name, in grey: the folder it's in, its hotkey and where
  it's active.
- **Lines (n):** one row per line (see [Lines](#lines)), and **Add line** under them.
- Three sections that fold open and closed, each showing a summary when closed:
  **Where it's active** (see [Filters](Filters)), **Hotkey and favourite**, and **Note**. A
  problem shows in yellow even when its section is closed, like "(the hotkey is shared)" or
  "(can't be active anywhere)". The addon remembers which sections you keep open, for every
  macro.
- **Advanced**, at the bottom, only with [Advanced options](Advanced-Options) ticked: Raw text,
  the hotkey cooldown and the text file link.

<!-- screenshot: editor-advanced.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Advanced section of a macro, with Advanced options ticked.*

A folder's details have the same header (with **...** for Rename and Delete), then its level,
what's in it, **Favourite** and **Where it's active**.

## Note

The **Note** section is a box for what the macro is for, or when to use it ("Before each boss,
needs a commander tag"). It's never typed into chat. You see it in grey above the lines when you
hover over the macro in the [quick menu](Sending-Macros#quick-menu), and the library's
[search](The-Macro-Library#search) finds it. A note can have up to 10 lines and 1000 bytes
(about 1000 letters, fewer with accents); it's exported and imported with the macro.

<!-- screenshot: editor-note.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: a macro's editor with a note.*

## Duplicate

Right-click a macro in the library > **Duplicate** to make a variant of it. The copy goes right
below it and is selected, named "Hello (2)" (a copy of "Hello (2)" is "Hello (3)", or the next
free number). It has the same lines, filters, cooldown and note. It has **no hotkey** (a hotkey
belongs to one macro), isn't a favourite, and isn't linked to a text file. Folders can't be
duplicated.

## Lines

Each line is one chat message, sent to its own channel. When the macro is sent, the lines go
out one after another, in order. A macro can have up to 50 lines.

There are two ways to edit lines. Switching between them loses nothing.

### The "Lines" tab

Each line is one row: its number, a channel dropdown, the message box, its
[character count](#the-199-character-limit), arrows to move it up or down, and **X** to remove
it. A whisper line has its **Recipient** box in the row, and a squad line its **Broadcast** box.
In a narrow window the message box gets a row of its own. **Add line** adds one at the end, on
the same channel as the last line.

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

- **Whisper:** type a character or account name in **Recipient** (at most 29 characters; a
  longer recipient gets a red note and the line isn't sent).
- **Squad broadcast:** tick **Broadcast** on a squad line. It's only sent as a broadcast while
  you're the commander; otherwise it's a normal squad message. It needs the Squad Broadcast chat
  key (see [Installation](Installation)).

### The "Raw text" tab

> **Advanced option:** only with **Advanced options** ticked on the addon's options page; see
> [Advanced options](Advanced-Options). Pick it in the editor's **Advanced** section, under
> **Edit the lines as:**.

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

GW2 chat takes at most 199 characters per message, counting the channel command. Next to each
message box a counter shows `23/199`. It turns **yellow from 150** and **bright red from 180**,
so you see the limit coming. A line over 199 gets a red line under it, "Over 199 characters: this
line won't be sent", and is refused. In Raw text the count is next to each row of **Will send**. If a line has
placeholders, the count is shown with "~", because the real length is only known when it's sent.

## Chat links

Waypoint, item and other chat links (like `[&BDAEAAA=]`) work as they are: paste them into the
message, and the game turns them into links when it's sent.

## Text file links

A macro can be kept in step with a `.txt` file, so you can edit it in Notepad while the game
runs. This is an advanced option: it only shows when **Advanced options** is ticked on the
addon's options page. See [Advanced options](Advanced-Options#text-file-links).

## Placeholders

Words in `{ }` like `{time}` or `{map}` are filled in when the message is sent. Hover over
**Placeholders (?)** in the editor for the list, or see [Placeholders](Placeholders).
