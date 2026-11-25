# Quick Access icon suggestions

These are the candidate icons for the Nexus Quick Access bar: 00-09 collected on 2026-11-20,
10-17 on 2026-11-25. The **scroll (01)** was used first; the **quick bubble (10)** replaced it on
2026-11-25. The others are kept here in case the choice changes.

The concept SVGs are 64×64 and use the Raidcore-style palette: parchment `#D6D4AA`, darker
parchment `#B8B68E`, ink `#1E1E1A`. They are sketches. The real icon is drawn by
`tools/draw_chat_icon.ps1`, which writes `resources/icons/icon.png` and `icon_hover.png`. To
switch to a different concept, port its shapes into that script.

| # | File | Concept | Notes |
|---|---|---|---|
| 00 | `00-speech-bubble-original.png` | Speech bubble with three lines | The first icon. Clear, but the most generic. |
| 01 | `01-scroll.svg` | Scroll (used until 2026-11-25) | Prepared words to read out. Fits Tyria's look, but says "written text" more than "chat", and can read as a quest or notes addon. |
| 02 | `02-quill.svg` | Quill pen | "Written ahead of time". At 32 px it may read as notes or a journal. |
| 03 | `03-folder-bubble.svg` | Folder with a speech bubble | The only concept that shows both folders and chat. |
| 04 | `04-stacked-lines.svg` | Three offset message bars | Shows one macro sending several lines. |
| 05 | `05-megaphone.svg` | Megaphone | Announcing to many people. Strong for commanders, but reads as broadcast only. |
| 06 | `06-war-horn.svg` | War horn | The GW2 version of a megaphone. May be confused with a WvW or commander addon. |
| 07 | `07-sealed-letter.svg` | Letter with a red wax seal | "A message ready to send". Stands out, but suggests mail more than chat. |
| 08 | `08-paper-plane.svg` | Paper plane | The universal "send" symbol. Clear but generic. |
| 09 | `09-slash-command.svg` | Chat box with `/_` | Like typing `/s` or `/p`. Players recognize it, and it's distinctive. |
| 10 | `10-quick-bubble.svg` | **Speech bubble with a lightning bolt** (chosen 2026-11-25) | "Chat, fast": a message sent in one press. The clearest at 32 px. |
| 11 | `11-hotkey-bubble.svg` | Keyboard key with a bubble on it | The most specific (press a key, chat goes out), but busier at 32 px. |
| 12 | `12-stacked-bubbles.svg` | Two bubbles, one behind the other | A collection of ready messages. |
| 13 | `13-send-bubble.svg` | Bubble with a play arrow | "Start a message", but could be taken for a music or video addon. |
| 14 | `14-bubble-folder.svg` | Bubble with a folder tab | The library of folders; a tidier take on 03. |
| 15 | `15-bubble-list.svg` | Bubble with bullet points | A list of preset lines. |
| 16 | `16-herald-banner.svg` | Pennant with writing | An announcement, Tyria style; says "chat" less clearly. |
| 17 | `17-phrasebook.svg` | Book with a bubble on the cover | A phrasebook of lines; says "chat" less clearly. |

## Research notes

- I found no shared list of the icons other Nexus addons use. The addons I looked at use simple
  symbols in the Raidcore style. For example, Clears Tracker uses a raid icon.
- The closest Blish HUD module, LT Messages (preset commander messages), uses the text "[LT]" as
  its icon.
- bhm-chat-macros uses a Blish HUD corner icon.
- 2026-11-25: still no Nexus style guide for Quick Access icons. The game's own icons (a chat
  bubble, emotes) can't be used: the GW2 wiki's permission to show ArenaNet art doesn't extend to
  third parties, so every icon here is our own drawing.
