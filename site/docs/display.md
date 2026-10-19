# Native original-resolution display composition

`src/display.c` presents the native world/actor renderer's120x120 indexed
framebuffer and native sprite OAM as240x160 RGB555 pixels. It executes no ROM
instructions. Its input palettes and physical OBJ tile memory remain explicit,
so palette animation, cache transfer order and HUD construction are not silently
replaced. `ssa_display_rgb888` optionally produces host0x00RRGGBB pixels by
replicating five-bit channels; this is a host color conversion, not LCD calibration.

## Original display evidence

Normal gameplay uses a tiled affine background, not a bitmap mode:

| Original routine | Observed behavior |
| --- | --- |
| `074F0` | Sets display-state PA128,PB0,PC0,PD192 and reference X/Y0. |
| `0750C` | Writes DISPCNT1441, BG2CNT6085/6089 and those affine parameters to hardware. |
| `06E30`/`06F38` | Clears both16KiB character banks, fills the32x32 affine map withFF, then writes the original column-ordered tile indices. |
| ARM `08000264`, copied to `03000314` | Copies the120x120 framebuffer as2258x8 tiles in column order, toggling between character banks06004000/06008000. |

DISPCNT1441 selects mode1, BG2, OBJ and one-dimensional OBJ mapping. BG2CNT
selects priority1, a256x256 wrapping affine map at06000000 and character bank1
or2. Tile number for world tile(tx,ty) is15*tx+ty. The map's extra column
references cleared tiles; remaining map cells use cleared tile255.

The resulting normal sample is `world[floor(3*y/4)][floor(x/2)]` across the
entire240x160 display. Horizontal pixels double; vertical output repeats one
source row in each group of three. A square120x120 host viewport therefore
does not preserve the original display aspect. General signed affine parameters
are supported against this recovered map layout, including wrapping and the
cleared area beyond120x120. This does not recover the code that changes those
parameters for other effects.

Automap takes a separate mode4 branch in0750C and is rejected by this compositor.
0750C also performs palette/status updates; the register oracle executes them,
but this module receives the resulting palettes rather than implementing their
gameplay state machine.

## OBJ rules and bounded scope

The compositor consumes all128 native `SsaSpriteOam.entries`, including their
interleaved matrix words, and32768 bytes of physical OBJ memory. It supports
normal8bpp one-dimensional tiled objects, transparent index0, horizontal/vertical
flips, affine matrices and double-size bounds, signed/wrapped positions, and
tile-address wrap. Matrix sampling uses the object/bounding-box centers with
eight fractional bits. Sprite palette selection remains separate from BG colors.

