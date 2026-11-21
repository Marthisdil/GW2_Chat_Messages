# Files and data

Everything the addon saves is in `<GW2>\addons\GW2_Chat_Messages\`. The addon creates this folder
when it loads.

| File | What it is |
|---|---|
| `macros.json` | Your library: folders, macros, hotkeys, filters, and the trash bin. |
| `macros.json.bak` | The previous version of `macros.json`, kept on every save. |
| `settings.json` | Addon settings: the text size and whether the trash bin is on. Safe to delete (back to the defaults). |
| `maps.json` | Map names from the GW2 API, so they're only looked up once. Safe to delete. |
| your `.txt` files | Text files for the `{txt}` placeholder. |
| `exports\` | Files saved by Export. |
| `imports\` | Put files here to see them in the Import window. |
| `links\` | Text files linked to macros (when you give just a file name). |

## Saving

- Changes are saved automatically, a moment after you make them, and when the addon unloads.
- Saving is safe against crashes: the new file is written first and then swapped in, and the
  old one is kept as `macros.json.bak`.
- If saving fails, the library window says so and keeps retrying. Check the Nexus log for the
  reason.

## If `macros.json` can't be read

The addon never overwrites a file it can't read. It renames it aside (so you can fix or recover
it) and starts with an empty library. Your `macros.json.bak` may hold the previous version.

## Backups and moving to another PC

Copy the whole `GW2_Chat_Messages` folder, or use **Export all** and import the file on the other PC
(see [Import and export](Import-and-Export)).

## Internet use

The addon only goes online for:

- map names, from `https://api.guildwars2.com/v2/maps` (no API key, nothing about you is sent),
- `{json}` placeholders, to the address you wrote in the macro.
