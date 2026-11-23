# Files and data

Everything the addon saves is in `<GW2>\addons\GW2_Chat_Messages\`. The addon creates this folder
when it loads.

| File | What it is |
|---|---|
| `macros.json` | Your library: folders, macros, hotkeys, filters, and the trash bin. |
| `macros.json.bak` | The previous version of `macros.json`, kept on every save. |
| `settings.json` | Addon settings: the text size, the language, whether the trash bin is on, and the backup schedule. Safe to delete (back to the defaults). |
| `maps.json` | Map names and regions from the GW2 API (in the addon's language), the API's full list of map ids for the map search, and the maps you've been to. Safe to delete: it's fetched again (only the "Maps you've been to" list starts over). |
| `exports\` | Files saved by Export. Created when the addon loads; **Exports folder** in the toolbar opens it. |
| `imports\` | Put files here to see them in the Import window. Created when the addon loads; **Open imports folder** in the Import window opens it. |
| `backups\` | Copies of your library, named with the date and time in UTC (see [Backups and restore](Backups-and-Restore)). Created when the addon loads. |
| `links\` | Text files linked to macros (when you give just a file name). |
| `localization\` | Extra language files (see [Languages](Languages)). |

## Saving

- Changes are saved automatically, a moment after you make them, and when the addon unloads.
- Saving is safe against crashes: the new file is written first and then swapped in, and the
  old one is kept as `macros.json.bak`.
- If saving fails, the library window says so and keeps retrying. Check the Nexus log for the
  reason.

## If `macros.json` can't be read

The addon never overwrites a file it can't read. It renames it aside (so you can fix or recover
it) and starts with an empty library. Your `macros.json.bak` may hold the previous version, and
the **Restore** tab lists your backups (see [Backups and restore](Backups-and-Restore)).

## Backups and moving to another PC

The addon keeps backups in `backups\` on a schedule you choose, or a weekly safety copy if you
leave them off (see [Backups and restore](Backups-and-Restore)).

To move to another PC, copy the whole `GW2_Chat_Messages` folder, or use **Export all** and import the file on the other PC
(see [Import and export](Import-and-Export)).

## Internet use

The addon only goes online for:

- map names, from `https://api.guildwars2.com/v2/maps` (no API key, nothing about you is sent):
  the map you arrive on, when its name isn't saved yet, and the full map list (about 10 short
  requests) on the first run, once a week, when you change the language, and when you click
  **Refresh map list**.

No macro can make it go anywhere else.
