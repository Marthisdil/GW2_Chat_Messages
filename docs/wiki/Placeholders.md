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
| `{nexus}` | This addon's name and version, like `Chat Messages 2026.11.21.1412.Dev`. |

<!-- screenshot: placeholders-tooltip.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Placeholders (?) tooltip in the editor.*

## How they're written

- Names are not case-sensitive: `{Time}` works.
- Arguments are separated by spaces. Use quotes for one with spaces: `{txt "my lines.txt"}`.
- Anything else in `{ }` is sent exactly as typed, so `{hello}` stays `{hello}`.
- If a placeholder can't be filled in (a missing file, no internet for `{map}`), **the whole
  macro is not sent**, and the error window says why. Nothing half-finished goes to chat.
- A filled-in value is plain text on one line: line breaks, tabs and other control characters in
  it become spaces, so it can't press Enter or Tab in chat.
- A placeholder can't start a chat command. If its value would put a `/` where chat reads a
  command (at the start of the message or whisper recipient, or right after a command such as
  `/g `), **the macro is not sent**. Say a `{json}` page sends back `/w Someone hi`: in
  `/g {json ...}` that would whisper, so it's refused. Only a `/` you typed yourself can begin a
  command; a `/` later in the message is just text. A `/` you type right before a placeholder
  (`/{txt emotes.txt}`) is your choice to let that value pick the command, so don't do that with a
  `{json}` page or file you don't trust.

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
- Only addresses on the internet. An address that leads to this PC or your local network
  (`localhost`, `127.0.0.1`, `192.168.x.x`, `10.x.x.x` and other private or link-local addresses,
  also after a redirect) is refused, and the macro isn't sent. This keeps a shared macro from
  reading your router or another device and typing the answer into chat.
- Behind a proxy (a WinHTTP proxy, set with `netsh winhttp set proxy`), the addon looks the
  address up itself before sending, and checks every redirect the same way. If your network only
  lets the proxy look names up, that `{json}` is refused, because the address can't be checked.

## Length

The 199-character limit applies to the filled-in text. The editor marks the length of lines with
placeholders with "~", since it can only guess.
