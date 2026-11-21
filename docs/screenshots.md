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

## Tips

- **Crop to the addon's window** (or the menu or dialog), with a little of the game around it.
  Win+Shift+S (Snipping Tool) works well with the game in windowed or borderless mode.
- **Hide names:** blur or crop out character names, account names and guild tags you don't want
  shown, especially in chat screenshots.
- Use made-up example macros (like the ones suggested below), not private ones.
- Keep the game's default UI size and the Nexus default font, so the pictures match what most
  players see.
- PNG, ideally under 1 MB each.

## The list

| # | File name | What to capture | Used in | Status |
|---|---|---|---|---|
| 1 | `library-window.png` | The whole library window: a few folders (e.g. *Raids › Wing 3*, *WvW*, *Guild*) with one expanded, and a macro selected so the editor shows on the right. | README, wiki Home, The macro library | Needed |
| 2 | `chat-result.png` | The game's chat box right after sending a multi-line macro, showing two or three lines in different channels (e.g. squad, party, map). | README | Needed |
| 3 | `quick-menu.png` | The quick menu open from right-clicking the scroll icon: nested folders, with the mouse over a macro so its line preview shows. | README, Sending macros | Needed |
| 4 | `nexus-quick-access.png` | The Nexus Quick Access bar with the scroll icon, mouse over it so the tooltip shows. A small crop is fine. | Installation | Needed |
| 5 | `nexus-game-keybinds.png` | Nexus > Options > Game Keybinds, showing the **Chat** (and **Squad Broadcast**) keys set. | Installation | Needed |
| 6 | `tree-context-menu.png` | Right-click menu on a folder in the library tree (shows Export, Import into this folder, Sort...). | The macro library | Needed |
| 7 | `macro-editor-lines.png` | The macro editor's **Lines** tab with 3-4 lines: a say line, a whisper with a recipient, a squad line with **Broadcast** ticked, and the character counts. | Editing macros | Needed |
| 8 | `macro-editor-raw.png` | The **Raw text** tab of the same macro, with the **Will send:** list under it. | Editing macros | Needed |
| 9 | `send-status.png` | The editor after **Send now**, with the status line ("Sent 3 lines." or "Sending line 2 of 3..."). | Sending macros | Needed |
| 10 | `hotkey-set.png` | The **Hotkey:** row of a macro with a hotkey set (e.g. Ctrl+F1), ideally showing the "Also set on" or plain-key warning. | Sending macros | Needed |
| 11 | `filters.png` | **Where it's active** opened: game modes ticked, two or three maps listed by name, and the "Active here" line. | Filters | Needed |
| 12 | `placeholders-tooltip.png` | The tooltip from hovering **Placeholders (?)** in the editor. | Placeholders | Needed |
| 13 | `export-menu.png` | Right-click a macro > **Export** submenu open. | Import and export | Needed |
| 14 | `import-window.png` | The Import window with something loaded: the target folder, the options, and the preview with at least one taken name and its choice box. | Import and export | Needed |
| 15 | `send-alert.png` | A Nexus alert on screen from a hotkey that couldn't send. Easiest: give two macros that are both active here the same hotkey, then press it. | Safety and troubleshooting | Needed |
