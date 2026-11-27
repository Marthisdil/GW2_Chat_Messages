# Characters and symbols in Guild Wars 2 chat

Research for the special character picker (`docs/todo.md`, section 3, "Special character
picker"), gathered 2026-11-26. ArenaNet has never published a list of what the chat font can
draw, so everything here comes from players' tests. **Treat it as a starting point: each group
the picker offers still needs an in-game check** (see "Still to check in game" below).

## The short version

- **Works (community-tested):** everything in Latin-1 (accented letters, `£ ¥ ¢ © ® ° ± × ÷ ¿ ¡`
  ...), general punctuation (`– — ‘ ’ “ ” • … ‰ ′ ″ ‹ › ‼`), letterlike symbols (`™ № ℃ Ω`),
  arrows, maths operators, technical symbols, circled and bracketed numbers and letters
  (`① ⑳ Ⓐ ⓩ`), box drawing, geometric shapes (`■ □ ▲ ▼ ◆ ● ○`), the first part of
  Miscellaneous Symbols up to ♪ (`☀ ☁ ★ ☆ ☠ ☮ ☯ ☺ ♀ ♂ ♔-♟ ♠ ♣ ♥ ♦ ♪`), a few currency signs
  (`€ ₣ ₤ ₹ ₽ ...`), and CJK ideographs U+4E00 to U+9FA5.
- **Doesn't work:** Miscellaneous Symbols from U+266B on (`♫ ♬ ♭ ♮ ♯ ⚔ ⚡ ⛔ ...`), Cyrillic,
  Japanese kana, and colour emoji (😀 🎉 👍 and the rest of U+1F000 and up).
- **Unknown, test before offering:** Dingbats (`✓ ✔ ✗ ✦ ✯ ❤`), Hangul (`웃 유`), Greek,
  Hebrew, Arabic, Thai, and the "lenny face" pieces (`͡° ͜ʖ`, `ಠ_ಠ`).
- **A character the font can't draw** shows as an empty space, a small rectangle or a tall
  rectangle. It's still sent: a player who copies the line out of chat gets the real character
  back. So a wrong pick doesn't fail the send, it just looks broken to everyone.
- **Font size matters:** the tests were done at the **smallest** chat font size. A character
  missing there may still show at a larger size (different font files), but the picker can only
  rely on what works at every size.
- **Other boxes:** most of the working characters also show in the LFG panel, the squad message
  and the guild message of the day.

## The tested groups in detail

From the GW2 Wiki user page *User:MithranArkanere/Charset* (tested in the smallest chat font).
Code points are Unicode; every character below U+10000 is one UTF-16 unit, so it counts as one
of the 199.

| Block | Range | Result | Notes |
|---|---|---|---|
| Basic Latin (ASCII) | U+0020-007E | Works | Plain keyboard text. |
| Latin-1 Supplement | U+00A0-00FF | All work | U+0080-009F (control codes) turn into other letters: never offer them. |
| General Punctuation | U+2000-206F | All work | U+2000-200F and U+2028-202F (spaces, joiners, direction marks) turn into letters: never offer them. |
| Currency Symbols | U+20A0-20CF | Listed ones work | `₣ ₤ ₧ € ₸ ₹ ₺ ₻ ₼ ₽ ₾`, plus `£ ¥ ¢ ¤ $` from Latin-1/ASCII. |
| Letterlike Symbols | U+2100-214F | Whole block listed | `℃ ℉ № ™ Ω ℮ ℹ ⅋` ... |
| Arrows | U+2190-21FF | Whole block listed | `← ↑ → ↓ ↔ ↕ ↖ ↗ ↘ ↙ ⇐ ⇒ ⇦ ⇧ ⇨ ⇩ ↺ ↻` ... |
| Mathematical Operators | U+2200-22FF | Whole block listed | `∞ √ ≈ ≠ ≤ ≥ ∑ ∆ ∴ ⊕ ⋆` ... |
| Miscellaneous Technical | U+2300-23FA | Whole range listed | `⌂ ⌘ ⌚ ⌛ ⏎ ⏩ ⏪ ⏰ ⏳ ⏸ ⏹ ⏺` ... (the emoji-looking ones show as plain text glyphs). |
| Enclosed Alphanumerics | U+2460-24FF | Whole block listed | `① - ⑳`, `⑴ - ⒇`, `⒈ - ⒛`, `⒜ - ⒵`, `Ⓐ - Ⓩ`, `ⓐ - ⓩ`, `⓪ ⓫ - ⓿`. |
| Box Drawing | U+2500-257F | Whole block listed | `─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ ═ ║ ╔ ╗ ╚ ╝ ╭ ╮ ╯ ╰ ╱ ╲ ╳`. |
| Geometric Shapes | U+25A0-25FF | Whole block listed | `■ □ ▪ ▫ ▬ ▲ △ ► ▼ ▽ ◄ ◆ ◇ ○ ◎ ● ◐ ◑ ◢ ◣ ◤ ◥ ◯`. |
| Miscellaneous Symbols | U+2600-266A | Works | `☀ ☁ ☂ ☃ ☄ ★ ☆ ☎ ☐ ☑ ☒ ☕ ☘ ☚ ☛ ☜ ☝ ☞ ☟ ☠ ☢ ☣ ☮ ☯ ☹ ☺ ☻ ☼ ☽ ☾ ♀ ♂ ♈-♓ ♔-♟ ♠ ♡ ♢ ♣ ♤ ♥ ♦ ♧ ♨ ♩ ♪`. |
| Miscellaneous Symbols | U+266B-26FF | **Doesn't work** | `♫ ♬ ♭ ♮ ♯ ♻ ⚐ ⚑ ⚔ ⚠ ⚡ ⛔ ⛪` ... |
| CJK Unified Ideographs | U+4E00-9FA5 | Included | From the Chinese client's font. Japanese kana (U+3040-30FF) aren't, so Japanese text mostly breaks. |

A guild forum post (doncpauli.com, 2015) also lists, as usable, a few characters from groups the
wiki page doesn't cover: `✓ ✯` (Dingbats), `웃 유` (Hangul), `٩ ۶` (Arabic-Indic), `†` (General
Punctuation, so covered) and `© ™ ®`. These are one player's list, more than ten years old.

