# Filters

Filters decide **where a macro is active**: in which game modes, and optionally on which maps.
Open them in the macro editor under **Where it's active**. The header shows a summary, like
"Everywhere", "PvE, PvP" or "PvE on 2 maps", and "(not here)" when the macro isn't active where
you are.

## What filters change

- **Hotkeys** only send macros that are active here.
- The **quick menu** only lists macros that are active here.
- In the **library**, inactive macros are dimmed. **Send now** always works.

## Game modes

Tick one or more of:

| Mode | Includes |
|---|---|
| PvE | Open world, story, fractals, raids and strikes. |
| WvW | The WvW maps, Armistice Bastion and Edge of the Mists. |
| PvP | PvP maps, including the Heart of the Mists lobby. |

A macro needs at least one game mode. Loading screens and character select count as no mode,
so only macros active everywhere would match there (and hotkeys are off outside gameplay
anyway).

## Maps

- **Any map** (the default): the game modes decide.
- Otherwise, the macro is only active on the maps in its list (and only in the ticked modes).
- **Add this map** adds the map you're on. **Add id** adds a map by its id. Click the "x" on a
  map to remove it. A macro can list up to 50 maps.
- Map names, like "Queensdale (15)", come from the official GW2 API and are saved on your PC
  (see [Files and data](Files-and-Data)). Without an internet connection, maps just show as
  "Map 15". Map ids are also listed on the [GW2 wiki](https://wiki.guildwars2.com/).

The editor says whether the macro is active where you are now ("Active here" / "Not active
here").

## Sharing a hotkey

Because hotkeys only use active macros, you can give the same key to macros whose filters never
overlap, for example "WvW only" and "PvE only". The editor warns only when two macros with the
same key could be active in the same place.
