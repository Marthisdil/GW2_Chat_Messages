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
| Line 2: {txt}: ... | A placeholder failed, so nothing was sent. The message says why. |
| ... the GW2 API couldn't be reached | `{map}` needs the internet for a map it hasn't seen before. |
| ... is set on 2 macros | Two active macros share this hotkey. Change one, or set filters that don't overlap. |
| Your macros couldn't be saved | See the Nexus log. The addon keeps retrying. |

## A hotkey does nothing

- Is the macro active here? Check **Where it's active** (see [Filters](Filters)).
- Are you typing in chat, or in a text box in another addon? Hotkeys are ignored then.
- Is another active macro using the same key? You'll see an alert.
- Holding the key down doesn't repeat it: press it again.

## Getting help

Open an issue on the project's GitHub page, with what you did, what happened, and the lines from
the Nexus log that mention Chat Messages.
