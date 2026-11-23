# The macro library

The library window holds all your macros. Open it by left-clicking the scroll icon in the Nexus
bar, from the addon's page in Nexus options ("Open macro library"), or with the "Show/hide Chat
Messages library" keybind.

The window has two parts: the **tree** on the left, and the **details** of the selected item on
the right.

<!-- screenshot: library-window.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the library window, with the tree and the editor.*

## The Examples folder

The first time the addon starts, the library isn't empty: an **Examples** folder shows what a
macro can do. Select a macro to see how it's made, try **Preview** or **Send now**, and change or
delete anything you like. It's an ordinary folder.

```
Examples
├── Hello                      Say: "Hello everyone!"
├── Basics
│   ├── Two channels           Say, then Party
│   ├── Whisper a friend       a whisper to "Friend Name"
│   └── Wave                   an emote
├── Placeholders
│   ├── Roll a die             {random 1 6}
│   ├── Time check             {time}
│   └── Where am I             {map}
└── Filters
    ├── In WvW
    │   └── Stack on tag       squad (broadcast as commander), WvW only
    └── In Lion's Arch
        └── Meet at the bank   map chat, only in Lion's Arch
```

- The examples are in the addon's language at the time, and none has a hotkey.
- They're only added once. If you delete them, or start over by removing `macros.json`, they
  don't come back by themselves.
- To get them back, click **Add the example macros** on the addon's page in Nexus options. The
  library opens and:
  - **If you still have the Examples folder** (even renamed or moved), it asks whether to
    **replace** it with a fresh copy, in the same place. The old folder goes to the trash bin
    (or, with the trash bin off, is deleted for good), with everything in it, including macros
    you added or changed there. The two are never merged.
  - **If you had the examples before** but deleted them, it asks whether to add them again.
  - Otherwise it adds them at once.
  Your other macros are never changed.

<!-- screenshot: examples-replace.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the "Replace the examples?" prompt.*

<!-- screenshot: starter-library.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the library on a first run, with the Examples folder open.*

## Folders and macros

- Folders can be nested up to **6 levels** deep, for example *Raids › Wing 3 › Escort*. Macros can
  sit in any folder, or at the top level.
- Names are up to 100 characters. Two items in the same folder can't have the same name.
- **New folder** and **New macro** at the top create the item in the folder shown as "New items
  go into: ..." (the selected folder, or the top level).
- **New folder** is refused where the folder would go past level 6.

## Search

The **Search (Ctrl+F)** box above the tree finds folders and macros in a big library. Type part
of:

- a folder's or macro's **name**,
- the **text of a macro's lines**, or a whisper's **recipient**,
- a **hotkey**, like `ctrl+f1`.

Case and accents don't matter: `cafe` finds `Café`. While you search:

- The tree shows only what matches, with the folders above each match opened. Matched text in a
  name is highlighted; a macro found by its lines or hotkey says "(found in its lines)" or
  "(found in its hotkey)". A matching folder shows everything inside it.
- A line above the tree counts the matches, or says nothing matches.
- Dragging, **Move up** and **Move down** are off, because the tree isn't complete. The trash bin
  isn't searched or shown.
- A selected item that no longer shows is unselected.

Click **x** next to the box, or press **Escape** in it, to clear the search: the whole tree is
back, with the same folders open and closed as before. **Ctrl+F** anywhere in the library window
puts the cursor in the box. Hotkeys don't fire while you type in it.

### Only what's active here

Under the search box, the checkbox shows only the macros your hotkeys and the quick menu would
use where you are now. Its label says where that is, for example **Only active in WvW on
Eternal Battlegrounds (38)** or **Only active in PvE on Queensdale (15)**, and it follows you as
you change maps. Folders with nothing active are hidden, and the line above the tree says how many
macros are active here. It works together with the search text, and the addon remembers it.
Like searching, it turns off dragging and **Move up** / **Move down** while ticked. (See
[Filters](Filters) for setting where a macro is active.)

<!-- screenshot: search.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the library while searching, with a highlighted match.*

## Right-click menu

Right-click any item in the tree for:

- **New folder inside** / **New macro inside** (folders)
- **Send now** (macros)
- **Rename**
- **Move up** / **Move down** (not while [searching](#search))
- **Export**: save as a .json or .txt file, or copy to the clipboard (see
  [Import and export](Import-and-Export))
- **Import into this folder...** (folders)
- **Sort contents A-Z**, **Expand all inside**, **Collapse all inside** (folders)
- **Delete (to trash bin)**: moves it, and everything inside a folder, to the
  [trash bin](#trash-bin). If you turned the trash bin off, this is **Delete...**: it asks first,
  says how many folders and macros are inside, and can't be undone.

<!-- screenshot: tree-context-menu.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the right-click menu of a folder.*

## Trash bin

Deleted folders and macros go to the **Trash bin** at the bottom of the library tree, so a
mistake can be undone. You can also drag an item onto the bin. Its counter shows folders/macros:
**Trash bin (1/4)** is 1 folder and 4 macros, counting everything inside deleted folders.
The trash bin is never exported, and isn't imported from someone else's `macros.json`.

- Click an item in the bin to see what it is, when it was deleted and where from. Items in the
  bin can't be sent, edited or used by a hotkey, and don't show in the quick menu.
- **Restore** (button, or right-click the item): it goes back where it was. If its folder has
  been deleted since, you choose: **Recreate the folders** or **Put it at the top level**. If an
  item with the same name is already there, the restored one gets " (2)" added (a very long name
  is shortened so it fits).
- **Delete permanently...** removes one item for good. Right-click the bin > **Empty trash
  bin...** removes everything in it. Both ask first and can't be undone.
- Items stay in the bin until you restore them or empty it.
- To turn the bin off: Nexus > Options > Addons > Chat Messages, untick **Use a trash bin for
  deleted folders and macros**. Delete then asks first and removes items for good. Anything
  already in the bin stays there until you empty it.

<!-- screenshot: trash-bin.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the trash bin opened, with an item selected.*

## Drag and drop

Drag an item to move it:

- Drop it on the **middle of a folder** to put it inside that folder.
- Drop it **above or below an item** to place it next to that item.

A move is refused if it would put a folder deeper than level 6, or if the target already has an
item with the same name. The window tells you why.

## Toolbar

- **Expand all** / **Collapse all**
- **Sort A-Z**: sorts every folder, folders first and then macros, each alphabetically.
- **Import...** and **Export all** (see [Import and export](Import-and-Export)).
- **Text size**: see below.

## Text size

The **Text size** dropdown (at the end of the toolbar, and on the addon's page in Nexus options)
sets how big the text is in all of this addon's windows: the library, the Import window, the
quick menu and their tooltips. Nexus' own options page isn't changed.

- The sizes are the fonts Nexus provides, shown with their size in pixels, for example
  "Default (13 px)" and "Nexus large font (24 px)". How many there are depends on Nexus.
- Open windows grow or shrink to match, and stay on screen.
- Long text wraps at the edge of the window. Buttons that don't fit move to the next line. If a
  name is too long for the tree, scroll it sideways.
- Your choice is saved in `settings.json` and used again next time.

<!-- screenshot: text-size.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the library at a large text size, with the Text size dropdown open.*

## Dimmed macros

Macros that aren't active where you are right now (see [Filters](Filters)) are shown dimmed.
They still work with **Send now**.
