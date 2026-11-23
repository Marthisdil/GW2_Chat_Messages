# Languages

The addon's own text (windows, buttons, tooltips and messages) can be shown in **English (US),
Deutsch, Français and Español**. That includes the error messages (from sending, imports, link
files and the library) and the key names shown for hotkeys ("Maus 4", "Bild auf"; Ctrl, Alt and
Shift keep their names). Your macros are never translated: they're sent exactly as you
wrote them.

> The German, French and Spanish texts were made by AI and haven't been checked by a native
> speaker yet. If a text reads oddly, please tell me (see
> [Contributing](https://github.com/Marthisdil/GW2_Chat_Messages/blob/main/CONTRIBUTING.md)).

## Choosing the language

Open **Nexus > Options**, go to the addon's page, and use the **Language** dropdown (under
**Text size**):

- **Follow Nexus (...)** (the default) uses the language set in Nexus' own options, and switches
  when you change it there. If the addon has no translation for that language, it's shown in
  English. (Nexus follows the game's language only if you use the Unofficial Extras addon;
  otherwise set it in Nexus.)
- Or pick a language, to use it whatever Nexus is set to.

Each language shows how much of it is translated, e.g. **Deutsch (100%)**. Texts that aren't
translated are shown in English. The choice is remembered.

The names of the addon's keybinds in **Nexus > Options > Keybinds**, and the scroll icon's
tooltip, follow Nexus' language.

<!-- screenshot: language-options.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the Language dropdown on the addon's options page.*

<!-- screenshot: library-german.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the library in German.*

## More languages

Other languages can be added with a language file (a `.json` file).

1. Get the file from the `localization` folder of the addon's
   [GitHub page](https://github.com/Marthisdil/GW2_Chat_Messages/tree/main/localization).
2. On the addon's options page, click **Open localization folder**. It opens
   `<GW2>\addons\GW2_Chat_Messages\localization\` (and creates it if needed).
3. Put the file there and click **Reload language files**. The language appears in the
   dropdown, marked "file from your localization folder". Hover it to see which addon version it
   was made for.

A file with the same language code as a built-in one (`de.json`, for example) replaces the
built-in language, which is useful for testing a correction. English can't be replaced.

To translate the addon into your own language, start from `template.json` in the same folder;
[CONTRIBUTING.md](https://github.com/Marthisdil/GW2_Chat_Messages/blob/main/CONTRIBUTING.md)
explains how.

### Safety

Language files only contain text. The addon never downloads them and never runs anything in
them. Every file is checked before it's used: its size, its structure, the characters in each
text, and that each text keeps the `%s`-style codes of the English text. A file that fails is
not loaded, and a text that fails is shown in English; the error window says which file and why.
Still, **only use language files from the addon's GitHub page**.

### Fonts

Nexus' fonts have the Latin letters with accents. Languages with other scripts (Chinese, Korean,
Russian...) would show `?` boxes for now.
