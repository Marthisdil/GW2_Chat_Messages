# Editing macros

Select a macro in the library to edit it on the right. Changes are saved automatically.

A macro has a **title**, one or more **lines**, an optional **hotkey** (see
[Sending macros](Sending-Macros)), a **Favourite** box (favourites are at the top of the
[quick menu](Sending-Macros#favourites-and-recently-sent)) and **where it's active** (see
[Filters](Filters)).

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

- **Whisper:** type a character or account name in **Recipient** (at most 29 characters; a
  longer recipient gets a red note and the line isn't sent).
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

GW2 chat takes at most 199 characters per message, counting the channel command. Under each
message box a counter shows `23/199 characters`. It turns **yellow from 150** and **bright red
from 180**, so you see the limit coming. A line over 199 says "too long, this line won't be sent"
and is refused. In Raw text the count is next to each row of **Will send**. If a line has
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
