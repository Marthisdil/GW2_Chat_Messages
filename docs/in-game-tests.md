# In-game tests

The unit tests cover the rules (tree, raw text, send plan, hotkeys, filters, placeholders,
import, link sync). These checks cover what only the game can tell us: whether GW2 takes our keys
and text, and whether normal game input is ever disturbed.

The checks are grouped into sessions (A to L) in the order to run them: each one builds on the
ones before. A whole pass takes about an hour. Session B is the one that matters most: if it
fails, most of the later sessions can't be run, so report it first.

## How to report

For every check, write **pass**, **fail** or **skipped**, and for a fail (or anything odd):

- what you did and what happened, in your own words;
- the **exact status text** shown under the macro editor, or in the alert;
- the Nexus log lines mentioning `Chat Messages` (Nexus > Log; filter by "Chat Messages");
- for anything visual, a screenshot (save it anywhere, not in `docs\images\`, which is for the
  README and wiki pictures).

Checks marked **(question)** have no right answer yet: they decide how a feature will work, so
describe what you saw even if nothing went wrong. A copy-paste template is at the end.

## Before you start

1. Build Release and copy `build\Release\GW2_Chat_Messages.dll` into `<GW2>\addons\`. If you had
   an earlier build in game, back up `<GW2>\addons\Chat Messages\` first.
2. In **Nexus > Options > Game Keybinds**, check that **Chat** matches your in-game chat key, and
   (for session E) **Squad Broadcast**. The addon uses these; it never guesses a key.
3. In **Nexus > Options > Keybinds**, set keys for **Show/hide Chat Messages library** and **Open
   Chat Messages quick menu** (any free keys, e.g. Ctrl+Shift+M and Ctrl+Shift+Q).
4. Stand somewhere quiet in the open world (PvE), near an enemy you can target with Tab, but not
   in combat.

## A. Loading and the library

**A1. Loading.** Start the game. In the Nexus log: `Loaded v0.1.0.` and no warnings.
The scroll icon is in the Nexus bar. Clicking it opens and closes the library; so does your
library key. Nexus > Options > Addons > Chat Messages shows the version and an **Open macro
library** button that works.

**A2. Building the tree.** In the library:
- **New folder** (toolbar) → name `Test`. In it, **New folder inside** five times, one inside the
  next, so the deepest is 6 folders deep. A 7th folder inside the deepest one is refused with a
  "levels deep" message. A macro inside the deepest folder is allowed.
- A second folder or macro called `test` (any case) in the same folder is refused ("There is
  already a ...").
- Rename (right-click > Rename), Move up / Move down, **Sort contents A-Z**, Expand all / Collapse
  all: each does what it says.
- Drag a macro into another folder, and drag a folder into a folder: both move. Dragging a folder
  into its own sub-folder, or so that it would end up more than 6 deep, is refused.
- Delete a folder with macros in it: it asks first, then removes the folder and its contents.

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
- The character count on `Long` shows 199/199 (196 plus `/s `). Typing one more character shows
  it over the limit (red). Remove that character again.
- **Add line** stops at 50 lines (try on a scratch macro, then delete it).

**A5. Saving.** Close the game normally (or unload the addon in Nexus > Addons), start it again:
the whole tree, every line, expanded folders and selection are as you left them.
`<GW2>\addons\Chat Messages\` has `macros.json` and `macros.json.bak`.

**A6. A broken file.** Close the game. Open `macros.json` in Notepad, delete the first `{` and
save. Start the game: the Nexus log says the file couldn't be read and the macros were restored
from the backup; the library shows your macros (maybe missing the very last change); a
`macros.json.unreadable-<date>` file is in the folder. Nothing is lost.

## B. Sending: does the game take our keys? (most important)

Use **Send now** in the editor for these. If B1 fails, stop and report it: the rest depends on it.

**B1. `Say`.** Click **Send now** with the mouse over the addon window. `test 1` appears in Say
chat and chat closes again; the status says "Sent 1 line." If it fails, say which part:
- chat didn't open at all (status: "chat didn't open"), or
- chat opened but stayed empty (the game ignores our typed text: we'd need the clipboard way), or
- the text was typed but not sent (status: "the game didn't take the message").

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

Nothing should happen in game for D1-D4: no chat opens, nothing is typed.

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
(or click into the game). The send stops with a message. The game menu must **not** open (we send
at most one Escape). Report what you see in chat.

**D6. Not in game.** Log out to character select, open the library from the Nexus bar and send
`Say`: refused, "Not in game". (If the bar isn't there on character select, mark skipped.)

## E. Commander (skip if you can't tag up)

**E1.** Add a Squad line with **Broadcast** ticked. As commander it goes out as a squad broadcast;
when you're not commander, the same macro is sent as normal squad chat.

## F. Hotkeys

**F1. Setting one.** On `Say`, click **Set...** and press Ctrl+F1: the editor and the tree show
`Ctrl+F1`. **Set...** then Escape: cancelled, `Ctrl+F1` stays. **Set...** then plain Tab, plain
Enter or Ctrl+Escape: refused with a message. Closing the window while it waits for a key also
cancels it.

**F2. Mouse buttons (question).** On `Emote`, **Set...** with the mouse over the addon window and
press Mouse 4. Is it recorded? Try Mouse 5 and Mouse 3 (wheel click) too. Then try with the mouse
over the game instead of the window. Report which work where.

**F3. Using it.** Close the library and press Ctrl+F1, releasing Ctrl quickly: `Say` is sent. Hold
Ctrl+F1 down for a few seconds: sent once only. Press it twice quickly: sent once. Press the mouse
hotkey from F2 while moving the camera: `Emote` is sent and the camera isn't affected afterwards
(no stuck button).

**F4. Not while typing.** Give `Say` the plain key `F2` (or any key you never use) for this check.
Open chat and press F2: nothing is sent. Click into another addon's text box (or our own message
box) and press F2: nothing is sent. Set `Say` back to Ctrl+F1.

**F5. Clashes.** Give `Two` the same hotkey as `Say`: both show "Also set on ...". Pressing it
shows an alert and sends nothing. Clear `Two`'s hotkey. Then clear the Chat game keybind and
press Ctrl+F1: an alert explains why nothing was sent. Set the key back.

**F6. Saved.** Reload the addon (Nexus > Addons, or restart): the hotkeys are still set and work.

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
mouse hotkey does nothing; **Send now** still sends it. Unticking the last game mode is refused.

**H2. Maps.** **Add this map** adds the map you're on, shown by name (and `maps.json` appears in
the addon folder). The macro is then active only on this map: check on another map, then remove it
with its **x**. While the current map is in the list, **Add this map** is greyed out, and
**Add id** with the same number says it's already in the list.

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
`<GW2>\addons\Chat Messages\`, saved as UTF-8. `{txt names.txt}` sends a random line;
`{txt names.txt 2}` the second one; accents arrive right. `{txt missing.txt}`: nothing is sent and
the status says it wasn't found.

**I3. Left alone.** `/s {smile} meet at [&BDAEAAA=]`: `{smile}` arrives as typed, and the code
arrives as a clickable waypoint link.

**I4. Too long once filled in.** Make line 1 of `names.txt` 190 characters long, and send
`/s {txt names.txt 1} and more`: refused with a "too long" message, nothing sent.

**I5. Map name and {json}.** `/s Hello from {map}`: sends the map name, also right after you
arrive on a map you haven't visited this session. `{json id https://api.guildwars2.com/v2/build}`:
sends the game build number. `{json nope https://api.guildwars2.com/v2/build}`: nothing is sent;
the status says the path wasn't found.

**I6. Offline.** Close the game, delete `maps.json`, disconnect from the internet, start the game
(Nexus may complain about updates; that's fine) and enter a map. `{map}` fails with a "couldn't be
reached" message and nothing is sent. The game doesn't stutter while it tries. Reconnect.

**I7. Typing while it prepares.** Send the `{json ...build}` macro and, while the status says
"Preparing...", open chat and start typing. The send stops ("A text box is open") and **your chat
box stays open with your text**.

## J. Import and export

**J1. Export.** Right-click `Say` > Export > **Save as a .json file**, twice: `exports\Say.json`
and `exports\Say (2).json` appear and the status names the file. **Save as a .txt file** and
**Copy the lines as text** give the raw lines, one per row (paste into Notepad to check).

**J2. Import with conflicts.** Right-click the `Test` folder > Export > **Copy to the clipboard**.
Then toolbar **Import...** > **Paste from clipboard**, with `Test`'s parent as the target. The
preview shows `Test` as "goes into the existing folder" and the macros as conflicts. Set one to
Replace and one to Skip, untick **Keep hotkeys**, and click **Import**. The result matches the
preview, and the imported macros have no hotkeys.

**J3. Undo.** **Undo import**: the library is exactly as before (names, lines, hotkeys, filters).

**J4. Files and folders.** Copy `Say.json` into `imports\` and open **Import...**: it's listed and
loads when clicked. Make a folder in `imports\` with `.txt` files in two levels of sub-folders,
and import it into the deepest test folder (level 6): the preview leaves the too-deep folders
out, or with "Move the contents of too-deep folders up a level" ticked, their macros land one
level up. In the path box, paste a path with quotes (Explorer's "Copy as path") and press Enter:
it loads.

**J5. Focus.** **Open imports folder**: Explorer opens. Back in the game: keys and Tab work.
With the Import window open, hotkeys still work, and typing in its path box doesn't trigger them.

## K. Text file links

Have Notepad ready. "Within a second" means after you save, not after you switch back.

**K1. New file.** On `Two`, open **Text file link**, type `test.txt`, click **Link**:
`<GW2>\addons\Chat Messages\links\test.txt` appears with `Two`'s lines, and the status says the
file was created.

**K2. Both ways.** **Open file**, add `/s from notepad` and save: within a second `Two` shows it
(Lines and Raw text). Edit `Two` in the addon: within a second the file changes (Notepad asks to
reload when you switch to it). Keep Notepad open on the file while you type in the addon's
message box: no stutter, nothing lost, hotkeys still work.

**K3. Existing file and duplicates.** Make `links\other.txt` with one line, link `Say` to it:
`Say` now has that line. Link `Emote` to `TEST.txt`: refused, "already linked".

**K4. Gone and back.** Delete `test.txt`: the editor says the file isn't there. Edit `Two`: the
file is **not** recreated. Click **Recreate the file**: it's back with `Two`'s lines.

**K5. Edited while closed.** Close the game, edit `test.txt`, start the game: `Two` has the new
lines. **Unlink**: the macro and the file both stay; editing either no longer changes the other.

## L. Final checks

**L1. Game input after everything.** Movement keys, skills, Tab targeting, Shift/Ctrl/Alt combos,
the keys you used as hotkeys (doing their normal game action), left/right click, camera turning,
and the chat key all work normally. No key feels stuck.

**L2. Unloading.** Send `Long` and, while it's sending, unload the addon in Nexus > Addons: the
game doesn't freeze and the unload finishes within about a second. Do the same right after sending
the `{json ...build}` macro: done within a few seconds. Load the addon again: everything's there.

## Report template

```
Build: (date or commit)   Nexus version:   Game language:
A1  A2  A3  A4  A5  A6
B1  B2  B3  B4 (what arrived):
C1  C2 (did /w Name work without Tab?):
D1  D2  D3  D4  D5  D6
E1
F1  F2 (which mouse buttons, where):  F3  F4  F5  F6
G1  G2  G3 (where did it appear?):
H1  H2  H3  H4 (map: mode, map id):
I1 (date format sent):  I2  I3  I4  I5  I6  I7
J1  J2  J3  J4  J5
K1  K2  K3  K4  K5
L1  L2
Nexus log warnings:
```
