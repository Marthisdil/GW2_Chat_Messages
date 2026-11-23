# Screenshots

The screenshots the README and the wiki pages need. Each one has a placeholder in those pages,
marked with `<!-- screenshot: file-name.png -->` and a line "Screenshot to come".

## Where to put them

Save each screenshot as a PNG with exactly the file name below, in:

```
C:\Dropbox\~ Github\GW2_Chat_Messages\docs\images\
```

At each build, Claude checks that folder. Each new file replaces its placeholders with the
image, and its row below is marked **Captured**. Nothing in the folder is overwritten.

The list is kept up to date with every change: new screens get a row, and a row whose screen has
changed gets a new description (and goes back to **Needed** if its picture is out of date).

## Tips

- **Crop to the addon's window** (or the menu or dialog), with a little of the game around it.
  Win+Shift+S (Snipping Tool) works well with the game in windowed or borderless mode.
- **Hide names:** blur or crop out character names, account names and guild tags you don't want
  shown, especially in chat screenshots.
- Use made-up example macros (like the ones suggested below), not private ones.
- Keep the game's default UI size, the Nexus default font and the addon's **Text size** on
  Default (except for `text-size.png`), so the pictures match what most players see.
- PNG, ideally under 1 MB each.

## The list

| # | File name | What to capture | Used in | Status |
|---|---|---|---|---|
| 1 | `library-window.png` | The whole library window: a few folders (e.g. *Raids › Wing 3*, *WvW*, *Guild*) with one expanded, and a macro selected so the editor shows on the right. The empty **Search (Ctrl+F)** box and the unticked **Only active in ...** checkbox show above the tree. | README, wiki Home, The macro library | Needed |
| 2 | `chat-result.png` | The game's chat box right after sending a multi-line macro, showing two or three lines in different channels (e.g. squad, party, map). | README | Needed |
| 3 | `quick-menu.png` | The quick menu open from right-clicking the scroll icon: nested folders, with the mouse over a macro so its line preview shows. | README, Sending macros | Needed |
| 4 | `nexus-quick-access.png` | The Nexus Quick Access bar with the scroll icon, mouse over it so the tooltip shows. A small crop is fine. | Installation | Needed |
| 5 | `nexus-game-keybinds.png` | Nexus > Options > Game Keybinds, showing the **Chat** (and **Squad Broadcast**) keys set. | Installation | Needed |
| 6 | `tree-context-menu.png` | Right-click menu on a folder in the library tree (shows Export, Import into this folder, Sort...). | The macro library | Needed |
| 7 | `macro-editor-lines.png` | The macro editor's **Lines** tab with 3-4 lines: a say line, a whisper with a recipient, a squad line with **Broadcast** ticked, and the character counters under the message boxes (ideally one long line with a yellow counter). | Editing macros | Needed |
| 8 | `macro-editor-raw.png` | The **Raw text** tab of the same macro, with the **Will send:** list under it. | Editing macros | Needed |
| 9 | `send-status.png` | The editor after **Send now**, with the status line ("Sent 3 lines." or "Sending line 2 of 3..."). A successful send: errors now show in the error window instead. | Sending macros | Needed |
| 10 | `hotkey-set.png` | The **Hotkey:** row of a macro with a hotkey set (e.g. Ctrl+F1), ideally showing the "Also set on" or plain-key warning. | Sending macros | Needed |
| 11 | `filters.png` | **Where it's active** opened: game modes ticked, two or three maps listed by name, and the "Active here" line. | Filters | Needed |
| 12 | `placeholders-tooltip.png` | The tooltip from hovering **Placeholders (?)** in the editor. | Placeholders | Needed |
| 13 | `export-menu.png` | Right-click a macro > **Export** submenu open. | Import and export | Needed |
| 14 | `import-window.png` | The Import window with something loaded: the target folder, the options, **Select all / Deselect all**, and the preview with its checkboxes (one item unticked), at least one **NAME TAKEN** item and its choice box, a macro marked **READS A FILE**, and (with **Keep hotkeys** ticked) one marked **YOUR HOTKEY** with its notice above the preview. | Import and export | Needed |
| 15 | `send-alert.png` | A Nexus alert on screen from a hotkey that couldn't send, with the error window it also opens. Easiest: give two macros that are both active here the same hotkey, then press it with the library closed. | Safety and troubleshooting | Needed |
| 16 | `link-file.png` | A macro's **Text file link** section opened, with a file linked ("In step") and the Open file / Open folder / Unlink buttons. | Editing macros | Needed |
| 17 | `text-size.png` | The library window at the biggest text size, with the **Text size** dropdown (end of the toolbar) open. | The macro library | Needed |
| 18 | `error-window.png` | The error window with "What you did" and the OK / Copy to clipboard buttons. Easiest: toolbar **Export all** with an empty library, or select a folder 6 levels deep and click **New folder**. (A duplicate name shows it as a pop-up over the name box, which works too.) | Safety and troubleshooting | Needed |
| 19 | `trash-bin.png` | The library with the **Trash bin (F/M)** opened at the bottom of the tree (two or three items, one a folder), one item selected so the details pane shows **Restore** / **Delete permanently...**. | The macro library | Needed |
| 20 | `search.png` | The library tree while searching: a word typed in the **Search** box (e.g. `stack`), the "Found: ..." line, an opened folder path with a highlighted match, and one macro marked "(found in its lines)". Crop to the tree pane. | The macro library | Needed |
| 21 | `import-library.png` | The Import window with **A whole library** chosen and a file like `Alex.json` loaded: step 2's folder name (`From Alex`), **Keep hotkeys** unticked, and the preview with the new folder holding their folders and macros. | Import and export | Needed |
| 22 | `quick-menu-search.png` | The quick menu pop-up (opened by its key) with `say` or another word typed in its search box: the flat result list with each macro's folders in brackets, the mouse over one so its preview shows. | Sending macros | Needed |
| 23 | `language-options.png` | The addon's page in Nexus > Options with the **Language** dropdown open: **Follow Nexus (...)**, English (US), Deutsch, Español, Français with their percentages. | Languages | Needed |
| 24 | `library-german.png` | The library window in German (pick **Deutsch**), with a macro selected so the editor shows. | Languages | Needed |
