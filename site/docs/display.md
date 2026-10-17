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
GBATEK documents lower OAM indices winning, OBJ winning equal BG priority,
transparent color0, affine centers, and unusual conflicts when OAM and BG
priority orders disagree. Native composition accepts nondecreasing OBJ BG
priorities, which is FCE4's ordinary sorted layer order; decreasing order returns-2.
The original unmasked tile-number overflow can alter priority bits, so some
cache-overflow interactions can also reach this unsupported case.

Physical OBJ06018000..0601FFFF mirrors06010000..06017FFF in normal mode1.
GBATEK's Memory Mirrors section and
[mGBA's VRAM store implementation](https://github.com/mgba-emu/mgba/blob/master/src/gba/memory.c)
establish this mapping. Cache transfers must apply the mirror before presentation;
passing an independent64KiB logical window would incorrectly retain overwritten data.

Unsupported display modes/layers, windows, blending/semitransparent objects,
nontrivial mosaic,4bpp objects, and decreasing priority order return-2. A
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

There are635 independent full-frame pixel comparisons:64 render from the
original-generated VRAM/map,311 use every real sprite image through native
FCE4,256 exercise synthetic raw OAM including128 affine cases, and4 verify
overlap/transparent holes at each BG priority. Synthetic cases include odd
tile numbers, physical tile wrapping, both coordinate wrap periods, flips,
fractional scaling, shear and extreme signed matrices.

`tests/test_display.c` passes ASan+UBSan with418281 assertions,40 rejection
guards and1308 native frames, including all311 decoded sprite resources after
freeing the ROM. The report is `native-display-validation.json`; tests record
source hashes and preserve the authorized ROM identity. These checks establish
the bounded compositor, not a completed game executable or hardware-cycle
emulation.

Actual decomp.wtf matching for074F0 stopped after five manual compiles at83.2%
(score235), not an exact match. All14 instructions are present; the remaining
difference is base/zero register allocation and initial load scheduling. The
claim was released with notes. `recovered/display_defaults.c` retains the best
candidate; the attempt history and reproducible helper are checked in.
