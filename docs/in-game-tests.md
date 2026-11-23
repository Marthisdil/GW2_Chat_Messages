# In-game tests

The unit tests cover the rules (tree, raw text, send plan, hotkeys, filters, placeholders,
import, link sync). These checks cover what only the game can tell us: whether GW2 takes our keys
and text, and whether normal game input is ever disturbed.

The checks are grouped into sessions (A to Q) in the order to run them: each one builds on the
ones before. A whole pass takes about an hour. If the text is hard to read, do M1 first and
pick a bigger size. Session B is the one that matters most: if it
fails, most of the later sessions can't be run, so report it first.

## Progress

Results so far (keep your notes in `docs\testing results.txt`; it isn't committed):

| Session | State | Notes |
|---|---|---|
| A1 | Passed (PR #15 build) | Re-check only the version and build stamp lines with a new build. |
| A2 | Passed, 2 fixes to re-check | The duplicate-name error and the Delete pop-up's text ran off the right. Fixed: text now wraps (PR #17) and errors open the error window (PR #18). Re-check those two points only, then go on to A3. |
| A3 to Q | Not run yet | New since the last build: O (trash bin), the counter in A4, J3b/J6 (import checkboxes, trash bin not exported or imported). Newer still: P (search box; P6-P8 the "only active here" filter and the quick menu search), J7 (a whole library), and F2/F3/F7 (mouse buttons now need Ctrl, Alt or Shift). Newest: Q (languages), J8/K6 (text files that aren't UTF-8), and J2 (**Keep hotkeys** now starts unticked for every import; ticked, the preview shows the hotkeys), O3's long name, I8 (a placeholder can't add a command or a key) and I5 (`{json}` refuses local addresses). Newer: I9 (a damaged `maps.json`) and J9 (a file with thousands of warnings). Newest: J10 (the import preview's marks; Replace drops a clashing hotkey). |

**Errors since PR #18:** every refusal and error the checks below expect ("refused", "says
why", "a message") now opens the **error window** instead of a red line under the editor. Click
**OK** to close it before the next step. Where a check asks for the exact message, use **Copy to
clipboard** and paste it into your notes. Errors from a hotkey or the quick menu also show the
Nexus alert, as before.

## How to report

For every check, write **pass**, **fail** or **skipped**, and for a fail (or anything odd):

- what you did and what happened, in your own words;
- the **exact error text**: click **Copy to clipboard** in the error window and paste it (for
  a hotkey error, the Nexus alert's text too);
- the Nexus log lines mentioning `Chat Messages` (Nexus > Log; filter by "Chat Messages");
- for anything visual, a screenshot (save it anywhere, not in `docs\images\`, which is for the
  README and wiki pictures).

Checks marked **(question)** have no right answer yet: they decide how a feature will work, so
describe what you saw even if nothing went wrong. A copy-paste template is at the end.

## Before you start

1. Build Release and copy `build\Release\GW2_Chat_Messages.dll` into `<GW2>\addons\`. If you had
   an earlier build in game, back up `<GW2>\addons\GW2_Chat_Messages\` first.
2. In **Nexus > Options > Game Keybinds**, check that **Chat** matches your in-game chat key, and
   (for session E) **Squad Broadcast**. The addon uses these; it never guesses a key.
3. In **Nexus > Options > Keybinds**, set keys for **Show/hide Chat Messages library** and **Open
   Chat Messages quick menu** (any free keys, e.g. Ctrl+Shift+M and Ctrl+Shift+Q).
4. Stand somewhere quiet in the open world (PvE), near an enemy you can target with Tab, but not
   in combat.

## A. Loading and the library

**A1. Loading.** Start the game. In the Nexus log: `Loaded 2026.MM.DD.HHMM.Dev (build abc1234, 2026-MM-DD HH:MM UTC).`
(the build's version and stamp; `+changes` after the commit if it was built with uncommitted
edits) and no warnings. Right-click the DLL > Properties > Details: File version and Product
version show the version, Comments shows the same stamp.
The scroll icon is in the Nexus bar. Clicking it opens and closes the library; so does your
library key. Nexus > Options > Addons > Chat Messages shows the version, the build stamp under it
("Build abc1234, ..."), and an **Open macro library** button that works.

**A2. Building the tree.** In the library:
- **New folder** (toolbar) → name `Test`. In it, **New folder inside** five times, one inside the
  next, so the deepest is 6 folders deep. A 7th folder inside the deepest one is refused with a
  "levels deep" message: "New folder inside" isn't offered there, so select the deepest folder
  and click **New folder** in the toolbar. A macro inside the deepest folder is allowed.
- A second folder or macro called `test` (any case) in the same folder is refused ("There is
  already a ...") in the error window (session N). *Re-check: the error isn't cut off.*
- Rename (right-click > Rename), Move up / Move down, **Sort contents A-Z**, Expand all / Collapse
  all: each does what it says.
- Drag a macro into another folder, and drag a folder into a folder: both move. Dragging a folder
  into its own sub-folder, or so that it would end up more than 6 deep, is refused.
- Delete a folder with macros in it: with the trash bin on (the default) it goes straight to
  the **Trash bin** at the bottom of the tree (session O tests the bin). *Re-check the old
  pop-up:* untick the trash bin in the addon's Nexus options, delete another folder with macros
  in it: it asks first, its text about what's inside wraps and can be read in full, then it's
  removed. Tick the trash bin again.

**A3. The test macros.** In `Test`, create these (you'll use them in later sessions):

| Name | Lines |
|---|---|
| `Say` | Say: `test 1` |
| `Two` | Map: `test map`, then Party: `test party` (Say twice if you're not in a party) |
| `Whisper` | Whisper to **your own character name**: `test whisper` |
| `Emote` | Current channel: `/wave` |
| `Long` | Say: 196 characters (paste `1234567890` 20 times and delete 4) |
| `Accents` | Say: `héllo wörld ñ ü – “quotes” 日本` |

**A4. The line editor.** On `Two`:
- Switch to **Raw text**: it shows `/m test map` and `/p test party`. Add a row `/sagen hallo`
  (German) and a row `/dire salut` (French); back in **Lines** they show as Say lines.
- The counter is **under** each message box: `23/199 characters`. On `Long` it shows 199/199 (196
  plus `/s `) in bright red. Delete characters: it's yellow from 150 to 179 and grey below 150
  (question: are yellow and red easy to see in game?). Type one more than 199: it says "too long,
  this line won't be sent". Put `Long` back to 196 characters.
- **Add line** stops at 50 lines (try on a scratch macro, then delete it).

**A5. Saving.** Close the game normally (or unload the addon in Nexus > Addons), start it again:
the whole tree, every line, expanded folders and selection are as you left them.
`<GW2>\addons\GW2_Chat_Messages\` has `macros.json` and `macros.json.bak`.

**A6. A broken file.** Close the game. Open `macros.json` in Notepad, delete the first `{` and
save. Start the game: the Nexus log says the file couldn't be read and the macros were restored
from the backup, and the error window shows the same (What you did: "Started the game"); the
library shows your macros (maybe missing the very last change); a
`macros.json.unreadable-<date>` file is in the folder. Nothing is lost.

## B. Sending: does the game take our keys? (most important)

Use **Send now** in the editor for these. If B1 fails, stop and report it: the rest depends on it.

**B1. `Say`.** Click **Send now** with the mouse over the addon window. `test 1` appears in Say
chat and chat closes again; the status says "Sent 1 line." If it fails, the error window says why;
copy it, and say which part:
- chat didn't open at all ("chat didn't open"), or
- chat opened but stayed empty (the game ignores our typed text: we'd need the clipboard way), or
- the text was typed but not sent ("the game didn't take the message").

**B2. More lines and kinds.** `Two`: both lines arrive, in order, in the right channels.
`Emote`: your character waves. `Long`: arrives in full. `Accents`: every character arrives exactly
as written (including `日本`, if your chat font shows it).

**B3. Right-click.** Right-click `Say` in the tree > **Send now**: same as the button.

**B4. Leftover text (question).** Open chat yourself, type `leftover`, press Escape (GW2 keeps
the text). Send `Say`. What arrives: `test 1`, or `leftovertest 1`? We can't clear the box without
Ctrl+A, so this tells us whether that's needed.

## C. Whispers and Tab

**C1. Whisper.** Send `Whisper` (to yourself): it arrives as a whisper. Then, with chat closed,
press **Tab** a few times: it targets enemies as normal. Send `Whisper` 5 times in a row (wait for
each), then Tab again: still normal.

**C2. Whisper without Tab (question).** By hand, in chat, type `/w YourName test` (no Tab, just a
space after the name) and press Enter. Does the whisper arrive? Also try a name with a space in
it if you know one, e.g. `/w Some Name test`. If the first works, the addon can stop pressing Tab.

## D. Refusals and recovery

Nothing should happen in game for D1-D4: no chat opens, nothing is typed. Each refusal opens
the error window with the message below; click OK before the next check.

**D1. You're typing.** Open chat and type something. Move the mouse to the addon window and click
**Send now** on `Say` (the click takes focus from chat; if chat closes, say so). Expected: refused,
"A text box is open in game", and your own text is untouched.

**D2. Modifier held.** Hold Shift, click **Send now**, keep holding for a second: refused with
the "Shift, Ctrl, Alt or Windows key is held down" message. Repeat with Ctrl.

**D3. Chat key not set.** Clear **Chat** in Nexus Game Keybinds; send `Say`: refused with
"Nexus doesn't know your "Chat" key". Set it back afterwards.

**D4. Busy.** Click **Send now** on `Two` twice quickly: the second click is ignored ("Already
sending a macro.") and `Two` arrives once.

**D5. Interrupting.** Send `Two` and, while the first line is going out, press Escape yourself
(or click into the game). The send stops and the error window says why. The game menu must **not** open (we send
at most one Escape). Report what you see in chat.

**D6. Not in game.** Log out to character select, open the library from the Nexus bar and send
`Say`: refused, "Not in game". (If the bar isn't there on character select, mark skipped.)

## E. Commander (skip if you can't tag up)

**E1.** Add a Squad line with **Broadcast** ticked. As commander it goes out as a squad broadcast;
when you're not commander, the same macro is sent as normal squad chat.

## F. Hotkeys

**F1. Setting one.** On `Say`, click **Set...** and press Ctrl+F1: the editor and the tree show
`Ctrl+F1`. **Set...** then Escape: cancelled, `Ctrl+F1` stays. **Set...** then plain Tab, plain
Enter or Ctrl+Escape: refused, with the reason in the error window. Closing the window while it waits for a key also
cancels it.

**F2. Mouse buttons (question).** Mouse buttons need Ctrl, Alt or Shift. On `Emote`, **Set...**
with the mouse over the addon window and press plain Mouse 4: refused, the error window says it
needs Ctrl, Alt or Shift. **Set...** again and press **Ctrl+Mouse 4**. Is it recorded? Try
Shift+Mouse 5 and Alt+Mouse 3 (wheel click) too. Then try with the mouse over the game instead of
the window. Report which work where. Leave `Emote` on Ctrl+Mouse 4.

**F3. Using it.** Close the library and press Ctrl+F1, releasing Ctrl quickly: `Say` is sent. Hold
Ctrl+F1 down for a few seconds: sent once only. Press it twice quickly: sent once. Press the mouse
hotkey from F2 (Ctrl+Mouse 4) while moving the camera: `Emote` is sent and the camera isn't
affected afterwards (no stuck button). Plain Mouse 4 (without Ctrl) does its normal game action
(e.g. dodge) and sends nothing.

**F4. Not while typing.** Give `Say` the plain key `F2` (or any key you never use) for this check.
Open chat and press F2: nothing is sent. Click into another addon's text box (or our own message
box) and press F2: nothing is sent. Set `Say` back to Ctrl+F1.

**F5. Clashes.** Give `Two` the same hotkey as `Say`: both show "Also set on ...". Pressing it
shows an alert and the error window (without taking focus), and sends nothing. Clear `Two`'s
hotkey. Then clear the Chat game keybind and press Ctrl+F1: the alert and the error window
explain why nothing was sent. Set the key back.

**F6. Saved.** Reload the addon (Nexus > Addons, or restart): the hotkeys are still set and work.

**F7. An old plain mouse hotkey.** Builds before this one allowed plain Mouse 3/4/5. Close the
game, open `macros.json` in Notepad, find `Emote`'s `"hotkey"` and change it to `"Mouse 4"`, save,
and start the game. Nothing in the log or error window complains. The tree shows `Emote  [Mouse
4: off, needs Ctrl/Alt/Shift]`; its editor shows `Mouse 4` in red with "This hotkey doesn't work
any more: mouse buttons need Ctrl, Alt or Shift". Pressing Mouse 4 sends nothing (and does its
game action); the quick menu lists `Emote` without a key. **Change...** to Ctrl+Mouse 4: the
warning goes and it works again.

## G. Quick menu

**G1. Right-click menu.** Right-click the scroll icon: the library shows as nested menus (empty
folders hidden). Hovering a macro shows its lines; clicking it sends it and closes the menu.
Put a folder and a macro with the same name in one folder: both work.

**G2. Pop-up key.** Press your quick menu key: the menu opens at the mouse pointer. Press it again,
or click outside: it closes. Send something from it.

**G3. Action camera (question).** Turn on action camera (mouse hidden), press the quick menu key.
Where does it appear, and can you use it? We may need a "centre of the screen" option.

## H. Filters (where a macro is active)

**H1. Game modes.** On `Emote`, open **Where it's active**: by default it's everywhere. Untick
PvE. In the open world: `Emote` is dimmed in the library, missing from the quick menu, and its
mouse hotkey does nothing; **Send now** still sends it. Unticking the last game mode is refused (error window).

**H2. Maps.** **Add this map** adds the map you're on, shown by name (and `maps.json` appears in
the addon folder). The macro is then active only on this map: check on another map, then remove it
with its **x**. While the current map is in the list, **Add this map** is greyed out, and
**Add id** with the same number opens the error window: it's already in the list.

**H3. Same hotkey, different modes.** Give `Say` a WvW-only filter and `Two` a PvE-only filter, both
with the same hotkey: no warning in the editor, and the hotkey sends the right one in each mode.
Make both PvE: now the editor warns, and pressing it in PvE shows the "set on 2 macros" alert.
Clear the filters and `Two`'s hotkey afterwards.

**H4. What mode is this map? (question)** The editor's "Active here (mode, map name (id))" line
shows the mode it thinks you're in. Visit as many of these as you can and note the mode and map number for
each: Heart of the Mists, Edge of the Mists, Armistice Bastion, a WvW borderland, a fractal, a
raid, a strike mission, your home instance, a guild hall, a story instance, a PvP match. Note
any that land in the wrong mode. On a map where no macro is active, the quick menu says "No macros
for this map."

## I. Placeholders

Make a macro `Fill` and change its line for each check.

**I1. Time and random.** `/s It's {time} on {today}, roll {random 1 6}`: the real time and date;
send it a few times, the roll stays 1-6. Note the date format as sent (question: is the weekday
in your Windows language, and is the format what you'd want?).

**I2. Text files.** Put `names.txt` with three lines (one with an accent) in
`<GW2>\addons\GW2_Chat_Messages\`, saved as UTF-8. `{txt names.txt}` sends a random line;
`{txt names.txt 2}` the second one; accents arrive right. `{txt missing.txt}`: nothing is sent and
the error window says it wasn't found.

**I3. Left alone.** `/s {smile} meet at [&BDAEAAA=]`: `{smile}` arrives as typed, and the code
arrives as a clickable waypoint link.

**I4. Too long once filled in.** Make line 1 of `names.txt` 190 characters long, and send
`/s {txt names.txt 1} and more`: refused, the error window says it's too long, nothing sent.

**I5. Map name and {json}.** `/s Hello from {map}`: sends the map name, also right after you
arrive on a map you haven't visited this session. `{json id https://api.guildwars2.com/v2/build}`:
sends the game build number. `{json nope https://api.guildwars2.com/v2/build}`: nothing is sent;
the error window says the path wasn't found. `{json id http://127.0.0.1/}` and
`{json id http://localhost/}`: nothing is sent; the error window says the address leads to this
PC or a local network. `{json id http://localtest.me/}` (a public name that points to 127.0.0.1):
the same. *Only if you use a WinHTTP proxy* (`netsh winhttp show proxy` names one): the build
number still arrives, and the local addresses are still refused, as is
`{json id http://odd_name.example.com/}` (the error window says the name has characters that
can't be checked).

**I6. Offline.** Close the game, delete `maps.json`, disconnect from the internet, start the game
(Nexus may complain about updates; that's fine) and enter a map. `{map}` fails, the error window says the API
"couldn't be reached", and nothing is sent. The game doesn't stutter while it tries. Reconnect.

**I7. Typing while it prepares.** Send the `{json ...build}` macro and, while the status says
"Preparing...", open chat and start typing. The send stops (error window: "A text box is open") and
**your chat box stays open with your text**. The error window doesn't take your typing away.

**I8. A placeholder can't add a command or a key.** Put `emote.txt` with the one line `/wave` in
`<GW2>\addons\GW2_Chat_Messages\`.
- A Current-channel line `{txt emote.txt}`: nothing is sent, and the error window says a
  placeholder would start a chat command. `/s {txt emote.txt}` and `/g {txt emote.txt}` (typed in
  a Current-channel line): the same.
- A line `/wave` (typed yourself) still waves, and `/s waves at {txt emote.txt}` sends
  `waves at /wave` in Say.
- Put `tabs.txt` with one line `a`, a Tab, `b` (typed in Notepad), and send `/s {txt tabs.txt}`:
  `a b` arrives in Say, and nothing else happens (chat doesn't jump to another box or channel).
- *(question)* Look-alikes: put `slash.txt` with `／g hi` (a fullwidth slash, U+FF0F) and
  `filler.txt` with `ㅤ/g hi` (U+3164 first), and send each as `{txt ...}` on Current. Note whether
  either posts to guild chat (the addon only treats `/` and ordinary spaces as commands). If one
  does, report it: the check has to learn that character.

**I9. A damaged map cache.** Close the game. In `maps.json` in the addon folder, change
the number after `"format":` to `"x"` (with the quotes) and save. Start the game and enter a map: no error, and
`/s Hello from {map}` still sends the map name (the cache is rebuilt).

## J. Import and export

**J1. Export.** Right-click `Say` > Export > **Save as a .json file**, twice: `exports\Say.json`
and `exports\Say (2).json` appear and the status names the file. **Save as a .txt file** and
**Copy the lines as text** give the raw lines, one per row (paste into Notepad to check).

**J2. Import with conflicts.** Right-click the `Test` folder > Export > **Copy to the clipboard**.
Then toolbar **Import...** > **Paste from clipboard**, with `Test`'s parent as the target. The
preview shows `Test` as "goes into the existing folder" and the macros as conflicts. **Keep
hotkeys** is unticked and the preview shows no hotkeys. Tick it: each macro with a hotkey shows it
after its name (`Name  [Ctrl+F1]`). Click **Paste from clipboard** again: **Keep hotkeys** is
unticked again. Set one macro to Replace and one to Skip, and click **Import selected**. The result
matches the preview, and the imported macros (the replaced one too) have no hotkeys.

**J3. Undo.** **Undo import**: the library is exactly as before (names, lines, hotkeys, filters).

**J3b. Choosing items.** Copy `Test` to the clipboard again and paste it into the Import window.
Every row has a checkbox, all ticked. Untick `Test`: everything inside unticks, rows say "not
selected", and the summary counts them. Tick just `Say`: `Test` ticks again with "[some]", and
only `Say` is ticked inside. Taken names show **NAME TAKEN** with their choice box. **Deselect
all**: **Import selected** is greyed out. **Select all**, then untick one macro and click **Import
selected**: everything but that macro arrives. **Undo import**.

**J4. Files and folders.** Copy `Say.json` into `imports\` and open **Import...**: it's listed and
loads when clicked. Make a folder in `imports\` with `.txt` files in two levels of sub-folders,
and import it into the deepest test folder (level 6): the preview leaves the too-deep folders
out, or with "Move the contents of too-deep folders up a level" ticked, their macros land one
level up. In the path box, paste a path with quotes (Explorer's "Copy as path") and press Enter:
it loads.

**J5. Focus.** **Open imports folder**: Explorer opens. Back in the game: keys and Tab work.
With the Import window open, hotkeys still work, and typing in its path box doesn't trigger them.

**J6. Trash bin stays out.** With something in the trash bin: right-click a folder > Export >
the menu says items in the trash bin aren't exported; **Export all**'s tooltip and the message
after exporting say the same. Then copy `macros.json` from the addon folder into `imports\` and
open it in the Import window: a warning says its trash bin isn't imported, and the preview shows
only the library. Cancel.

**J7. A whole library.** Copy `macros.json` from the addon folder into `imports\` and rename the
copy `Alex.json` (pretend a friend sent it; give one of its macros a hotkey first). Also copy
`macros.json.bak` into `imports\`. Open **Import...** and pick **A whole library**: the list shows
only `.json` and `.bak` files (no folders or `.txt`). Click `Alex.json`:
- step 2 asks for the new folder's name, filled in as `From Alex`; the "Import into" box and the
  "name is taken" choices aren't shown;
- the preview has one new folder, `From Alex`, with your whole library inside it, in order;
- **Keep hotkeys** is unticked; a warning says its trash bin isn't imported (if yours has items).
Rename it `Friend`, click **Import selected**: a top-level `Friend` folder holds the copy, with no
hotkeys and no text file links. Import `Alex.json` again with the name `Friend`: the preview
marks it NAME TAKEN, "new, as "Friend (2)"". Click `macros.json.bak`: it loads, named
`Imported library`. Pick a `.txt` file's path in the path box: refused ("A library file is a
macros.json..."). If your library is 6 folders deep, the deepest folder is shown as too deep (it
would be 7 deep inside the new folder). Switch back to **Macros**: the list shows `.txt` files and
folders again. **Undo import** removes the copies. Close and reopen the Import window: it's on
**Macros**.

**J8. A file that isn't UTF-8.** In Notepad, write `/s Käse für dich` and **Save as**
`imports\ansi.txt` with Encoding **ANSI**. Open it in the Import window: a warning says it isn't
saved as UTF-8, and the preview's macro has `�` (or `?`) where `ä` and `ü` were. Import it,
then rename any macro: no "couldn't be saved" message. Right-click the imported macro > Export >
**Save as a .json file** and **Copy to the clipboard**: both work, and the game stays open.
**Undo import**.

**J9. Many warnings.** In PowerShell, make a file with 5000 broken entries:
`'{"format":1,"root":{"children":[' + ((1..5000 | % { '1' }) -join ',') + ']}}' | Set-Content -Encoding ascii imports\broken.json`
(run it in the addon folder). Open it in the Import window: it lists 200 warnings, then
"...and 4800 more problem(s) like these.", and says there's nothing to import. The game doesn't
slow down while the window is open. Close it and delete `broken.json`.

**J10. Preview marks.** Give `Say` the hotkey Ctrl+F1. Make a macro `Risky` in `Test` with the
line `/s {txt emote.txt}`, and a macro `Plain` with hotkey Ctrl+F1 too (yes, the same) and the
line `/s plain`. Export `Test` to the clipboard, then delete `Risky` and change `Plain`'s hotkey to
Ctrl+F2. **Paste from clipboard** with `Test`'s parent as the target, and set macros to
**Replace**:
- `Risky` is marked **READS A FILE**, and a line above the preview counts it.
- Tick **Keep hotkeys**: `Plain` (Ctrl+F1, as `Say` has) is marked **COMES WITHOUT ITS HOTKEY**
  and **YOUR HOTKEY GOES**, with their lines above the preview. Hover each mark: a reason shows.
- **Import selected**: `Plain` has no hotkey now, `Say` keeps Ctrl+F1, and the line with **Undo
  import** says 1 replaced macro has no hotkey now, or a different one. **Undo import**.
- Set macros to **Keep both (rename)** and paste again: `Plain (2)` is marked **YOUR HOTKEY**
  (it keeps it if imported). Close the window without importing.

## K. Text file links

Have Notepad ready. "Within a second" means after you save, not after you switch back.

**K1. New file.** On `Two`, open **Text file link**, type `test.txt`, click **Link**:
`<GW2>\addons\GW2_Chat_Messages\links\test.txt` appears with `Two`'s lines, and the status says the
file was created.

**K2. Both ways.** **Open file**, add `/s from notepad` and save: within a second `Two` shows it
(Lines and Raw text). Edit `Two` in the addon: within a second the file changes (Notepad asks to
reload when you switch to it). Keep Notepad open on the file while you type in the addon's
message box: no stutter, nothing lost, hotkeys still work.

**K3. Existing file and duplicates.** Make `links\other.txt` with one line, link `Say` to it:
`Say` now has that line. Link `Emote` to `TEST.txt`: refused, the error window says it's "already linked".

**K4. Gone and back.** Delete `test.txt`: the editor says the file isn't there. Edit `Two`: the
file is **not** recreated. Click **Recreate the file**: it's back with `Two`'s lines.

**K5. Edited while closed.** Close the game, edit `test.txt`, start the game: `Two` has the new
lines. **Unlink**: the macro and the file both stay; editing either no longer changes the other.

**K6. A file that isn't UTF-8.** Make `links\ansi.txt` in Notepad with `/s Käse`, saved with
Encoding **ANSI**. Make a new macro `Ansi` in `Test` and link it to `ansi.txt`: it shows `�` (or
`?`) for `ä`, and the library still saves (no "couldn't be saved" message). Wait a few seconds
without editing: the file isn't rewritten (Notepad doesn't ask to reload). Edit `Ansi` in the
addon: the file is rewritten as UTF-8, with `�` for the `ä`. Delete `Ansi`.

## L. Final checks

**L1. Game input after everything.** Movement keys, skills, Tab targeting, Shift/Ctrl/Alt combos,
the keys you used as hotkeys (doing their normal game action), left/right click, camera turning,
and the chat key all work normally. No key feels stuck.

**L2. Unloading.** Send `Long` and, while it's sending, unload the addon in Nexus > Addons: the
game doesn't freeze and the unload finishes within about a second. Do the same right after sending
the `{json ...build}` macro: done within a few seconds. Load the addon again: everything's there.

## M. Text size

**M1. The choices.** Open the **Text size** dropdown at the end of the library's toolbar. Note
every entry and its size (question: which fonts and sizes does your Nexus offer?). Pick the
biggest: within a frame the library's text, buttons, title bar and window all get bigger, and the
window stays on screen. Pick the smallest, then the biggest again: the window shrinks and grows
back to about the same size.

**M2. Everything follows.** At the biggest size, check: the tree, the macro editor (lines, Raw
text, Where it's active, Text file link), the Rename and Delete pop-ups, the Import window, the
quick menu pop-up, the Quick Access right-click menu, and tooltips (hover a macro in the quick
menu, the Placeholders (?) label). All use the big text. Nexus' own options page doesn't.

**M3. Nothing cut off.** At the biggest size, make the library window narrow:
- toolbar buttons that don't fit move to a second row;
- status messages, long hints and the error window's text wrap instead of running off the edge;
- a macro with 6+ maps in its filter shows the map buttons over several rows;
- a long macro name deep in the tree can be reached with the tree's sideways scrollbar.
Report anything that's still cut off, with a screenshot.

**M4. Saved.** Restart the game: the same size is used. `settings.json` in the addon folder has
`"text_size"`. Change the size from the addon's page in Nexus options: the library follows.

**M5. Font reload.** Change Nexus' own font or UI settings (if it has any) while the library is
open: no crash, and the library keeps working (it may show the default size for a moment).

## N. The error window

These look at the error window itself. The other sessions have already opened it many times:
note anything odd you saw there too.

**N1. Looks and buttons.** In the library, make a second folder called `test` next to `Test`:
an error pop-up opens over the name box with the error (wrapped, not cut off), "What you did:
Created a folder named "test" in ...", **OK** and **Copy to clipboard**. Click Copy to
clipboard ("Copied." shows), paste into Notepad: the version with its build stamp, time, what you did, the error and
"In game: ..." are there (paste it in your report). OK: back in the name box.

**N2. Queue and the game.** Give macros `Say` and `Two` the hotkey Ctrl+F9, and two other macros
Ctrl+F10 (all active here). Close the library and press Ctrl+F9 twice, then Ctrl+F10: a Nexus
alert shows each time, and the error window appears without taking focus. While it's open the
game keeps all its input (move, click to target, turn the camera, skills). It says "Error 1 of
2" (the repeated Ctrl+F9 is kept once); OK shows the next one, OK again closes it. Put the
hotkeys back afterwards.

**N3. Text size.** At the biggest text size the error window and its text get bigger and nothing
is cut off.

## O. Trash bin

Turn on (if you turned it off) Nexus > Options > Addons > Chat Messages > **Use a trash bin for
deleted folders and macros**, and read its description. Make a scratch folder `Bin test` with a
sub-folder `Sub` and a macro `Junk` (one line) inside `Sub`.

**O1. Delete and restore.** Select `Junk`, click **Delete**: no question, it's gone from `Sub`, the
message under the toolbar says it's in the trash bin, and **Trash bin (1)** at the bottom of the
tree holds it, counted as **Trash bin (0/1)** (folders/macros). Click it: the details show when and where from, its line, and no editor. Its
hotkey (give it one first, e.g. Ctrl+F11) does nothing and it's not in the quick menu.
Right-click it > **Restore**: it's back in `Sub`, in its old place, and its hotkey works again.

**O2. Folder gone.** Delete `Junk`, then delete `Sub` too. In the bin, right-click `Sub` >
**Delete permanently...** (it asks; confirm). Now restore `Junk`: a pop-up says its folder no
longer exists. **Recreate the folders**: `Bin test > Sub > Junk` is back.
Do it again and choose **Put it at the top level**: `Junk` is at the top level.

**O3. Name taken and drag.** Make a new macro `Junk` in `Sub`, delete it, make another `Junk`
there and restore the deleted one: it comes back as `Junk (2)` and the message says so. Drag a
macro onto **Trash bin**: it's moved to the bin.
*Long name:* name a macro with 100 characters (paste `aaaaaaaaaa` ten times), delete it, make
another with the same name and restore the first: the game doesn't freeze, and it comes back
named with 96 `a`s and ` (2)`. Export that macro and import the file into the same folder: the
preview offers it as a new name ending ` (2)` (or ` (3)`), and the import works.

**O4. Empty and saved.** Delete a couple of items, restart the game: they're still in the bin.
Right-click the bin > **Empty trash bin...**: it asks, then the bin is empty.

**O5. Turned off.** Untick the setting: Delete asks first and removes for good; items already in
the bin stay, and the bin still shows until emptied. Tick it again. Delete `Bin test` when done.

## P. Search box

Use the `Test` folder from A3 (with `Say` on Ctrl+F1 from F1, and the `Accents` macro).

**P1. Finding by name.** Type `say` in the **Search (Ctrl+F)** box above the tree: only `Say`
shows, with `Test` (and any folders above it) opened, and "say" highlighted in its name. The line
above the tree says "Found: 0 folder(s), 1 macro(s)". Type `SAY`: the same (case doesn't matter).
Type `test`: the `Test` folder matches and shows everything inside it. Type `zzz`: "Nothing
matches "zzz"."

**P2. Lines, recipients, hotkeys, accents.** `test map` finds `Two` with "(found in its lines)";
your own character name finds `Whisper` (its recipient); `ctrl+f1` finds `Say` with "(found in its
hotkey)"; `hello` finds `Accents` (its line says `héllo`: accents don't matter).

**P3. While searching.** With a search typed: dragging an item does nothing; right-click > **Move
up** and **Move down** are greyed out; the trash bin isn't shown. Folders that were closed before
the search open to show matches. Select `Say`, then change the search so `Say` no longer shows: it
isn't selected any more. Rename a match or edit its lines: the tree updates.

**P4. Clearing and keys.** Click the **x** next to the box: the whole tree is back, with the same
folders open and closed as before the search, and the trash bin shows again. Click into the
library window (not the box) and press **Ctrl+F**: the cursor goes into the box. Type something
and press **Escape**: the box is cleared and the full tree is back. **(question)** Does Ctrl+F or
that Escape also do anything in the game (Escape opening the game menu, for example)? While typing
in the box, macro hotkeys don't fire (try Ctrl+F1).

**P5. Text size and narrow windows.** At the biggest text size, make the library narrow: the box
and its **x** stay inside the tree pane, the box gets narrower, and nothing is cut off.

**P6. Only active here.** Give `Emote` a PvE-only filter (as in H1). In the open world, look at
the checkbox under the search box: it names where you are, e.g. **Only active in PvE on Queensdale
(15)**. Tick it: `Emote` still shows, and the line above the tree says how many macros are active
here. Go into WvW (or the Edge of the Mists): the label changes to **Only active in WvW on ...**,
`Emote` is gone, and folders with nothing active are hidden. With the box ticked, type `emo`:
"Nothing active here matches". Restart the game: the box is still ticked. Untick it and clear
`Emote`'s filter afterwards. On the character select screen (or while loading), note what the
label says.

**P7. Search in the quick menu (pop-up).** Open the quick menu with its key: the cursor is
already in the search box at the top. Type `say`: the menu becomes a flat list, `Say  (Test)`,
coloured by channel, with its hotkey on the right and the line preview on hover. Press **Enter**:
`Say` is sent and the menu closes. Open it again: the box is empty and the folders are back. Type
`zzz`: "No macro active here matches". **(question)** While typing in the box, did any letter, or
the Enter, also reach the game (chat opening, a skill, movement)?

**P8. Search in the Quick Access menu.** Right-click the scroll icon: the search box is there but
not focused. Click it and type `say`: the same flat list. Click `Say` to send it. Close and reopen
the menu: the box is empty. A macro that isn't active here (P6's `Emote` in WvW) never shows in the
results.

## Q. Languages

Languages are on the addon's page in **Nexus > Options**, under **Text size**. The German, French
and Spanish texts were made by AI: if you speak one of them, note any text that reads oddly.

**Q1. Following Nexus.** The **Language** dropdown shows **Follow Nexus (English (US))**. In
Nexus' own options, switch Nexus to Deutsch: within a second or two the addon's windows (library,
quick menu, this page) switch to German, and the dropdown shows **Nexus folgen (Deutsch)**. Switch
Nexus back to English: the addon follows. Set Nexus to a language the addon doesn't have (e.g.
Italiano): the addon stays in English, the dropdown shows **Follow Nexus (it)**, and a line under it says there's no translation yet. Set Nexus back.

**Q2. Picking a language.** Pick **Français**: everything switches to French at once, while
Nexus stays in English. Restart the game: still French. Pick **Follow Nexus** again afterwards.

**Q3. German in the windows.** With German on, open the library, the macro editor, the Import
window, the trash bin's pop-ups and the error window (e.g. a duplicate name). Note anything cut
off, overlapping or running out of a window, at the default and the biggest text size.

**Q4. Accented letters.** In French and German, letters like é, è, à, ç, ö, ü and ß show
correctly (no `?` boxes) in the library, the menus and the tooltips.

**Q5. Names in Nexus.** With Nexus set to Deutsch, **Nexus > Options > Keybinds** shows the
addon's two keybinds in German, and hovering the scroll icon shows a German tooltip.

**Q6. A file of your own.** Click **Open localization folder**: Explorer opens
`<GW2>\addons\GW2_Chat_Messages\localization\`. Copy `localization\template.json` from the
repository into it as `nl.json`, set `"Identifier": "nl"`, `"DisplayName": "Nederlands"`, and
translate one text, e.g. `"Open macro library": "Macrobibliotheek openen"`. Click **Reload
language files**: **Nederlands (0%) - file from your localization folder** is in the list. Pick
it: that one text is in Dutch, the rest in English.

**Q7. A bad file.** In `nl.json`, change a text with a `%s` so the `%s` is missing (e.g. `"Sent
%s from the quick menu": "Verzonden"`), then click **Reload language files**: the error window
says one text was left out, and that text shows in English. Then break the file (delete the last
`}`) and reload: the error window says the file wasn't loaded and why, and the dropdown falls
back. Delete `nl.json` and reload afterwards.
## Report template

```
Build: (stamp from the log)   Nexus version:   Game language:
A1  A2  A3  A4  A5  A6
B1  B2  B3  B4 (what arrived):
C1  C2 (did /w Name work without Tab?):
D1  D2  D3  D4  D5  D6
E1
F1  F2 (which mouse buttons, where):  F3  F4  F5  F6  F7
G1  G2  G3 (where did it appear?):
H1  H2  H3  H4 (map: mode, map id):
I1 (date format sent):  I2  I3  I4  I5  I6  I7
J1  J2  J3  J3b  J4  J5  J6  J7
K1  K2  K3  K4  K5
L1  L2
M1 (entries and sizes):  M2  M3  M4  M5
N1 (pasted text):  N2  N3
O1  O2  O3  O4  O5   A4 colours easy to see?
P1  P2  P3  P4 (did Ctrl+F / Escape reach the game?):  P5  P6 (label at character select):  P7 (did typing or Enter reach the game?):  P8
Nexus log warnings:
```
