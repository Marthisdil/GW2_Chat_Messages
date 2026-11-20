# The macro library

The library window holds all your macros. Open it by left-clicking the scroll icon in the Nexus
bar, from the addon's page in Nexus options ("Open macro library"), or with the "Show/hide Chat
Messages library" keybind.

The window has two parts: the **tree** on the left, and the **details** of the selected item on
the right.

## Folders and macros

- Folders can be nested up to **6 levels** deep, for example *Raids › Wing 3 › Escort*. Macros can
  sit in any folder, or at the top level.
- Names are up to 100 characters. Two items in the same folder can't have the same name.
- **New folder** and **New macro** at the top create the item in the folder shown as "New items
  go into: ..." (the selected folder, or the top level).
- **New folder** is refused where the folder would go past level 6.

## Right-click menu

Right-click any item in the tree for:

- **New folder inside** / **New macro inside** (folders)
- **Send now** (macros)
- **Rename**
- **Move up** / **Move down**
- **Sort contents A-Z**, **Expand all inside**, **Collapse all inside** (folders)
- **Delete...**: asks first. Deleting a folder deletes everything inside it, and the dialog says
  how many folders and macros that is. This can't be undone.

## Drag and drop

Drag an item to move it:

- Drop it on the **middle of a folder** to put it inside that folder.
- Drop it **above or below an item** to place it next to that item.

A move is refused if it would put a folder deeper than level 6, or if the target already has an
item with the same name. The window tells you why.

## Toolbar

- **Expand all** / **Collapse all**
- **Sort A-Z**: sorts every folder, folders first and then macros, each alphabetically.

## Dimmed macros

Macros that aren't active where you are right now (see [Filters](Filters)) are shown dimmed.
They still work with **Send now**.
