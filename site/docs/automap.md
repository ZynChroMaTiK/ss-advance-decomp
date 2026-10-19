# Native in-level automap

M toggles the original flag8 automap in the Windows frontend. The mouse wheel
zooms while it is open; held plus/minus keys, including the numeric keypad,
use the recovered one-sixteenth-per-tick zoom steps. Outside the map the wheel
still cycles weapons. Each entrance starts at8192, as original `08008B28` does;
zoom clamps to4096..65536. Tab/N retain NETRICSA and Esc/P retain Pause.

Options -> Extras -> Automap enables/disables the feature. It defaults to Off,
including when older version1..6 settings files are loaded. Version7 stores the
choice in byte10 bit7, with music volume in the lower seven bits; explicit saved
On/Off choices are preserved. The existing
16-byte CRC-protected file and all other settings retain their layout. Disabling
an open map closes it and clears held zoom/partial wheel input. Gameplay M no
longer toggles audio mute, and F no longer opens the map. This is a north-up,
player-following overhead map with live walls and zoom, in the familiar classic
FPS form; its drawing remains the recovered Serious Sam Advance map rather
than importing Doom assets or code.

The map follows the player with north up. WASD and mouse look remain available,
and the existing native world simulation continues. New firing and weapon
selection are suppressed while the map is open. A running automatic weapon
stops, and death returns to the normal view/death flow. Pause and NETRICSA can
open over the map and return to it. Focus loss clears held zoom. These are PC
control bindings around the recovered rendering/state; no GBA instruction runs
in the game process.

Both renderer settings display the same original240x160 map canvas through the
existing viewport scaler. M switches pixel ownership, dimensions and palette
before queued paint messages can present it, avoiding a stale framebuffer size
or a flash of the map palette over the world. Zoom and scanline motion use the
simulation clock; repeat draws do not advance either clock.

## Recovery

`src/automap.c` ports original `08007FDC` line clipping/rasterization and the
normal geometry in `08008420`. The original line routine ORs palette indices
into the framebuffer. Horizontal/vertical lines include both endpoints;
diagonals omit their final endpoint. The original isolated-point case writes
the neighboring byte. All these quirks are retained and compared to the ROM.
The original invalid post-clipping infinite loop becomes a native error.

The scene skips equal-floor two-sided lines, draws the remaining walls, colors
the ordered nearby contact lines, submits the scaled body-facing sight segment,
camera-heading triangle and active-entity crosses. Fixed-point multiplication,
coordinate wrap, live sector heights, coarse body facing versus full camera
heading, colors and draw order are retained. It also draws the original moving
five-row scan, palette indices14/12/10/8/6 at two-row intervals, with byte wrap.
The palette comes directly from original `0846265C`.

The normal-gameplay projection has no debug ray list. The original optional
debug branch destructively translates its global ray coordinates; that debug
list mutation is not implemented or claimed by this API. Original multiplayer
DMA/BIOS transport choices are not a network implementation.

`ssa_player_interaction_query_at` exposes the existing recovered0429C traversal
for an explicit destination. The old facing-based query delegates to it and
retains its original64-unit behavior. Automap uses its own initialized collision
scratch and the original zoom-scaled destination. It does not replace weapon,
movement or camera scratch, change `world.blocking_entity`, activate switches,
move entities or modify gameplay during drawing.

Native game ownership adds only the map's zoom, scanline counter and dedicated
query scratch. Initialization/free cover partial failures. The original flag8
drives existing HUD visibility and palette policy; the M adapter additionally
inhibits PC firing after the alternate-weapon policy refresh. Mode changes keep
the palette's simulation-owned flash clock unchanged.

## Verification

- `decomp.ps1 test-automap-rom`:3,000 original line cases/115,200,000 pixels,
  384 original scene cases/166,680 ordered line submissions on all16maps, and
  16 complete original map frames/614,400 pixels, including scanline DMA fills.
  Geometry cases control the contact-query result and service fixed multiplication
  arithmetically; trigonometry and line raster instructions execute from the ROM.
  Complete-frame cases have an empty debug list. UBSan enabled for native code.
- `decomp.ps1 test-player-hazards`:28,672 original comparisons, including the
  previous24,576 cases and4,096 additional zoom-scaled0429C contact sweeps.
- `decomp.ps1 test-automap`:1,813 ASan/UBSan/leak checks,638 native frames and672
  draws on all16maps. It covers zoom endpoints, repeated-draw equality, player/
  sprite-cache purity, preserved blocker state, movement, death and a controlled
  already-firing minigun fixture proving no additional shots while mapped.
  Turning the map off remains valid after completion, during pause and after
  death; opening it in those states remains blocked.
- `scripts/test-automap.ps1 -SkipBuild`: MSVC/GCC agreement plus actual hidden
  Windows M, wheel and held-key routes in both renderers/Egypt and Rome. Checks
  include repeat-key suppression, immediate paint dimensions, ordinary weapon
  selection suppression, Pause/NETRICSA return, focus cleanup and death. Extras
  gating, removed F binding and unchanged mute state are covered, with settings
  codec/menu/Windows persistence tests covering the saved choice in all languages.

Reports: `automap-rom-validation.json`, `automap-validation.json`,
`windows-automap-validation.json`, `native-player-hazards-validation.json`.
Full campaign playthroughs remain manual. This is native behavioral recovery;
it adds no exact byte match. The original ROM remains unchanged.
