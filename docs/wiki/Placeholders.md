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
| `{txt file}` | A random line from a text file, like `{txt jokes.txt}`. |
| `{txt file line}` | That line of the file (1 = the first), like `{txt rules.txt 3}`. |
| `{map}` | The name of the map you're on, like `Lion's Arch`. |
| `{json path url}` | One value from a JSON web page (see below). |
| `{nexus}` | This addon's name and version, like `Chat Messages v0.1.0`. |

<!-- screenshot: placeholders-tooltip.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Placeholders (?) tooltip in the editor.*

## How they're written

- Names are not case-sensitive: `{Time}` works.
- Arguments are separated by spaces. Use quotes for one with spaces: `{txt "my lines.txt"}`.
- Anything else in `{ }` is sent exactly as typed, so `{hello}` stays `{hello}`.
- If a placeholder can't be filled in (a missing file, no internet for `{map}`), **the whole
  macro is not sent**, and the error window says why. Nothing half-finished goes to chat.

## Text files

- Put text files in `<GW2>\addons\GW2_Chat_Messages\`, or a folder inside it:
  `{txt "raid/callouts.txt"}`.
- Files elsewhere on your PC can't be used. This keeps a shared macro from typing out your
  other files.
- UTF-8 text, at most 1 MB. `{txt file}` skips blank lines.
- The text from a file is sent as it is: placeholders inside it are not filled in.

## `{map}`

The map's name comes from the official GW2 API and is saved on your PC, so each map is only
looked up once. The addon looks up the map you're on in the background as soon as you arrive.
If you've never been online on that map, `{map}` needs an internet connection.

## `{json path url}`

Sends one value from a JSON web page. For example, the current game build:

```
{json id https://api.guildwars2.com/v2/build}
```

- The **path** picks the value, with dots between the steps. Numbers pick entries in a list:
  `data.0.name` is the `name` of the first entry of `data`.
- The value must be text, a number or true/false. Lists, objects and empty values are refused.
- Only `http://` and `https://` addresses, pages up to 512 KB, and a short time limit.

## Length

The 199-character limit applies to the filled-in text. The editor marks the length of lines with
placeholders with "~", since it can only guess.
