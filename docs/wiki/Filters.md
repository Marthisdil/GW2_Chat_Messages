# Filters

Filters decide **where a macro is active**: in which game modes, and optionally on which maps.
Open them in the macro editor under **Where it's active**. Folders can have filters too: see
[Filters on folders](#filters-on-folders). The header shows a summary, like
"Everywhere", "PvE, PvP" or "PvE on 2 maps", and "(not here)" when the macro isn't active where
you are.

<!-- screenshot: filters.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the "Where it's active" section.*

## What filters change

- **Hotkeys** only send macros that are active here.
- The **quick menu** only lists macros that are active here, except your
  [favourites](Sending-Macros#favourites-and-recently-sent): they show on every map, faded.
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
- **Find a map by name or id:** type part of a map's name (case and accents don't matter, so
  "lions" finds "Lion's Arch") or its id. Up to 12 matches are listed as "Name - Region (id)",
  maps you've been to first; click one to add it. A map that's already in the list is greyed out.
  Several maps can share a name (story instances, festival copies): the region and id tell them
  apart.
- **Maps you've been to** lists the last 30 maps you visited, newest first. The addon remembers
  each map as you arrive on it.
- A map shown in **yellow with (!)** isn't in the GW2 API's map list any more: it may have been
  removed or replaced with a new id in a game update (then this filter never matches, so swap it
  for the new map), or it's an instance the API doesn't list.
- Map names, like "Queensdale (15)", come from the official GW2 API and are saved on your PC
  (see [Files and data](Files-and-Data)), in the addon's language where the API has it (German,
  French, Spanish; English otherwise). Without an internet connection, maps just show as
  "Map 15". Map ids are also listed on the [GW2 wiki](https://wiki.guildwars2.com/).

<!-- screenshot: filter-map-search.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the map search with a few matches, and the "Maps you've been to" list.*

### The map list

The map search uses the GW2 API's full list of maps. The addon fetches it in the background on
the first run, once a week, and when you change the language. The addon's page in **Nexus >
Options** shows how many maps it knows and from when. With [Advanced options](Advanced-Options)
ticked it also has a **Refresh map list** button to fetch it again now. If a refresh fails (no internet, the API is down), the old list is kept and
the page says why.

<!-- screenshot: options-map-list.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: the map list on the addon's options page.*

The editor says whether the macro is active where you are now ("Active here" / "Not active
here").

## Filters on folders

A folder can have game modes and maps too, and **everything inside it follows**: its macros and
its sub-folders.

- The filters **add up**. A macro is active only where its own filter **and** every folder above
  it allow. A "WvW only" folder means nothing in it is ever active in PvE, whatever the macros
  inside say. A macro with no filter of its own simply follows its folders.
- **Game modes:** when you create a folder or rename it, the name box has **Active in:** PvE,
  WvW, PvP. At least one stays ticked.
- **Maps, and everything else:** select the folder and open **Where it's active** in its
  details. It works like a macro's: game modes, maps, map search, and whether it's active here.
- **In the library:** a folder with a filter shows it after its name, like `Roaming  [WvW]`,
  `LA  [1 map]` or `Squad  [WvW, 2 maps]`. A folder that isn't active where you are is dimmed,
  and so is everything inside it.
- A macro's editor lists the folders that also limit it ("Also limited by the folders above
  it: Roaming (WvW)").
- **Filters that have nothing in common:** a PvE-only macro in a WvW-only folder, or a macro
  whose maps aren't on its folder's map list, can't be active anywhere. The editor warns you in
  yellow. Change one of the filters, or move the macro.
- Moving a macro into a folder with a filter makes it follow that folder.
- Exports carry a folder's filter, and an imported folder brings it along (see
  [Import and export](Import-and-Export)).

<!-- screenshot: folder-filters.png (docs/screenshots.md) -->
> 📷 *Screenshot to come: folders with filter tags in the library, one dimmed, with its "Where it's active".*

## Sharing a hotkey

Because hotkeys only use active macros, you can give the same key to macros whose filters never
overlap, for example "WvW only" and "PvE only" (a folder's filter counts too). The editor warns only when two macros with the
same key could be active in the same place.