## What doesn't show, and the reports behind it

- **Cyrillic:** Russian- and Ukrainian-speaking players report having to switch to an English
  keyboard layout, because the chat font has no Cyrillic (official forum threads "Add Cyrillic
  Symbols" on the old forum, and "Cyrillic ingame", 2021).
- **Japanese:** kana and most kanji don't show (official forum "Can we have 日本語 text in
  game?", "Japanese language input support", 2021). A reply there points out GW2 has no Japanese
  client, so it has no Japanese font.
- **Chinese and Korean:** an old support thread ("Asian/Chinese text in chat box shows up
  blank") reports Traditional Chinese and Hangul showing while Simplified Chinese and Japanese
  didn't. That matches the wiki's CJK range but is mixed evidence; test before offering any.
- **Colour emoji:** there's no sign the chat has ever drawn them, and the official forum thread
  asking for them ("emoji update to ingame chat") is a request, not a change. They're outside
  U+FFFF, so each would also cost **two** of the 199 units.
- **A past bug:** one official forum thread ("chat horribly bugged and not taking many
  letters") describes a time when chat dropped most non-ASCII letters; a developer was looking
  into it. A game update can change the font, so the picker's list may need re-checking after
  big patches.

## What this means for the picker

- **Offer only the tested groups** above (Latin-1 extras, punctuation, letterlike, arrows,
  maths, technical, enclosed numbers and letters, box drawing, geometric shapes, Misc Symbols up
  to ♪, currency), and only after each group passes in game. Leave out Cyrillic, kana, emoji and
  U+266B-26FF; leave the "unknown" groups out until a check says yes.
- **Never offer** control codes (U+0000-001F, U+007F-009F), the invisible spaces, joiners and
  direction marks (U+2000-200F, U+2028-202F, U+2060-206F), or variation selectors (U+FE00-FE0F).
  `ChatLine` should probably refuse them in typed text too; that's a separate decision.
- **Length:** every character in the tested groups is one UTF-16 unit, so `ChatLine`'s
  199-unit count already handles it. Emoji and other characters above U+FFFF would count two.
- **The addon's own font:** the picker draws in Nexus' ImGui font. That font must have these
  glyph ranges, or the picker shows "?" even where the game shows the character. Check which
  ranges Nexus loads (the fonts are Nexus', not ours), and whether the addon can add a font with
  the extra ranges without changing `imconfig.h` or keeping an `ImFont*` (both forbidden in
  `CLAUDE.md`). The game's own font can't be used (it's packed inside `Gw2.dat`); the plan, a
  built-in open-licence font loaded with just the picker's glyph ranges, is in the todo item.
- **Be careful with what gets offered:** some characters in these blocks are offensive (the
  forum list above includes a swastika); the picker's list is hand-picked, not a whole block
  dump.
- **Advanced option?** No (already decided in `docs/todo.md`): it only inserts text.

## Still to check in game

When the picker is built, add a session to `docs/in-game-tests.md` (the next free letter, in
priority tier 4, "looks, languages and rare cases"): for each group the picker offers, send a
line of its characters in say chat at the **smallest and the largest** chat font size and note
any that show as boxes or blanks; also check the unknown groups above (Dingbats, Hangul, Greek)
before adding them. Until then the in-game check A3 (`Accents`, with `日本`) is the only one
that touches non-ASCII text.

## Sources

- GW2 Wiki, *User:MithranArkanere/Charset*:
  https://wiki.guildwars2.com/wiki/User:MithranArkanere/Charset (read 2026-11-26).
- GW2 Wiki, *Chat* (the 199-character limit): https://wiki.guildwars2.com/wiki/Chat
- doncpauli.com, "Some Cool Symbols usable in gw2 chat" (2015-09-02):
  https://doncpauli.com/topic/some-cool-symbols-usable-in-gw2-chat/
- Official forum, "Cyrillic ingame" (2021): https://en-forum.guildwars2.com/topic/96049-cyryllic-ingame
- Official forum, "Japanese language input support" (2021):
  https://en-forum.guildwars2.com/topic/97553-japanese-language-input-support-%E6%97%A5%E6%9C%AC%E8%AA%9E
- Official forum, "Can we have 日本語 text in game?":
  https://en-forum.guildwars2.com/topic/16434-
- Official forum, "chat horribly bugged and not taking many letters":
  https://en-forum.guildwars2.com/topic/141851-
- Official forum, "emoji update to ingame chat":
  https://en-forum.guildwars2.com/topic/41947-emoji-update-to-ingame-chat/
- Old official forum archive, "Add Cyrillic Symbols":
  https://forum-en.gw2archive.eu/forum/game/gw2/Add-Cyrillic-Symbols
- Old official forum archive, "Asian/Chinese text in chat box shows up blank":
  https://forum-en.gw2archive.eu/forum/support/support/Asian-Chinese-text-in-chat-box-shows-up-blank
- The official forum and the archive block automated reading (bot checks), so their threads are
  summarised from search results, not read in full.