Hardware references are the original author's
[GBATEK GBA documentation](https://raw.githubusercontent.com/mgba-emu/gbatek/master/gba.md)
and the implementation's
[mGBA OBJ renderer](https://github.com/mgba-emu/mgba/blob/master/src/gba/renderers/software-obj.c).
GBATEK documents OBJ winning equal BG priority, transparent color0, affine
centers, and unusual conflicts when OAM and BG priority orders disagree.
Mixed priorities now render instead of returning-2. The reduction follows the
normal8bpp OBJ path in mGBA's software renderer: equal priority keeps the earlier
OAM sample; a later higher-priority opaque sample replaces it. A transparent
higher-priority sample preserves an already occupied pixel's color but promotes
its priority. Empty pixels stay empty, and affine samples outside the source
rectangle do not promote anything. The resulting single OBJ layer is then
compared with BG2. Sorting sprites by priority or simply ignoring transparent
samples would lose this behavior. The original unmasked tile-number overflow
can alter priority bits, so it must not turn an otherwise valid frame into a
native presentation error. These rules are a hardware-model implementation,
not a new exact byte match or a physical GBA measurement.

Physical OBJ06018000..0601FFFF mirrors06010000..06017FFF in normal mode1.
GBATEK's Memory Mirrors section and
[mGBA's VRAM store implementation](https://github.com/mgba-emu/mgba/blob/master/src/gba/memory.c)
establish this mapping. Cache transfers must apply the mirror before presentation;
passing an independent64KiB logical window would incorrectly retain overwritten data.

Unsupported display modes/layers, windows, blending/semitransparent objects,
nontrivial mosaic and4bpp objects return-2. A
conservative scanline cost check also returns-2 when the frame might exceed the
hardware budget. It includes the complete128-entry scan, offscreen-X objects
and un-clipped affine bounding boxes; it does not approximate hardware pixel
dropout. Actual FCE4 pieces have no mosaic-enable bit. Unsupported cases remain
explicit even when their output might happen to look similar.

Invalid inputs return-1. Both errors occur before any output writes. Output
must not overlap input registers, palettes, OAM, tiles or framebuffer; the API
checks those spans. No pointers survive the call. The function has no mutable
shared context, and rejects OAM while it is being updated.

## Evidence

`tests/test_display.py` runs448 complete isolated original calls:64 each074F0,
06E30 and ARM264, plus2560750C calls across all16 levels, both character banks,
both original copy paths and four game-flag states. Only BIOS/DMA copying is
controlled; register setup, tilemap generation and ARM tile conversion execute
the original instructions. This is test-only Unicorn use.

There are893 independent full-frame pixel comparisons:64 render from the
original-generated VRAM/map,311 use every real sprite image through native
FCE4,256 exercise synthetic raw OAM including128 affine cases, and4 verify
overlap/transparent holes at each BG priority. Another258 cover all priority
pairs and opacity combinations, the three-sprite transparent-promotion anomaly,
and mixed affine/normal overlapping pieces. Synthetic cases include odd
tile numbers, physical tile wrapping, both coordinate wrap periods, flips,
fractional scaling, shear and extreme signed matrices.

`tests/test_display.c` passes ASan+UBSan with418538 assertions,39 rejection
guards and1438 native frames, including all311 decoded sprite resources after
freeing the ROM. The report is `native-display-validation.json`; tests record
source hashes and preserve the authorized ROM identity. These checks establish
the bounded compositor, not a completed game executable or hardware-cycle
emulation.

Both playable renderers use this compositor: GBA mode for the display, and the
PC renderer for weapon/HUD transparency. The latter now has a directed
three-sprite regression in addition to its seven output sizes and24 live chase/
jump frames. Its older fixture assumed the chase camera could always appear at
the entrance wall; the fixture now walks away first and respects collision
fallbacks. Gameplay camera behavior is unchanged. GCC sanitizers and MSVC pass.
Original HUD color checks still pass32cases/64scanline interrupts/1228800pixels.
Run `scripts/test-display.ps1 -SkipBuild` after a static Windows build; its
hash-bearing report is `windows-display-validation.json`. No campaign replay
was required or run.

Actual decomp.wtf matching for074F0 stopped after five manual compiles at83.2%
(score235), not an exact match. All14 instructions are present; the remaining
difference is base/zero register allocation and initial load scheduling. The
claim was released with notes. `recovered/display_defaults.c` retains the best
candidate; the attempt history and reproducible helper are checked in.

Aspect ratio preference (2026-10-19): Settings now places Aspect ratio below
Renderer. Native (default) renders across the selected software output size;
GBA fits a centered3:2 render area with black margins at presentation. Actual
software dimensions are constrained before rendering, so world perspective and
HUD layout use3:2 without stretching or cropping. Auto dimensions track the
client; explicit resolution bounds are constrained the same way. Existing
pause/NETRICSA captures inherit the view dimensions and their pointer mapping.
Menus and original GBA output retain their existing3:2 presentation.
The GBA renderer disables the row, displays gray GBA, ignores clicks/activation
and skips it during keyboard navigation. Its software preference is retained.
A12th SsaSettings field stores Native/GBA. Version9 retains the16-byte CRC file,
using byte8's high bit for aspect and its low7 bits for version; versions1..8
migrate to Native. Clear Data resets it along with other settings. The ctypes
Menu structure has been synchronized to the complete12-field settings layout.
Validation: settings ASan/UBSan/leaks=1 and MSVC38958 core checks pass; Windows
23614 settings/input/audio/persistence checks pass. Twelve actual render/present
cases cover both renderer modes and both aspect choices at854x480,640x480 and
480x640, with every black-margin pixel checked and no simulation advance.
Native/GBA rendering and all three localized Settings captures were inspected.
HUD sanitizer/MSVC357231 checks/409 draws plus Windows560 checks/768 PNG pixels
pass. Current native menu sanitizer76171 checks/10699 frames passes.
An additional tests/test_menu_rom.py run FAILED on the already modified main
menu: it expects four original rows at y82, while the existing Quit extension
uses five at y74. That comparison is not a passed check and was not bypassed;
its ctypes settings layout was corrected, but its old visual oracle needs a
separate policy for intentional menu extensions. No menu behavior was reverted.
Static125libraries/frontend rebuilt; executable SHA256:
ccf5b4a6179aa9ca96d4b3ca1d67918a555f358e4f206ef7d13eac9b54993906
See aspect-ratio-validation.json, audio-settings-validation.json and
windows-audio-settings-validation.json. ROM unchanged; no campaign replay or
physical-input check was performed for this change.

Back-row adjustment (2026-10-19): both Settings and Extras move Back from y152
to y144 (one tile), with pointer hit areas moved accordingly. Settings retains
slider interaction through y143 and places Back at y144..151; Extras uses
its usual8px upper hit inset, y136..151. Eight-pixel-high volume bars now end
at y143 to avoid drawing over Back. Other item positions and keyboard order
remain unchanged.
Validation for Back positioning: existing MSVC38958 core and Windows23614
settings checks pass; Settings and Extras captures inspected. Static125libraries
and frontend rebuilt; SHA256 a1f8766456f5ee024674693462ef30b6bc2fe362f07d376e25f9f25e8b69daf7.
See windows-audio-settings-validation.json for current source hashes. No new
tests or full campaign replay were added for this visual adjustment.

Settings spacing correction (2026-10-19): Aspect ratio now displays Native/3:2
(the disabled GBA-renderer row also displays3:2). Stored choices and rendering
are unchanged. Volume sliders and percent text move from104/136 to96/128,
one tile nearer labels88/120, with their pointer hit areas following BAR_TOP.
Back remains144; the bars remain eight pixels high.
Settings-spacing verification: MSVC38958 existing core checks pass, including
slider pointer updates. Native/disabled3:2 captures inspected. Static125libraries
and frontend rebuilt; SHA256 8f45d96387d48665579286290a1d255e27c023238de8a5cd0669bd99c1ab0941.
Result/captures: build/settings-spacing-tests/. No new tests or broad campaign/
launcher/sanitizer rerun for this presentation-only change.
