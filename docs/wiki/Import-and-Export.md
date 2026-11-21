# Import and export

Share macros with friends, move them to another PC, or keep a backup. An import always shows a
preview first. Your library only changes when you click **Import**, and one import can be undone.

## Export

Exports go to `<GW2>\addons\Chat Messages\exports\`. An existing file is never overwritten: a
second export of the same name is saved as `Name (2).json`.

- **Export all** (in the library's toolbar) saves your whole library as
  `Chat Messages library.json`.
- **Right-click a folder or macro > Export** for just that item:
  - **Save as a .json file**: the folder with everything inside it, or the macro, with its lines,
    filters and hotkey.
  - **Save as a .txt file** (macros only): the lines as raw text, one per row, like
    `/s Hello` (see [Editing macros](Editing-Macros)). Filters and hotkeys aren't included.
  - **Copy to the clipboard**: the same as the .json file, ready to paste into a message or a
    friend's Import window.
  - **Copy the lines as text** (macros only): the raw text, like the .txt file.

Link files (a planned feature) are never exported, because they're paths on your own PC.

## Import

Click **Import...** in the toolbar, or right-click a folder and choose **Import into this
folder...**. The Import window has three steps.

### 1. Choose what to import

You can import:

- a **.json** file exported by this addon (or a whole `macros.json`),
- a **.txt** file, which becomes one macro named after the file,
- a **folder of .txt files**: the folder becomes a folder, each .txt file a macro, and
  sub-folders become sub-folders. Other files are ignored.

Pick it in one of these ways:

- Put it in `<GW2>\addons\Chat Messages\imports\` and click it in the list. **Open imports
  folder** opens that folder in Windows Explorer; **Refresh** updates the list.
- Type or paste a full path (Windows' "Copy as path" works, quotes and all) and click **Load**.
- **Paste from clipboard**: an export someone copied for you, or raw text lines, which become
  one macro called "Pasted macro".

### 2. Choose where it goes

- **Import into**: the top level or any folder.
- **When a macro's name is taken**:
  - **Keep both (rename)** (the default): the new one is called `Name (2)`.
  - **Skip**: the existing macro stays, the new one is left out.
  - **Replace**: the existing macro keeps its place, but gets the new lines, filters and hotkey.
- **When a folder's name is taken**:
  - **Merge** (the default): the new folder's contents go into the existing folder, with the
    rules above for anything inside.
  - **Keep both (rename)**, **Skip**: as for macros.
  - **Replace**: everything in the existing folder is deleted, then the new contents go in.
- **Move the contents of too-deep folders up a level**: folders can be nested at most 6 deep.
  Unticked, a folder that would go deeper is left out with everything inside. Ticked, that
  folder is left out but its contents go into the folder above.
- **Keep hotkeys**: untick it for macros from someone else, so their keys don't clash with
  yours.

### 3. Check the preview

The preview shows every folder and macro and what will happen to it: **new**, **new, as "Name
(2)"**, **goes into the existing folder**, **replaces the existing one**, **skipped** or **left
out**. A summary line counts them.

Each item whose name is already taken has its own choice box, so you can decide one by one: for
example replace one macro and keep both of another.

Click **Import** to do it, or **Cancel**. Nothing has changed until you click **Import**.

## Undo

After an import, the library window shows what was imported and an **Undo import** button. It
puts your library back as it was just before the import. Anything you changed after the import
is undone too. The button stays until you click **Dismiss**, import again, or unload the addon.

Every save also keeps the previous version as `macros.json.bak` (see
[Files and data](Files-and-Data)).

## Limits

- A .json file can be up to 4 MB, a .txt file up to 256 KB.
- A folder import reads at most 2000 .txt files.
- One import can hold up to 5000 folders and macros.
- A macro keeps at most 50 lines; longer .txt files are cut, with a warning in the Import window.
