# Quick Access icon suggestions

These are the candidate icons for the Nexus Quick Access bar, collected on 2026-11-20. The
**scroll (01)** was chosen. The others are kept here in case the choice changes.

The concept SVGs are 64×64 and use the Raidcore-style palette: parchment `#D6D4AA`, darker
parchment `#B8B68E`, ink `#1E1E1A`. They are sketches. The real icon is drawn by
`tools/draw_chat_icon.ps1`, which writes `resources/icons/icon.png` and `icon_hover.png`. To
switch to a different concept, port its shapes into that script.

| # | File | Concept | Notes |
|---|---|---|---|
| 00 | `00-speech-bubble-original.png` | Speech bubble with three lines | The first icon. Clear, but the most generic. |
| 01 | `01-scroll.svg` | **Scroll** (chosen) | Prepared words to read out. Fits Tyria's look and stays readable at small sizes. |
| 02 | `02-quill.svg` | Quill pen | "Written ahead of time". At 32 px it may read as notes or a journal. |
| 03 | `03-folder-bubble.svg` | Folder with a speech bubble | The only concept that shows both folders and chat. |
| 04 | `04-stacked-lines.svg` | Three offset message bars | Shows one macro sending several lines. |
| 05 | `05-megaphone.svg` | Megaphone | Announcing to many people. Strong for commanders, but reads as broadcast only. |
| 06 | `06-war-horn.svg` | War horn | The GW2 version of a megaphone. May be confused with a WvW or commander addon. |
| 07 | `07-sealed-letter.svg` | Letter with a red wax seal | "A message ready to send". Stands out, but suggests mail more than chat. |
| 08 | `08-paper-plane.svg` | Paper plane | The universal "send" symbol. Clear but generic. |
| 09 | `09-slash-command.svg` | Chat box with `/_` | Like typing `/s` or `/p`. Players recognize it, and it's distinctive. |

## Research notes

- I found no shared list of the icons other Nexus addons use. The addons I looked at use simple
  symbols in the Raidcore style. For example, Clears Tracker uses a raid icon.
- The closest Blish HUD module, LT Messages (preset commander messages), uses the text "[LT]" as
  its icon.
- bhm-chat-macros uses a Blish HUD corner icon.
