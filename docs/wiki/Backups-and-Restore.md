# Backups and restore

The addon can keep copies of your library on a schedule you choose. You can put any copy back
with one click. Both are on the addon's page in **Nexus > Options**, on the **Backups** and
**Restore** tabs. Everything that was on that page before is now on the **General** tab.

## The Backups tab

- **Make backups of my macros** turns backups on. It's off by default (see
  [While backups are off](#while-backups-are-off)).
- **Back up every** sets how often: a number of **hours** (up to 48), **days** (up to 30),
  **weeks** (up to 8) or **months** (up to 12). Months are calendar months, so a backup made on
  31 January is followed by one on 28 or 29 February.
- **Copies to keep** is from 1 to 50. When there are more, the oldest ones are deleted at the
  next backup.
- **Back up now** makes a backup at once, even if nothing changed. It counts as one of the
  copies you keep.
- **Open the backups folder** opens it in Explorer.

The tab also says when the last backup was made.

<!-- screenshot: options-backups.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Backups tab, with backups on.*

### The files

Backups go into `<GW2>\addons\GW2_Chat_Messages\backups\`. The addon creates this folder when it
loads. Each file is named with the date and time of the backup in **UTC**:

```
macros_backup_2026-11-23_21-46-05_UTC.json    a backup (on your schedule, or Back up now)
macros_safety_2026-11-30_21-46-05_UTC.json    a weekly safety copy (while backups are off)
```

Each one is a complete `macros.json`, including the trash bin. You can also bring one in with
the Import window's **A whole library** (see [Import and export](Import-and-Export)), which adds
it as a new folder instead of replacing your library.

- **A backup is only made when your macros changed** since the last one, so an unchanged
  library doesn't push out your older copies.
- The addon checks the schedule once a minute while the game runs. If a backup came due while
  the game was closed, it's made soon after the game starts.
- The addon only ever deletes files whose names have exactly this form. Anything else you put
  in the folder is never listed or deleted.

## While backups are off

If you leave backups off, the addon still quietly makes a **safety copy** of your macros once a
week, in case `macros.json` is ever damaged. It keeps the last **4** and deletes the oldest when
it makes a new one. The Backups tab says this while backups are off.

Safety copies and backups are counted separately. Turning backups off never deletes your
backups, and turning them on never deletes your safety copies.

## The Restore tab

The Restore tab lists every backup and safety copy in the folder, newest first, with these
columns:

| Column | What it shows |
|---|---|
| **Date (UTC)**, **Time (UTC)** | When the copy was made. Hover the date to see the file name. |
| **Folders**, **Macros** | How many folders and macros it holds (not counting the trash bin). |
| **Type** | **Backup** or **Safety copy**. |

A file that can't be read shows **Can't be read** (hover it to see why), and has no Restore
button. **Refresh** reads the folder again, for example after you copy a file into it.

**Restore...** asks first, then replaces your **whole library**, and the trash bin too, with the
one in the copy. The library window opens and says which backup was restored. Until you close
the game, **Undo restore** there puts your library back as it was. After the restore, your
previous `macros.json` is also kept as `macros.json.bak` (see [Files and data](Files-and-Data)).

Macros linked to a text file (see [Editing macros](Editing-Macros)) take that file's text after
a restore, just as when the game starts. A restore never writes old lines into your text files.

<!-- screenshot: options-restore.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Restore tab, with a few backups and a safety copy listed.*
