# Import and export

Share macros with friends, move them to another PC, or keep a backup. An import always shows a
preview first. Your library only changes when you click **Import selected**, and one import can be undone.

## Export

Exports go to `<GW2>\addons\GW2_Chat_Messages\exports\`. The addon creates this folder (and
`imports\`) when it loads, so you never have to make it. An existing file is never overwritten: a
second export of the same name is saved as `Name (2).json`.

- **Exports folder** (in the library's toolbar) opens the exports folder in Windows Explorer.
- After an export, the message under the toolbar ("Exported to ...") has an **Open exports
  folder** button.

- **Export all** (in the library's toolbar) saves your whole library as
  `Chat Messages library.json`.
- **Export some...** (in the library's toolbar) opens the **Export** window, to pick several
  folders and macros for one file. See [Export a selection](#export-a-selection) below.
- **Right-click a folder or macro > Export** for just that item:
  - **Save as a .json file**: the folder with everything inside it, or the macro, with its lines,
    filters and hotkey.
  - **Save as a .txt file** (macros only): the lines as raw text, one per row, like
    `/s Hello` (see [Editing macros](Editing-Macros)). Filters and hotkeys aren't included.
  - **Copy to the clipboard**: the same as the .json file, ready to paste into a message or a
    friend's Import window.
  - **Copy the lines as text** (macros only): the raw text, like the .txt file.
  - **Choose more to export...**: opens the Export window with this item ticked.

### Export a selection

The **Export** window lists your library with a tick box next to each folder and macro. It
starts with the item you had selected (or right-clicked) ticked.

- Tick a folder to tick everything inside it; untick one item inside and the folder shows
  **[some]**. Ticking a macro deep inside also ticks the folders above it, so it keeps its place.
- **Select all** / **Deselect all**, and a line that counts the ticked folders and macros.
- **File name:** `Chat Messages selection` unless you type another; `.json` is added, and a name
  that's taken gets " (2)".
- **Save as a .json file** saves the ticked items, in their folders, to the exports folder.
  **Copy to the clipboard** copies the same, ready to paste into someone's Import window.
- A folder with only some of its contents ticked is exported with just those inside.
- You can keep editing the library while the window is open. Something you delete is simply
  left out.

<!-- screenshot: export-window.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Export window with some folders and macros ticked.*

[Text file links](Editing-Macros) are never exported or imported, because they're paths on your
own PC. The [trash bin](The-Macro-Library) isn't exported either: while it holds anything, the
export message and the Export menu remind you.

<!-- screenshot: export-menu.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Export submenu.*

## Import

Click **Import...** in the toolbar, or right-click a folder and choose **Import into this
folder...**. The Import window has three steps.

### 1. Choose what to import

At the top, choose **Macros** (the usual) or **A whole library** (someone else's `macros.json`,
see [Importing a whole library](#importing-a-whole-library) below).

With **Macros**, you can import:

- a **.json** file exported by this addon (or a whole `macros.json`; its trash bin isn't
  imported, and the Import window says so),
- a **.txt** file, which becomes one macro named after the file,
- a **folder of .txt files**: the folder becomes a folder, each .txt file a macro, and
  sub-folders become sub-folders. Other files are ignored.

Pick it in one of these ways:

- Put it in `<GW2>\addons\GW2_Chat_Messages\imports\` and click it in the list. **Open imports
  folder** opens that folder in Windows Explorer; **Refresh** updates the list.
- Type or paste a full path (Windows' "Copy as path" works, quotes and all) and click **Load**.
- **Paste from clipboard**: an export someone copied for you, or raw text lines, which become
  one macro called "Pasted macro".

### 2. Choose where it goes

- **Import into**: the top level or any folder.
- **When a macro's name is taken**:
  - **Keep both (rename)** (the default): the new one is called `Name (2)` (a very long name is
    shortened so " (2)" fits in the 100-character limit).
  - **Skip**: the existing macro stays, the new one is left out.
  - **Replace**: the existing macro keeps its place, but gets the new lines and filters, and the
    new hotkey if **Keep hotkeys** is ticked (otherwise no hotkey). A new hotkey with no Ctrl, Alt
    or Shift, or one another of your macros already has, is never brought along by Replace.
- **When a folder's name is taken**:
  - **Merge** (the default): the new folder's contents go into the existing folder, with the
    rules above for anything inside.
  - **Keep both (rename)**, **Skip**: as for macros.
  - **Replace**: everything in the existing folder is deleted, then the new contents go in, and
    the folder takes the new one's [filter](Filters#filters-on-folders).
  - A new folder (or a renamed copy) brings its filter along. **Merge** keeps your folder's own.
- **Move the contents of too-deep folders up a level**: folders can be nested at most 6 deep.
  Unticked, a folder that would go deeper is left out with everything inside. Ticked, that
  folder is left out but its contents go into the folder above (without that folder's filter,
  so they may be active in more places).
- **Keep hotkeys**: off at first, for every import, so macros from someone else come without
  their hotkeys (a hotkey sends its macro whenever you press that key, and could clash with
  yours). Tick it for your own exports; the preview then shows each macro's hotkey next to its
  name, like `Stack  [Ctrl+F1]`.

### 3. Check the preview

The preview shows every folder and macro and what will happen to it: **new**, **new, as "Name
(2)"**, **goes into the existing folder**, **replaces the existing one**, **skipped**, **left
out** or **not selected**. A summary line counts them.

Each item whose name is already taken is marked **NAME TAKEN** and has its own choice box, so you
can decide one by one: for example replace one macro and keep both of another.

**Things to check first** are marked after the item, with a line above the preview that counts
them (hover a mark to see why):

| Mark | Means |
|---|---|
| **REMOVED PLACEHOLDER** | It uses `{txt}` or `{json}`, which this addon no longer has (see [Placeholders](Placeholders)). It won't be sent until you edit those out. |
| **KEY WITHOUT CTRL/ALT/SHIFT** | (Keep hotkeys ticked) Its hotkey is a plain key, so it sends whenever you press that key while playing. |
| **YOUR HOTKEY** | (Keep hotkeys ticked) One of your macros already has this hotkey, and both can be active at once (their filters overlap): there, the key sends nothing. |
| **COMES WITHOUT ITS HOTKEY** | It replaces one of yours, and its hotkey is one of the two kinds above, so the hotkey isn't imported. |
| **YOUR HOTKEY GOES** | The macro it replaces has a hotkey of its own, which is removed. |

A new macro (not replacing one) keeps a marked hotkey if **Keep hotkeys** is ticked: change it in
the editor, or untick the macro. After **Import selected**, the line with **Undo import** also
says how many replaced macros now have no hotkey or a different one.

**Choose what to import:** every item has a checkbox, and everything is ticked at first.

- A folder's checkbox ticks or unticks the folder with everything inside it.
- A macro's checkbox is just that macro. Ticking a macro inside an unticked folder also ticks the
  folders above it, so it has somewhere to go; the folder's other contents stay unticked.
- A folder with only some of its contents ticked shows **[some]**, and brings only those.
- **Select all** / **Deselect all** above the preview tick or untick everything.

Click **Import selected** to import what's ticked, or **Cancel**. Nothing has changed until you
click **Import selected**.

<!-- screenshot: import-window.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Import window with its preview.*

## Importing a whole library

A friend can send you their whole library: the `macros.json` file from their
`<GW2>\addons\GW2_Chat_Messages\` folder (or its backup, `macros.json.bak`). It's best to rename
it after them first, for example `Alex.json`.

1. Put the file in your `imports\` folder, open **Import...** and choose **A whole library**. The
   list then shows only `.json` and `.bak` files. (A path or **Paste from clipboard** work too.)
2. Their library goes into **one new folder at the top level of your library**, so it never mixes
   with your own folders. Its name is filled in from the file name (`From Alex` for `Alex.json`,
   or `Imported library` for a file still called `macros.json`); change it if you like. If you
   already have a folder with that name, the new one gets " (2)".
3. **Keep hotkeys** is off at first, as for any import, so their keys don't clash with yours.
   Their trash bin and [text file links](Editing-Macros) are never taken.
4. The preview and the checkboxes work as above: untick anything you don't want. Their folders are
   one level deeper inside the new folder, so a folder that would end up more than 6 deep is
   shown as too deep.

Their `settings.json` (text size, language, trash bin setting) isn't needed: those are your own choices.

<!-- screenshot: import-library.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Import window with a whole library loaded.*

## Undo

After an import, the library window shows what was imported and an **Undo import** button. It
puts your library back as it was just before the import. Anything you changed after the import
is undone too. The button stays until you click **Dismiss**, import again, or unload the addon.

Every save also keeps the previous version as `macros.json.bak` (see
[Files and data](Files-and-Data)).

## Limits

- A .json file can be up to 4 MB, a .txt file up to 256 KB.
- A folder import reads at most 2000 .txt files, and at most 16 MB of text in all.
- The Import window lists at most 200 warnings for one file; it counts the rest.
- One import can hold up to 5000 folders and macros.
- A macro keeps at most 50 lines; longer .txt files are cut, with a warning in the Import window.
- .txt files are read as UTF-8 (Notepad's default). In a file saved another way (ANSI, say),
  characters like `ä` or `é` can't be read: they're replaced with `�` (the font may show `?`),
  with a warning in the Import window. Save the file as UTF-8 and import it again to keep them.
  Each replaced character takes three bytes, so a file that would be over 256 KB that way is
  left out.
- If the game runs out of memory while importing, nothing is imported and the error window says
  so.
