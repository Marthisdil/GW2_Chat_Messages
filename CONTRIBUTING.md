# Contributing

Thanks for wanting to help with GW2 Chat Messages. There are three ways to contribute:
translations, bug reports and ideas, and code.

By contributing (a translation, text or code), you agree that your contribution is published
under this repository's license, the [GNU GPL v3](LICENSE), like the rest of the addon.

## Translations

The addon's own text (windows, buttons, tooltips, messages) can be shown in other languages.
Each language is one `.json` file in the [`localization/`](localization/) folder:

| File | What it is |
| --- | --- |
| `en.json` | Every English text the addon shows. It's written from the code by `tools\localization.ps1`: don't edit it. |
| `template.json` | An empty language file to start a new language from. |
| `de.json`, `fr.json`, `es.json` | German, French and Spanish. These are built into the addon. |

**The German, French and Spanish files were translated by Claude, an AI model, and haven't been
checked by a native speaker yet.** Corrections are very welcome, even for a single text.

### Fixing a text in an existing language

Open the language's file, find the line (the English text is on the left), change the text on
the right, and send it in (see [Sending it in](#sending-it-in)).

### Adding a language

1. Download [`localization/template.json`](localization/template.json) and save it as
   `<code>.json`, where `<code>` is the language's code: 2 or 3 lower-case letters, plus a
   region if needed (`nl.json`, `pt-br.json`, `it.json`).
2. Set `"Identifier"` to the same code, and `"DisplayName"` to the language's name in that
   language (`"Nederlands"`, `"Português Brasileiro"`). Nexus' own languages use these codes:
   `en`, `de`, `fr`, `es`, `cn`, `kr`, `tw`, `pt-br`, `cz`, `it`, `pl`, `ru`. If you use the
   same code as Nexus, "Follow Nexus" picks your language when Nexus is set to it.
3. Set `"AddonVersion"` to the addon version you translated (it's at the top of the addon's
   options page in Nexus, e.g. `2026.11.21.1412.Dev`). It tells people how current the file is.
4. Under `"Texts"`, fill in the right-hand side of each line. Don't change the left-hand side:
   it's the English text the addon looks up. A text you leave empty (`""`) is shown in English.

### The rules for each text

A text that breaks one of these rules isn't used (English is shown instead), and the addon says
so when it loads the file.

- **Keep every %-code exactly as it is, in the same order:** `%s` (a name or text), `%d` and
  `%zu` (numbers), `%.1f`, and so on. If your language needs the words in a different order,
  rephrase the sentence so the codes still come in the original order. `%%` is a plain `%` sign.
- **Keep the line breaks** (`\n`) where they are, as far as you can. A tooltip may have
  different line lengths, but the number of `\n` should stay the same.
- **Keep names as they are:** Nexus, GW2, Chat Messages, `macros.json`, file and folder names
  (`<GW2>\addons\GW2_Chat_Messages\...`), key names (Ctrl, Alt, Shift, Mouse 3, Ctrl+F),
  `{time}` and the other placeholders, and the Nexus menu names (`Nexus > Options >
  Keybinds`).
- Use the game's own words where your language has them (for example the game's names for
  chat channels and game modes).
- No tabs, no control characters, no invisible or right-to-left marks, and no `##`.
- Plain quotes `"` inside a text are written as `\"`, and a backslash as `\\`.

Some limits: a file can be at most 512 KB, a text at most 2048 bytes, and `DisplayName` at most
64 bytes on one line.

### Fonts

Nexus' fonts cover the Latin alphabets (with accents). Languages with other scripts (Chinese,
Korean, Russian...) may show `?` boxes until the addon has fonts for them. You're welcome to
translate them anyway; it's on the [to-do list](docs/todo.md).

### Trying it in game

1. Put your file in `<GW2>\addons\GW2_Chat_Messages\localization\` (create the folder if it
   isn't there; **Open localization folder** in the addon's options does it for you).
2. In Nexus > Options, open the addon's page and click **Reload language files**.
3. Pick your language in the **Language** dropdown. If a text was refused, an error window
   says why, and the Nexus log lists the first few texts.

A file in your localization folder with the same code as a built-in language (`de.json`, for
example) replaces the built-in one, so you can test a correction. English can't be replaced.

### Sending it in

- **Pull request (preferred):** add or change the file under `localization/` and open a pull
  request. If you can, run `tools\localization.ps1` (it checks that the files match the code)
  and `tools\run_tests.ps1`.
- **Issue:** if you don't use git, open an issue and attach the file (or paste the texts you
  changed).

Say which addon version you translated, and whether you're a native speaker.

### Safety

Language files are text only. The addon never runs anything from them and never downloads them
by itself, and it checks every file before using it (size, structure, allowed characters, and
that each text keeps its %-codes). Even so, **only use language files from this repository**
(the `localization/` folder here), not files passed around elsewhere.

## Bug reports and ideas

Open an issue. For a bug, the addon's error window has **Copy to clipboard**: paste that in, and
say what you did and what you expected. The Nexus log (`<GW2>\addons\Nexus\Nexus.log`) helps
too. The [Safety and troubleshooting](https://github.com/Marthisdil/GW2_Chat_Messages/wiki/Safety-and-Troubleshooting) wiki page lists common
problems.

## Code

- Read [CLAUDE.md](CLAUDE.md) (the project rules: input safety comes first) and
  [docs/design.md](docs/design.md) (decisions and data model) before starting on a feature.
  Open an issue first for anything big, so we can agree on the approach.
- Building: see [Building from source](README.md#building-from-source). Run
  `tools\run_tests.ps1` before sending a change.
- Work on a branch and open a pull request against `main`. Keep each pull request to one change,
  and update `CHANGELOG.md` and the user docs in `docs/wiki/` (the wiki's source) for anything a user would notice.
- New text in the UI goes through `T("...")` (or `TX("...")` for a text in a table, and `TR("...")` for a message built in a core module or on a worker thread); then run
  `tools\localization.ps1 -Write` to update `localization/en.json` and the other language files.
