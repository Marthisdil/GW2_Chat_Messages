# Screenshots

The screenshots the README and the wiki pages need. Each one has a placeholder in those pages,
marked with `<!-- screenshot: file-name.png -->` and a line "Screenshot to come".

## Where to put them

Save each screenshot as a PNG with exactly the file name below, in:

```
C:\Dropbox\~ Github\GW2_Chat_Messages\docs\images\
```

At each build, Claude checks that folder. Each new file replaces its placeholders with the
image, and its row is marked **Captured** and moves from the list to [Done](#done) at the bottom.
Nothing in the folder is overwritten. So the list only shows pictures still to take, and
[Retakes](#retakes) the ones to redo.

The list is kept up to date with every change: new screens get a row, and a row whose screen has
changed gets a new description (and goes back to **Needed**, moving from Done to the list, if
its picture is out of date).

## Tips

- **Crop to the addon's window** (or the menu or dialog), with a little of the game around it.
  Win+Shift+S (Snipping Tool) works well with the game in windowed or borderless mode.
- **Hide names:** blur or crop out character names, account names and guild tags you don't want
  shown, especially in chat screenshots.
- Use made-up example macros (like the ones suggested below), not private ones.
- **Advanced options** (options page, General tab) is off on a new install, and most pictures
  are taken that way. A row that needs it ticked says so.
- Keep the game's default UI size, the Nexus default font, the addon's **Text size** on Default
  (except for `text-size.png`) and its **Language** on English (except for `library-german.png`),
  so the pictures match what most players see.
- Several pictures can come from one setup: the macros from the in-game tests (`docs/in-game-tests.md`,
  A3 and J10) give `library-window.png`, `macro-editor-*.png`, `hotkey-set.png` and
  `import-window.png`.
- PNG, ideally under 1 MB each.

## The list

| # | File name | What to capture | Used in | Status |
|---|---|---|---|---|
| 1 | `library-window.png` | The whole library window: a few folders (e.g. *Raids › Wing 3*, *WvW*, *Guild*) with one expanded, and a macro selected so the editor shows on the right (with **Advanced options** off, so no **Cooldown** row, **Lines / Raw text** choice or **Text file link** section). The empty **Search (Ctrl+F)** box and the unticked **Only active in ...** checkbox show above the tree. | README, wiki Home, The macro library | Captured (2026-11-25), retake |
| 2 | `chat-result.png` | The game's chat box right after sending a multi-line macro, showing two or three lines in different channels (e.g. squad, party, map). | README | Captured (2026-11-25), retake |
| 3 | `quick-menu.png` | The quick menu open from right-clicking the chat bubble icon: the empty search box at the top, nested folders under it, and the mouse over a macro so its line preview shows. | README, Sending macros | Captured (2026-11-25), retake |
| 5 | `nexus-game-keybinds.png` | Nexus > Options > Game Keybinds, showing the **Chat** (and **Squad Broadcast**) keys set. | Installation | Needed |
| 6 | `tree-context-menu.png` | Right-click menu on a folder in the library tree (shows Export, Import into this folder, Sort...). | The macro library | Needed |
| 7 | `macro-editor-lines.png` | With **Advanced options** off (the default): the macro editor's lines (no Lines / Raw text choice above them) with 3-4 lines: a say line, a whisper with a recipient, a squad line with **Broadcast** ticked, and the character counters under the message boxes (ideally one long line with a yellow counter). | Editing macros | Needed |
| 8 | `macro-editor-raw.png` | With **Advanced options** ticked (options page, General tab): the **Raw text** tab of the same macro, with the **Will send:** list under it. | Editing macros | Needed |
| 9 | `send-status.png` | The editor after **Send now**, with the status line ("Sent 3 lines." or "Sending line 2 of 3...") next to the **Send now** and **Preview** buttons. A successful send: errors now show in the error window instead. | Sending macros | Needed |
| 10 | `hotkey-set.png` | The **Hotkey:** row of a macro with a hotkey set (e.g. Ctrl+F1) and its **Change...** / **Clear** buttons, ideally with the "Also set on: ... Where both are active, this hotkey sends nothing." warning (two macros active in the same place with the same hotkey). | Sending macros | Needed |
| 11 | `filters.png` | **Where it's active** opened: game modes ticked, two or three maps listed by name, and the "Active here" line. | Filters | Needed |
| 12 | `placeholders-tooltip.png` | The tooltip from hovering **Placeholders (?)** in the editor: `{time}` to `{nexus}`, with the line about other `{ }` text under it (no `{txt}` or `{json}` any more). | Placeholders | Needed |
| 13 | `export-menu.png` | Right-click a macro > **Export** submenu open. | Import and export | Needed |
| 14 | `import-window.png` | With **Advanced options** ticked (options page, General tab): the Import window with something loaded: the target folder, the options, **Select all / Deselect all**, and the preview with its checkboxes (one item unticked), at least one **NAME TAKEN** item and its choice box, a macro marked **REMOVED PLACEHOLDER**, and (with **Keep hotkeys** ticked) one marked **YOUR HOTKEY** with its notice above the preview. In-game check J10 sets this up (use **Keep both (rename)**). | Import and export | Needed |
| 15 | `send-alert.png` | A Nexus alert on screen from a hotkey that couldn't send, with the error window it also opens. Easiest: give two macros that are both active here the same hotkey, then press it with the library closed. | Safety and troubleshooting | Needed |
| 16 | `link-file.png` | With **Advanced options** ticked on the options page: a macro's **Text file link** section opened, with a file linked ("In step") and the Open file / Open folder / Unlink buttons. | Advanced options | Needed |
| 17 | `text-size.png` | The library window at the biggest text size, with the **Text size** dropdown (end of the toolbar) open. | The macro library | Needed |
| 18 | `error-window.png` | The error window with "What you did" and the OK / Copy to clipboard buttons. Easiest: toolbar **Export all** with an empty library, or select a folder 6 levels deep and click **New folder**. (A duplicate name shows it as a pop-up over the name box, which works too.) | Safety and troubleshooting | Needed |
| 19 | `trash-bin.png` | The library with the **Trash bin (F/M)** opened at the bottom of the tree (two or three items, one a folder), one item selected so the details pane shows **Restore** / **Delete permanently...**. | The macro library | Needed |
| 20 | `search.png` | The library tree while searching: a word typed in the **Search** box (e.g. `stack`), the "Found: ..." line, an opened folder path with a highlighted match, and one macro marked "(found in its lines)". Crop to the tree pane. | The macro library | Needed |
| 21 | `import-library.png` | With **Advanced options** ticked (options page, General tab): the Import window with **A whole library** chosen and a file like `Alex.json` loaded: step 2's folder name (`From Alex`), **Keep hotkeys** unticked, and the preview with the new folder holding their folders and macros. | Import and export | Needed |
| 22 | `quick-menu-search.png` | The quick menu pop-up (opened by its key) with `say` or another word typed in its search box: the flat result list with each macro's folders in brackets, the mouse over one so its preview shows. | Sending macros | Needed |
| 23 | `language-options.png` | The addon's page in Nexus > Options with the **Language** dropdown open: **Follow Nexus (...)**, English (US), Deutsch, Español, Français with their percentages. With **Advanced options** ticked (options page, General tab): the **Open localization folder** / **Reload language files** buttons and the text under them show too. | Languages | Needed |
| 24 | `library-german.png` | The library window in German (pick **Deutsch**), with a macro selected so the editor shows. | Languages | Needed |
| 25 | `filter-map-search.png` | **Where it's active** opened with `lion` typed in **Find a map by name or id**: the list of matches ("Lion's Arch - Kryta (50)", one marked "(been there)"), and the **Maps you've been to** dropdown next to it. | Filters | Needed |
| 26 | `hotkey-game-key.png` | The **Hotkey:** row of a macro set to a GW2 default key (e.g. F1 or Ctrl+T), with the yellow note "In GW2's default keys, F1 is "Profession skill 1"..." under it. | Sending macros | Needed |
| 27 | `options-map-list.png` | With **Advanced options** ticked (options page, General tab): the addon's page in Nexus > Options, **General** tab, scrolled to the map list: "Map list: 1,5xx maps, from ..." and the **Refresh map list** button. | Filters | Needed |
| 28 | `options-links.png` | The top of the addon's page in Nexus > Options: the version, the build stamp, the **Chat Messages on GitHub** and **ArenaNet's macro policy** links (the mouse over the policy link, so its address shows), the line under them, and the **General / Backups / Restore** tabs below. | ArenaNet's policies | Needed |
| 29 | `send-preview.png` | The macro editor after **Preview** on a 2-3 line macro with a whisper and a placeholder: each line's channel and typed text under the buttons, the whisper's "then Tab, then:" row, the note about placeholders, and **Close preview**. | Sending macros | Needed |
| 30 | `hotkey-cooldown.png` | With **Advanced options** ticked (options page, General tab): the macro editor's **Hotkey:** row with a hotkey set and, under it, the **Cooldown:** row with a value (e.g. 10 seconds) and the yellow "(N s left)" just after a hotkey send. | Sending macros | Needed |
| 31 | `starter-library.png` | The library on a first run: the **Examples** folder open with Basics, Placeholders and Filters (and Filters' two sub-folders) expanded, one example macro (e.g. Two channels) selected in the editor. | The macro library | Needed |
| 32 | `examples-replace.png` | The "Replace the examples?" prompt in the library after **Add the example macros** on the options page, with the trash bin on: the folder's current name, the yellow count of folders and macros that go to the trash bin, "can't be merged", and **Replace** / **Cancel**. | The macro library | Needed |
| 33 | `options-backups.png` | The **Backups** tab of the addon's options page with backups on: **Make backups of my macros** ticked, **Back up every** 1 day(s), **Copies to keep** 7, the text about the folder and file names, "Last backup: ... UTC." and the **Back up now** / **Open the backups folder** buttons. | Backups and restore | Needed |
| 34 | `options-restore.png` | The **Restore** tab with 3-4 backups and one safety copy listed: the Date (UTC), Time (UTC), Folders, Macros and Type columns and the **Restore...** buttons (the mouse over a date, so the file name shows). | Backups and restore | Needed |
| 35 | `quick-menu-favourites.png` | The quick menu with 2-3 **Favourites** (one a folder opened as a sub-menu, one faded because it isn't active on this map) and 2-3 **Recently sent** macros above the folders (each with its folders in brackets), and the library behind it showing a favourite with `*` after its name. | Sending macros | Needed |
| 36 | `export-window.png` | The **Export** window: the library as a tree with tick boxes, one folder ticked with a sub-folder showing **[some]**, a macro outside it ticked, the "N folder(s) and M macro(s) ticked" line, a file name typed, and the **Save as a .json file** / **Copy to the clipboard** buttons. | Import and export | Needed |
| 37 | `folder-filters.png` | The library with two or three folders that have filters, showing their tags (`Roaming  [WvW]`, `LA  [1 map]`), one of them dimmed because it isn't active here, and that folder selected: its details show **Where it's active** open with WvW ticked, a map listed, and "Not active here". | Filters | Needed |
| 39 | `options-advanced.png` | The addon's page in Nexus > Options, **General** tab, scrolled to the bottom: the **Advanced options** checkbox (unticked) with **[?]** next to it (the mouse over **[?]**, so its tooltip with the wiki address shows), the text under it listing the features behind it, and the two yellow lines "1 macro(s) linked to a text file..." and "1 macro(s) with a cooldown..." (have one of each). | Advanced options | Needed |
| 40 | `advanced-off-macro.png` | With **Advanced options** off: a macro linked to a text file selected in the library, dimmed in the tree, its editor showing the yellow "This macro is linked to a text file, which is an advanced option..." note above a faded **Send now**, and no **Text file link** section. | Advanced options | Needed |
| 38 | `new-folder-modes.png` | The **New folder** name box with a name typed and **Active in:** PvE / WvW / PvP below it (WvW only ticked), and the "Maps: in the folder's details" line. | The macro library | Needed |

## Retakes

Captured pictures that work but could be better. Save the new picture in `docs\images\` under
the same name, replacing the old one (the README and wiki already point to it). Its row in the
list above says "Captured, retake" until then; once the new picture is in, its row here goes and
its row in the list moves to Done.

| Image | What to change | Noted |
|---|---|---|
| `chat-result.png` | The middle line is the game's red "No one hears you." because nobody was near for the **Say** line. Retake with someone nearby, or with a macro whose lines go to Party and Squad, so every line shows as sent. | 2026-11-25 |
| `library-window.png` | It shows the **Cooldown:** row, the **Lines / Raw text** choice and the **Text file link: none** header in the editor, which are now hidden unless **Advanced options** is ticked (2026-11-25). Retake the same view with the box off (the default). | 2026-11-25 |
| `quick-menu.png` | No line preview shows: retake with the mouse resting over a macro (e.g. **Two channels**) so its lines appear next to the menu, as the row asks. The search box's hint is also cut off ("Search (Enter sends the f..."); retake after that's fixed. | 2026-11-25 |

## Done

Pictures that are in the docs and need nothing more. If a later change makes one out of date,
its row goes back to **Needed** in the list above.

| # | File name | What to capture | Used in | Status |
|---|---|---|---|---|
| 4 | `nexus-quick-access.png` | The Nexus Quick Access bar with the chat bubble icon (the quick bubble, since 2026-11-25), mouse over it so the tooltip shows. A small crop is fine. | Installation | Captured (2026-11-25) |
