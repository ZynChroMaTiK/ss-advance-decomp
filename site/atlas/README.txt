SERIOUS SAM ADVANCE - ALL 16 LEVEL TOP MAPS

Open index.html in a modern browser. It works offline and embeds all map data.
Use the level selector, layer checkboxes, wheel zoom and drag-to-pan. Click
items, doors, wall edges and rooms for their coordinates and script details.
Desktop level navigation is grouped into Egypt, Rome and Multiplayer. On
smaller screens it becomes a dropdown. The NETRICSA panel has Layers, Secrets
and Intel tabs; selecting a map object opens Intel automatically. Arrow keys
move between focused tabs. Map guide opens the legend and coverage notes.
Search finds pickups and doors in the selected level. Pink secret buttons
focus the reward, and the access buttons focus known switches / trigger areas.
The PNG / SVG / Map data links require the accompanying folders to stay beside
index.html. Extract the ZIP first.

WEB THEME
The interface uses the original title logo, green stone backdrop, fixed grid,
Egypt/Rome chapter artwork and bitmap font sheets. Green framed panels and
white selection states follow the game's menus and NETRICSA presentation.
The interactive map uses a matching green palette while keeping the sprite
colors and category colors. Printable PNG/SVG sheets retain their export palette.
Sheet legends are provided by the HTML interface; PNG/SVG maps omit the sidebar.
All visual assets and scripts are embedded in index.html for offline use.

CONTENTS
  index.html                 Offline interactive atlas, all 16 levels
  all-levels.png             Overview/contact sheet
  png/level-01.png ... 16    1760 x 1900 annotated maps, without legend sidebars
  svg/level-01.svg ... 16    Scalable annotated maps
  data/level-01.json ... 16  All map geometry, placements, scripts and metadata
  icons/                     Original sprite PNGs used on the maps
  sprite-manifest.json        Original resource/frame/palette for every icon
  theme/                     Original menu art and bitmap font sheets
  theme-manifest.json        Source paths and hashes for all web theme assets
  sources-and-secret-coverage.json  Guide URLs and exact secret evidence
  render-scenes.json        Drawing primitives used to generate the maps
  validation.json           Extraction and package validation results
  manifest-sha256.json      Content hashes (excluding this manifest)

COVERAGE
All 16 decoded map hashes match the existing native map validation. All BSP
leaves were processed. The atlas includes 7,632 line records, 942 sectors,
484 initial objects, 1,057 scripted placements from 116 scripts, 130 movers
(doors and lifts), and 8 switches. Initial and scripted enemies are optional
viewer layers. The four multiplayer maps include every D9 pickup respawn point.

Every room is revealed, including concealed rooms. Five documented secrets
are marked in levels 1-5. Levels 7, 8, 10 and 11 have no secrets according to
the cited level guides. Secret classification remains unverified for levels
6, 9, 12 and the multiplayer maps. No dedicated ROM secret flag has been
established. These pins do not prove exhaustive discovery of every easter egg.
The Pyramid Entrance reward is 25 armor in this ROM, although the guide calls
it Medium Armor. Its room, guardian, door and trigger match ROM script 8.

READING THE MAPS
Coordinates are original map units, +X right and +Y up. D labels number movers
from one; raw mover indices and sectors in JSON start at zero. B marks switches.
S1 is a secret; the S prefix on an item amount means shield. P1-P4 are starts.
Original ROM sprites replace item, enemy and player-start dots. An asterisk (*)
marks scripted-only appearances. R marks a rotating multiplayer spawn site.
Leader lines connect displaced labels to exact positions.
Repeated identical placements at one position share a glyph; all individual
records remain in the JSON and viewer details. Items are not all present at once.
H100 adds 100 health with a 200 cap. Red floors are lethal surfaces or pits.
Height differences and stairs remain visible as sector boundaries; floor and
ceiling heights are available by clicking a room. Doors show initial lock state.
Scripts can open, close, unlock or lock them later. The viewer exposes these
commands and the switches / first-entry sectors / pickups that request them.

ROM AND REPRODUCTION
Original ROM SHA-1: 4ff48d8c18b70a5184758f1917343ae85a70858e
The original ROM is unchanged and is not included in this archive.
Map table 0844AEF4; object translation 0844A3D4; script table 0845280C;
mover counts/table 0844DD3C/0844DD4C. Geometry uses BSP half-plane clipping,
following the native renderer, instead of guessing closed outlines from lines.
The level-11 original object-coordinate correction is applied to item locations.
Multiplayer R locations are possible rotating spawn sites, not fixed item types.
Sprites use each resource's first animation sequence frame (not simply image 1),
the highest available resolution, and the level's original world palette.
Player starts compose the original Sam base and shirt overlays using ROM origins;
campaign starts use white, multiplayer starts use the four original variants.
Sprites are map icons with normalized display size, not world-scale footprints.
SVGs and the viewer embed PNGs, so their sprite artwork works offline.

Workspace generators:
  scripts/export_topmaps.py (uses export_presentation_assets.decompress)
  scripts/topmap_viewer.html, topmap_viewer.css, topmap_viewer.js
  scripts/topmap_secrets.json
  scripts/render_topmaps.ps1
  scripts/package_topmaps.py
Run the Python generator, then the PowerShell renderer, then the package script.
The static maps were visually inspected. Interactive browser QA was unavailable
in the generation session; see validation.json for the checks actually run.

LEVEL INDEX
01  Temple Of Herkat Upper
02  Temple Of Herkat Lower
03  Amon Thule
04  Slave Compound
05  Pyramid Entrance
06  Tomb Of Ramses
07  The Forum Of Mars
08  Slave Quarters
09  Gladiator School
10  Baths Of Diocletian
11  Praetorian Fort
12  Caesar'S Palace
13  Egypt Multiplayer 1
14  Egypt Multiplayer 2
15  Rome Multiplayer 1
16  Rome Multiplayer 2
