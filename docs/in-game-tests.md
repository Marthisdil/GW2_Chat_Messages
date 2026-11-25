# In-game tests

The unit tests cover the rules (tree, raw text, send plan, hotkeys, filters, placeholders,
import, link sync). These checks cover what only the game can tell us: whether GW2 takes our keys
and text, and whether normal game input is ever disturbed.

The checks are grouped into sessions (A to R) in the order to run them: each one builds on the
ones before (the `Test` macros from A3, `Say`'s Ctrl+F1 from F1, `Fill` from I1, ...). A
whole pass takes a few hours, so spread it over several play sessions. If the text is hard to
read, do M1 first and pick a bigger size. Session B is the one that matters most: if it fails,
most of the later sessions can't be run, so report it first.

## Progress

Results so far (keep your notes in `docs\testing results.txt`; it isn't committed):

| Session | State | Notes |
|---|---|---|
| A1 | Passed (PR #15 build) | Re-check with the current build: the log and the options page show its version and build stamp, the two links and the folders are new (A1's last part), and so is the icon (the quick bubble, 2026-11-25). |
| A2 | Passed, 2 fixes to re-check | The duplicate-name error and the Delete pop-up's text ran off the right. Fixed: text wraps and errors open the error window. Re-check those two points only (marked *Re-check* in A2), then go on to A3. |
| A3 to R | Not run yet | Everything after A2 is new since the PR #15 build that was tested, so run it all with the current build (H9 and H10, filters on folders, are the newest; then J11, export a selection; G4 to G6, favourites and recently sent, with G4 and G6 new for favourites shown everywhere, and R, backups and restore, are the newest; A1b, the example macros, and F9, the hotkey cooldown, came just before). |

## How to report

For every check, write **pass**, **fail** or **skipped**, and for a fail (or anything odd):

- what you did and what happened, in your own words;
- the **exact error text**: click **Copy to clipboard** in the error window and paste it (for
  a hotkey error, the Nexus alert's text too);
- the Nexus log lines mentioning `Chat Messages` (Nexus > Log; filter by "Chat Messages");
- for anything visual, a screenshot (save it anywhere, not in `docs\images\`, which is for the
  README and wiki pictures).

Every refusal and error the checks expect ("refused", "says why", "a message") opens the
**error window**. Click **OK** to close it before the next step. Errors from a hotkey or the quick
menu also show a Nexus alert.

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
The chat bubble icon (a speech bubble with a lightning bolt, since 2026-11-25; re-check it with
a build from then on) is in the Nexus bar, sharp at the bar's size, and turns cream while the
mouse is over it. Clicking it opens and closes the library; so does your
library key. Nexus > Options > Addons > Chat Messages shows the version, the build stamp under it
("Build abc1234, ..."), and an **Open macro library** button that works.
Under the build stamp: **Chat Messages on GitHub** and **ArenaNet's macro policy** in the link
colour, underlined. Hovering one shows the hand cursor and its address; clicking it opens that
page in your browser (the repository; ArenaNet's "Policy: Macros and Macro Use"). Back in the
game, keys and Tab work. The line under them says the addon only sends messages you start.
In the addon folder, `imports\` and `exports\` exist (delete both before starting the game to
check they're created).

**A1b. Example macros.** Close the game. In the addon folder, rename `macros.json` to
`macros.keep.json` and `macros.json.bak` to `macros.keep.bak` (if you have them), and in `settings.json` delete the line
`"examples_added": true` (keep the file valid JSON). Start the game: the library has an
**Examples** folder with Hello, Basics, Placeholders and Filters (Filters has In WvW and In
Lion's Arch inside), in the addon's language, and no macro has a hotkey. Preview `Two channels`
and `Roll a die`. In PvE, `Stack on tag` is dimmed (WvW only) and `Meet at the bank` too unless
you're in Lion's Arch. Delete the Examples folder and restart: it doesn't come back. On the
addon's page in Nexus options, click **Add the example macros**: the library opens and asks "Add
the examples again?" (they were added before); **Add**: a new Examples folder, selected. Rename it
to `Mine`, move it into another folder, and add a macro of your own inside it. Click the button
again: "Replace the examples?" names `Mine`, counts its folders and macros (yours included) and
says the old one goes to the trash bin. **Cancel**: nothing changes. Again, **Replace**: a fresh
Examples folder in `Mine`'s place (same folder), and `Mine` with your macro is in the trash bin.
No "Examples (2)" anywhere. Restore `Mine` from the bin: it comes back beside the new one; click
the button: the prompt names the new Examples folder, not `Mine`. Turn the trash bin off in the
options: the prompt says the old folder is deleted for good. Then close the game, delete
the new `macros.json` and `macros.json.bak`, put `macros.keep.json` and `macros.keep.bak` back
under their old names, and start again: your own library, no examples added.

**A2. Building the tree.** In the library:
- **New folder** (toolbar) → name `Test`. In it, **New folder inside** five times, one inside the
  next, so the deepest is 6 folders deep. A 7th folder inside the deepest one is refused with a
  "levels deep" message: "New folder inside" isn't offered there, so select the deepest folder
  and click **New folder** in the toolbar. A macro inside the deepest folder is allowed.
- A second folder or macro called `test` (any case) in the same folder is refused ("There is
  already a ..."). *Re-check: the error opens in a window and isn't cut off* (N1 looks at that
  window in detail).
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

**B3b. Preview.** Select `Two` and click **Preview** (next to **Send now**): under the buttons,
each line with its channel and exactly what would be typed (`/s ...`, `/p ...`); nothing appears
in chat. Change a line: the preview goes away. Preview a whisper macro: `/w Name`, then "then
Tab, then:", then the message. Preview `Fill` with `/s {map} {random 1 6}`: the map name and a
number. Preview a line over the limit once filled in (I4): the error window opens ("Clicked
Preview for ... (nothing was sent)"), the same message Send now gives. **Close preview** hides
it. Back in the game, keys and Tab work.

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
Last, **Set...** and click the left mouse button, then **Set...** and Ctrl+right-click: both are
refused, and the error window says the left and right buttons can't be hotkeys even with Ctrl,
Alt or Shift, and why. The click itself still does its normal job.

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

**F8. GW2's default keys.** On `Fill` (or any spare macro), set **F1**: a yellow note under the
hotkey says `In GW2's default keys, F1 is "Profession skill 1"`. Set **Ctrl+T**: "Call target".
Set **Alt+1**: a squad location marker. Set **Ctrl+1**: no default-key note. Set a plain letter
that isn't a default key (for example **K**): the older "plain letters are usually game keys"
note shows instead. Clear the hotkey afterwards.

**F9. Cooldown.** On `Say` (Ctrl+F1), set **Cooldown:** to `10`. Close the library and press
Ctrl+F1: `Say` is sent. Press it again at once: nothing is sent and one Nexus alert says
`Say: cooldown, wait N s.` Press it a few more times: still nothing, and no more alerts. Open the
library: the row shows `(N s left)`, counting down. During the cooldown, **Send now** and the
quick menu send `Say` anyway (and start the cooldown again). After 10 seconds Ctrl+F1 works
again. Set the cooldown to `0`: press Ctrl+F1 twice within a second: sent once only, no alert;
after a second it sends again. Type `9999` in the box: it becomes `600`. Reload the addon: the
cooldown is still set (and the hotkey works at once). Set it back to `0`.

## G. Quick menu

**G1. Right-click menu.** Right-click the chat bubble icon: the library shows as nested menus (empty
folders hidden). Hovering a macro shows its lines; clicking it sends it and closes the menu.
Put a folder and a macro with the same name in one folder: both work.

**G2. Pop-up key.** Press your quick menu key: the menu opens at the mouse pointer. Press it again,
or click outside: it closes. Send something from it.

**G4. Favourites.** In the library, tick **Favourite** in one macro's editor, and right-click
another > **Favourite**: both show `*` after their name. Open the quick menu: **Favourites** at
the top lists both (with their folders in brackets), above the folders. Untick one: it's gone
from the list. Give a favourite a filter that's off here (e.g. PvP only): it's still listed,
faded, its preview says it isn't set for this map, and clicking it sends it. Its hotkey doesn't
fire here.

**G6. Favourite folders and the limit.** Select a folder: its details have a **Favourite** box;
tick it (or right-click the folder > **Favourite**): the folder shows `*`. In the quick menu it's
under **Favourites** as a sub-menu with everything inside it, sub-folders too; macros filtered
out here are faded and still send. Give every macro in it a filter that's off here: the folder
itself is faded, and on a map where nothing at all is active the quick menu still shows
Favourites. Mark favourites until there are 10, then try an 11th from the box and from the
right-click menu: the error window says you can have up to 10, and nothing changes. Restart the
game: the folder is still a favourite.

**G5. Recently sent.** Send three macros that aren't favourites: one by hotkey, one from the
quick menu, one with Send now. **Recently sent** lists them newest first, under the favourites.
Send a favourite: it isn't added under Recently sent. Restart the game: the list is still there.
On the options page (General tab), set **Recently sent in the quick menu** to 1: only the newest
shows; set 0: none. Set it back to 5, then **Clear the list**: it's empty. Delete a macro that
was listed: it drops out. Type in the menu's search box: both lists are hidden while you search.

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

**H5. Finding a map.** On `Emote`'s **Where it's active**, type `lions` in **Find a map by name or
id**: "Lion's Arch - ..." is listed with its region and id; click it and it's added. Type `15`:
Queensdale is found by its id. A map already in the list is greyed out. Type `zzzz`: nothing
listed, no error. Remove the maps afterwards.

**H6. Maps you've been to.** Visit two or three maps, then open **Maps you've been to**: they're
listed newest first, marked "(been there)" in the search results too. Picking one adds it.
Restart the game: the list is still there.

**H7. A map the API doesn't list.** **Add id** `99999`: it's shown in yellow with **(!)**, and
hovering it says the GW2 API doesn't list that map. Remove it.

**H8. The map list on the options page.** On the addon's page in **Nexus > Options**: "Map list:
N maps, from (date)." Click **Refresh map list**: it says it's loading, then shows the new date,
and the game doesn't stutter. Disconnect from the internet and click it again: the old count stays
and a yellow line says the refresh failed and why. Reconnect.

**H9. Filters on folders.** **New folder** `Roam`: the name box has **Active in:** PvE, WvW,
PvP, all ticked; try to untick all three: the last one stays. Untick PvE and PvP and create it:
the tree shows `Roam  [WvW]`. In PvE it's dimmed, with everything in it. Move `Say` into it:
`Say` is dimmed in PvE, its hotkey does nothing there and it's gone from the quick menu, and its
editor says "Also limited by the folders above it: Roam (WvW)". Go to WvW (or Armistice
Bastion): all of it is active, not dimmed, and the hotkey sends. In `Say`'s filter, tick only
PvE: the editor warns it can't be active anywhere. Put it back to all modes.

**H10. Folder maps, sub-folders and sharing.** Select `Roam`, open **Where it's active** in its
details, and **Add this map**: the tag becomes `[WvW, 1 map]`. Make a sub-folder inside it with
the map of a different WvW borderland: the sub-folder and everything in it are never active
(its details warn), and it's dimmed everywhere. Remove that map again. Rename `Roam`: the name box
shows WvW ticked; tick PvE too and rename: the tag says `[PvE, WvW, 1 map]`. Give `Say` (in
`Roam`) and a macro outside it the same hotkey, the outside one PvE only, and untick PvE on
`Roam` again: no clash warning (they can never be active together). Export `Roam` and import
it as a copy (**Keep both**): the copy has the same tag. Restart the game: the folder filters are
still there. Move `Say` back out of `Roam` and delete the test folders.

## I. Placeholders

Make a macro `Fill` and change its line for each check.

**I1. Time and random.** `/s It's {time} on {today}, roll {random 1 6}`: the real time and date;
send it a few times, the roll stays 1-6. Note the date format as sent (question: is the weekday
in your Windows language, and is the format what you'd want?).

**I2. Removed placeholders.** `{txt}` and `{json}` were removed. Change `Fill`'s line to
`/s hello {txt names.txt}`: a red note under the line says they aren't available any more. Click
**Send now**: nothing is sent, and the error window says `Line 1: {txt} isn't available any
more: edit this line to remove it.` The same with `{JSON id https://api.guildwars2.com/v2/build}`
(any case). `/s {txts a}` is sent as typed (only those two names are refused). Hover
**Placeholders (?)**: neither is in the list.

**I3. Left alone.** `/s {smile} meet at [&BDAEAAA=]`: `{smile}` arrives as typed, and the code
arrives as a clickable waypoint link.

**I4. Too long once filled in.** Make `Fill`'s line 180 characters (paste `1234567890` 18
times) followed by ` {today}`: the counter shows `~` before the number. **Send now**: refused,
the error window says it's too long, nothing sent.

**I5. Map name.** `/s Hello from {map}`: sends the map name, also right after you arrive on a
map you haven't visited this session.

**I6. Offline.** Close the game, delete `maps.json`, disconnect from the internet, start the game
(Nexus may complain about updates; that's fine) and enter a map. `{map}` fails, the error window says the API
"couldn't be reached", and nothing is sent. The game doesn't stutter while it tries. Reconnect.

**I7. Typing while it prepares.** Close the game, delete `maps.json`, start it again and, as soon
as you're on a map, send `/s Hello from {map}`; while the status says "Preparing...", open chat
and start typing (if "Preparing..." is gone too fast to catch, mark this skipped). The send stops (error window: "A text box is open") and
**your chat box stays open with your text**. The error window doesn't take your typing away.

**I8. A damaged map cache.** Close the game. In `maps.json` in the addon folder, change
the number after `"format":` to `"x"` (with the quotes) and save. Start the game and enter a map: no error, and
`/s Hello from {map}` still sends the map name (the cache is rebuilt).

**I9. Map names in another language.** Set the addon's language to **Deutsch** (Q2) and wait about
10 seconds: in Lion's Arch, `/s Hello from {map}` sends `Löwenstein`, and the map search finds
`löwen`. Switch back to English: after a few seconds the names are English again.

## J. Import and export

**J1. Export.** Right-click `Say` > Export > **Save as a .json file**, twice: `exports\Say.json`
and `exports\Say (2).json` appear and the status names the file, with an **Open exports folder**
button under it that opens the folder (it goes away with the message). The toolbar's **Exports
folder** opens it too. **Save as a .txt file** and
**Copy the lines as text** give the raw lines, one per row (paste into Notepad to check).

**J2. Import with conflicts.** Right-click the `Test` folder > Export > **Copy to the clipboard**.
Then toolbar **Import...** > **Paste from clipboard**, with `Test`'s parent as the target. The
preview shows `Test` as "goes into the existing folder" and the macros as conflicts. **Keep
hotkeys** is unticked and the preview shows no hotkeys. Tick it: each macro with a hotkey shows it
after its name (`Name  [Ctrl+F1]`). Click **Paste from clipboard** again: **Keep hotkeys** is
unticked again. Set `Say` to Replace (it's marked **YOUR HOTKEY GOES**: it has Ctrl+F1 from F1,
and the copy comes without it) and one other macro to Skip, and click **Import selected**. The
result matches the preview, and the imported macros (`Say` too) have no hotkeys.

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

**J9. Crafted files.** Four files made to slow the game down; none should. Run these in
PowerShell in the addon folder:
- 5000 broken entries:
  `'{"format":1,"root":{"children":[' + ((1..5000 | % { '1' }) -join ',') + ']}}' | Set-Content -Encoding ascii imports\broken.json`.
  Open it in the Import window: it lists 200 warnings, then "...and 4800 more problem(s) like
  these.", and says there's nothing to import. The game doesn't slow down while the window is
  open. Close it.
- A line full of `{`: `'/s ' + ('{' * 200000) | Set-Content -Encoding ascii imports\braces.txt`.
  Open it in the Import window and leave the preview up for a few seconds: no stutter. Import it,
  select the macro so the editor shows it: still no stutter. **Send now**: refused at once as
  too long. Delete the macro.
- A macro with 100,000 lines (SEC-034):
  `'{"format":1,"root":{"children":[{"type":"macro","name":"Long","lines":[' + ((1..100000 | % { '{"channel":"say","message":"x"}' }) -join ',') + ']}]}}' | Set-Content -Encoding ascii imports\long.json`.
  Open it in the Import window: a warning says `"Long" has more than 50 lines. Only the first 50
  were kept.` Import it and select `Long`: the editor shows 50 lines and the game doesn't
  stutter. Delete the macro.
- A whisper with a 1,000,000-character recipient (SEC-035):
  `'{"format":1,"root":{"children":[{"type":"macro","name":"Far","lines":[{"channel":"whisper","whisper_to":"Bob' + ('x' * 1000000) + '","message":"hi"}]}]}}' | Set-Content -Encoding ascii imports\far.json`.
  Open it in the Import window: a warning says `"Far": a whisper recipient was longer than 29
  characters and was shortened.` Import it and select `Far`: the recipient is `Bob` and 26 `x`.
  Type one more letter in **Recipient**: a red note says it's longer than 29 characters, and
  **Send now** is refused without opening chat. Delete the macro.

Delete `broken.json`, `braces.txt`, `long.json` and `far.json` afterwards.

**J10. Preview marks.** Give `Say` the hotkey Ctrl+F1. In `Test`, make these macros: `Old`
with the line `/s {txt notes.txt}`; `Key` with the line `/s key` and the plain hotkey F8; and
`Plain` with hotkey Ctrl+F1 too (yes, the same as `Say`) and the line `/s plain`. Export `Test`
to the clipboard, then delete `Old` and `Key` and change `Plain`'s hotkey to Ctrl+F2. **Paste
from clipboard** with `Test`'s parent as the target, and set macros to **Replace**:
- `Old` is marked **REMOVED PLACEHOLDER**, and a line above the preview counts it.
- Tick **Keep hotkeys**: `Key` (a new macro) is marked **KEY WITHOUT CTRL/ALT/SHIFT**; it would
  keep F8. `Plain` (Ctrl+F1, as `Say` has) is marked **COMES WITHOUT ITS HOTKEY** and **YOUR
  HOTKEY GOES**. Each mark has its line above the preview; hover each mark: a reason shows.
- **Import selected**: `Plain` has no hotkey now, `Say` keeps Ctrl+F1, and the line with **Undo
  import** says 1 replaced macro has no hotkey now, or a different one. Select `Old`: the red
  note shows under its line, and **Send now** refuses it. **Undo import**.
- Set macros to **Keep both (rename)** and paste again: `Plain (2)` is marked **YOUR HOTKEY**
  (it keeps it if imported). Close the window without importing. Delete `Old`, `Key` and `Plain`
  if they're still there.

**J11. Export a selection.** Select the `Test` folder and click **Export some...** in the
toolbar: the **Export** window opens with `Test` and everything in it ticked, and the count says
how many. Untick one macro inside `Test`: `Test` shows **[some]**. Tick `Say` (outside `Test`) and
a macro inside a sub-folder somewhere else: its folders tick too. Type `My pick` as the file
name and **Save as a .json file**: the window and the library say `Exported to ...My pick.json`;
save again: `My pick (2).json`. Open **Import...** with that file: the preview has exactly the
ticked items, in their folders, and nothing else. Close it without importing. Back in the Export
window, **Copy to the clipboard** and paste into **Import...**: the same. **Deselect all**: the
count is 0 and the save buttons are greyed out. Right-click a macro > Export > **Choose more to
export...**: the window starts again with only that macro ticked. While the window is open,
rename and delete a ticked macro in the library: nothing breaks, and the deleted one isn't
exported. Keys and Tab still work in the game afterwards.

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
`{map}` on a map that isn't looked up yet (delete `maps.json` first): done within a few seconds. Load the addon again: everything's there.
Then unload and load it about five times with nothing sending (SEC-036): each unload finishes at
once.

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

Turn on (if you turned it off) Nexus > Options > Addons > Chat Messages > General > **Use a trash bin for
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

**P8. Search in the Quick Access menu.** Right-click the chat bubble icon: the search box is there but
not focused. Click it and type `say`: the same flat list. Click `Say` to send it. Close and reopen
the menu: the box is empty. A macro that isn't active here (P6's `Emote` in WvW) never shows in the
results.

## Q. Languages

Languages are on the addon's page in **Nexus > Options** (the **General** tab), under **Text size**. The German, French
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
addon's two keybinds in German, and hovering the chat bubble icon shows a German tooltip.

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

**Q8. Messages and key names.** With German on, the hotkey row and the tree show key names in
German: **Ctrl+Mouse 4** shows as `Ctrl+Maus 4` and Page Up as `Bild auf` (Ctrl, Alt and Shift
keep their names). Make a duplicate name, send a macro with the Chat key cleared, and import a
damaged file: each error window message is in German. Searching the library for `maus 4` and for
`mouse 4` both find the macro with that hotkey. `macros.json` still stores the hotkey in English.

## R. Backups and restore

Open the addon's page in **Nexus > Options**. Keep a UTC clock at hand (search "UTC time" on the
web) to compare the times.

**R1. The tabs.** The page has **General**, **Backups** and **Restore** tabs. General has
everything that was on the page before (the buttons, text size, language, trash bin, map list);
the version, build and links stay above the tabs. `<GW2>\addons\GW2_Chat_Messages\backups\`
exists.

**R2. The safety copy (backups off).** With **Make backups of my macros** unticked (the default)
and a library that isn't empty, play for a minute: `backups\` has one
`macros_safety_<date>_<time>_UTC.json`, with the current UTC date and time. The Backups tab says
backups are off, explains the weekly safety copy (4 kept), and shows "Last safety copy: ... UTC".
No message or alert appeared in game.

**R3. Backups on.** Tick **Make backups of my macros**, set **Back up every** 1 **hour(s)** and
**Copies to keep** 2. Within a minute a `macros_backup_..._UTC.json` appears and "Last backup"
shows it. Click **Back up now** three times, a few seconds apart: "Backed up as ..." each time,
and the folder keeps only the 2 newest backups (the safety copy is still there). Try out-of-range
values: **every** 0 becomes 1, 100 hours becomes 48, then switch to **month(s)**: it shows 12.
**Keep** 0 becomes 1 and 60 becomes 50. Set it back to 1 hour, keep 2, and restart the game: the
settings are kept.

**R4. Only when changed** (optional, takes an hour). With hourly backups on, change nothing for
just over an hour: no new backup. Then rename a macro: a new backup appears within a minute.

**R5. Restore.** On the Restore tab, the list is newest first, with the date and time (UTC),
folder and macro counts that match the library at that time, and the type (Backup or Safety
copy); hovering a date shows the file name. Rename a macro and delete another, then click
**Restore...** on a backup from before: it asks, with the date, time and counts. Click
**Restore**: the library window opens with the old library, and "Restored the backup from ..."
with **Undo restore** above the tree. Click **Undo restore**: your changes are back. Restore
again and restart the game: the library is still the restored one.

**R6. Other files in the folder.** Put a `notes.txt` into `backups\`, and a text file named
`macros_backup_2020-01-01_00-00-00_UTC.json` containing just `{`. Click **Refresh**: `notes.txt`
isn't listed, and the 2020 row shows **Can't be read** (hover: why) with no Restore button. Make
more backups than you keep: `notes.txt` is never deleted. Delete both files afterwards.

**R7. A linked macro.** With a macro linked to a `.txt` file (session K), edit the file, then
restore a backup from before the edit: the macro shows the file's text, and the file isn't
changed.

## Report template

```
Build: (stamp from the log)   Nexus version:   Game language:
A1  A2 (both re-checks)  A3  A4 (yellow/red easy to see?):  A5  A6
B1  B2  B3  B4 (what arrived):
C1  C2 (did /w Name work without Tab?):
D1  D2  D3  D4  D5  D6
E1
F1  F2 (which mouse buttons, where):  F3  F4  F5  F6  F7  F8
G1  G2  G3 (where did it appear?):  G4  G5  G6
H1  H2  H3  H4 (map: mode, map id):  H5  H6  H7  H8  H9  H10
I1 (date format sent):  I2  I3  I4  I5  I6  I7  I8  I9
J1  J2  J3  J3b  J4  J5  J6  J7  J8  J9  J10  J11
K1  K2  K3  K4  K5  K6
L1  L2
M1 (entries and sizes):  M2  M3  M4  M5
N1 (pasted text):  N2  N3
O1  O2  O3 (long name too)  O4  O5
P1  P2  P3  P4 (did Ctrl+F / Escape reach the game?):  P5  P6 (label at character select):  P7 (did typing or Enter reach the game?):  P8
Q1  Q2  Q3 (anything cut off?):  Q4  Q5  Q6  Q7  Q8   Translations that read oddly:
R1  R2  R3  R4  R5  R6  R7
Nexus log warnings:
```
