# Safety and troubleshooting

## How sending stays safe

Some chat macro tools can leave game keys stuck, for example Tab no longer targeting. This addon
is built so that can't happen:

- Keys and text are sent **only to the game window**, never into Windows' keyboard input. Your
  real keyboard state is never changed.
- Chat is opened with **your own Chat key** from Nexus. The addon never guesses a key.
- Your Shift, Ctrl, Alt and Tab are **never pressed or released for you**. If you're holding a
  modifier when a macro starts, it waits briefly and otherwise doesn't send.
- Text is typed character by character. Your clipboard is never used.
- Every step is checked (chat opened, line sent, chat closed). If something goes wrong, it stops,
  closes chat with one Escape **only if it opened chat itself**, and sends nothing more.
- Hotkeys only listen: your key presses always reach the game as normal.

## The error window

When something goes wrong, a small **Chat Messages: error** window opens with the error, what
you did to get it, and two buttons:

- **OK** closes it. If more errors are waiting it says "Error 1 of 3", and OK shows the next.
- **Copy to clipboard** copies the error for a bug report: the addon version and build stamp,
  the time, what you did, the error, and where you were in game.

The window stays in front of the library until you click **OK**, even if you click in the
library meanwhile. Errors from a hotkey or the quick menu also show a Nexus alert, and their
window opens without taking focus, so you can keep playing and read it later.

<!-- screenshot: error-window.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the error window.*

## Messages you may see

| Message | What to do |
|---|---|
| Nexus doesn't know your "Chat" key | Set the Chat key in Nexus > Options > Game Keybinds to match the game. |
| Nexus doesn't know your "Squad Broadcast" chat key | The same, for Squad Broadcast. Only needed for broadcast lines. |
| Chat didn't open | Check that the Chat key in Nexus matches your game setting. |
| A text box is open in game | You were typing in chat or another text box. Close it and try again. |
| A Shift, Ctrl, Alt or Windows key is held down | Release it and try again. |
| Already sending a macro | Wait for it to finish. |
| Chat closed while typing | You clicked away or pressed Escape. Send it again. |
| Not in game (loading screen or character select) | Wait until you are in the game world. |
| The game window isn't focused | Click into the game first. |
| Line 2: {random}: ... | A placeholder failed, so nothing was sent. The message says why. |
| Line 2: {txt} isn't available any more ... | The line still uses `{txt}` or `{json}`, which were removed. Edit the line to remove it. See [Placeholders](Placeholders). |
| Line 2: a placeholder would start a chat command ... | A placeholder's value would have put a `/` where chat reads a command, so nothing was sent. See [Placeholders](Placeholders). |
| Line 2: it has a control character ... | The line's own text has a tab, line break or another control character (for example from an imported file). Edit the line and send it again. |
| ... the GW2 API couldn't be reached | `{map}` needs the internet for a map it hasn't seen before. |
| ... is set on 2 macros | Two active macros share this hotkey. Change one, or set filters that don't overlap. |
| Your macros couldn't be saved | See the Nexus log. The addon keeps retrying. |

<!-- screenshot: send-alert.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: a Nexus alert from a hotkey that couldn't send.*

## A hotkey does nothing

- Is the macro active here? Check **Where it's active** (see [Filters](Filters)).
- Are you typing in chat, or in a text box in another addon? Hotkeys are ignored then.
- Is another active macro using the same key? You'll see an alert.
- Holding the key down doesn't repeat it: press it again.

## A whisper arrives without its message

The addon can't see which chat box the game is typing into, only that a chat box is open. If the
game is slow to move from the name box to the message box, the message can be lost, and the
whisper goes out empty (or not at all) **without an error**. The addon waits for the game after
`/w` and after Tab to prevent this. If it still happens, for example at a very low frame rate,
please report it with your frame rate and where you were (see below).

## Getting help

Open an issue on the project's GitHub page, with what you did, what happened, and the lines from
the Nexus log that mention Chat Messages. If the error window was shown, click **Copy to
clipboard** and paste that in too.
