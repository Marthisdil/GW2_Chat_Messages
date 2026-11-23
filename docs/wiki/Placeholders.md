# Placeholders

Placeholders are words in `{ }` that are filled in when a message is sent. They work in messages
and in whisper recipients.

## The list

| Placeholder | Becomes |
|---|---|
| `{time}` | The time now, like `21:05`. |
| `{hour}` | The hour, like `21`. |
| `{min}` | The minutes, like `05`. |
| `{today}` | The date, like `Friday, 20.11.2026` (weekday in your Windows language). |
| `{random}` | A random number from 0 to 100. |
| `{random max}` | A random number from 0 to max, like `{random 6}`. |
| `{random min max}` | A random number from min to max, both included, like `{random 1 20}`. |
| `{map}` | The name of the map you're on, like `Lion's Arch`. |
| `{nexus}` | This addon's name and version, like `Chat Messages 2026.11.21.1412.Dev`. |

<!-- screenshot: placeholders-tooltip.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Placeholders (?) tooltip in the editor.*

## How they're written

- Names are not case-sensitive: `{Time}` works.
- Arguments are separated by spaces: `{random 1 20}`.
- Anything else in `{ }` is sent exactly as typed, so `{hello}` stays `{hello}`.
- If a placeholder can't be filled in (a wrong number in `{random}`, no internet for `{map}`),
  **the whole macro is not sent**, and the error window says why. Nothing half-finished goes to
  chat.
- A filled-in value is plain text on one line: line breaks, tabs and other control characters in
  it become spaces, so it can't press Enter or Tab in chat.
- A placeholder can't start a chat command. If its value would put a `/` where chat reads a
  command (at the start of the message or whisper recipient, or right after a command such as
  `/g `), **the macro is not sent**. Only a `/` you typed yourself can begin a command; a `/`
  later in the message is just text.

## `{txt}` and `{json}` were removed

Earlier builds had `{txt file}` (a line from a text file) and `{json path url}` (a value from a
web page). They're gone: a shared macro could use them to type out a file from your PC or to
make it fetch a web address. A line that still has one is **not sent**: the editor shows a red
note under it, the error window says which placeholder it is, and the Import window marks such
macros **REMOVED PLACEHOLDER** (see [Import and export](Import-and-Export)). Edit the line to
remove it. Other words in `{ }` are still sent as typed.

## `{map}`

The map's name comes from the official GW2 API and is saved on your PC, so each map is only
looked up once. The addon looks up the map you're on in the background as soon as you arrive.
If you've never been online on that map, `{map}` needs an internet connection.

## Length

The 199-character limit applies to the filled-in text. The editor marks the length of lines with
placeholders with "~", since it can only guess.
