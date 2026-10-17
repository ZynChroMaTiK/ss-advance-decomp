# Native BE60 wall projection and portal rasterization

`src/wall_renderer.c` implements the complete world-wall drawing control flow
of `0800BE60`: one-sided walls and two-sided portals, perspective interpolation,
upper/middle/lower texture sampling, mip selection, plane limits and sprite
clipping metadata. It uses native decoded maps, texture bindings, the plane
pool and the previously verified ARM-derived column rasterizer. No ROM code
executes in the native renderer.

This does not recover later actor rendering (`D2D0`, `01828`), sprite blending,
weapon/HUD composition, palette effects or native window presentation. The
connected static-world renderer is described by the separate scene proof below.
No new exact compiler match is claimed.

## Wall command and shared state

The original input is the 60-byte visible-wall record constructed by `BA04`.
`SsaWallRenderCommand` retains its meaningful inputs with host-width pointers:

| Original offset | Native field / meaning |
| --- | --- |
| `04`, `08` | `depth_left`, `depth_right`: reciprocal-depth endpoints |
| `0C`, `10` | `u_left`, `u_right`: texture-coordinate endpoints |
| `14`, `15` | `left`, `right`: screen endpoints; right may be 120 |
| `16` | `segment`: lower 15 bits index a 16-byte map segment |
| `18`, `1A` | `range_count`, `ranges`: inclusive visible screen ranges |
| `20`, `24` | `ceiling`, `floor`: stable native plane handles |

Actual draw ranges end at 119; original horizontal occlusion removes the right
sentinel 120. The uncut right endpoint still participates in interpolation.
The renderer reads the segment's line and side, current sector heights, current
side texture indices and line flags. Map texture bindings provide original
dimensions and archive/mip correspondence. It does not substitute a raycaster.

The reciprocal table at `08034934`, two mip-transition tables at `0844AEDC`
and `0844AEEC`, and constant clip arrays are copied as data by
`ssa_wall_render_tables_load`. The caller keeps camera, geometry, bindings and
textures stable for a rendering pass. Sector/side changes between passes are
read afresh. Original sector and texture pointer caches are an optimization;
this host API resolves them directly rather than exposing GBA addresses or
self-modifying code. Framebuffer/opening storage must be separate from map,
texture, command, result and plane metadata storage.

`SsaWallRenderResult` projects the original mutable wall fields:
sprite clip flags at`+28`, clipping heights at`+2C/+2E`, and clip arrays at
`+30/+34`. Initialize it before first use: an unused height retains its prior
value just as in the original record. `top_clip_x` and `bottom_clip_x` express
the screen column corresponding to pointer element zero, avoiding the original
practice of storing a pointer before an allocated clip slice. The original
opening copies have length `right-left`, excluding the right endpoint.

Plane handles returned in the result represent the final `WORLD+6C/+68`
globals. The command's original handles remain unchanged, including after a
plane split. Clipping storage uses `SsaPlanePool.opening_cursor`; floor plane
checks precede ceiling checks for every visible range.

## Projection and original quirks

The interpolation denominator is the **uncut** `right-left+1`. It uses the high
halfword of the reciprocal table entry, not native floating-point division.
The texture-U interpolation includes the original `+0x2000` left and `-0x2000`
right adjustments. Skipped horizontal ranges advance all active interpolants
by their wrapped increments. Ceiling/floor projection uses the original fixed
multiply, 1.5 scaling and vertical center 60. Signed halfword truncation,
32-bit wrap and arithmetic right shift are retained without C signed overflow.

Upper and lower portal walls follow original independent height comparisons,
texture-presence gates and plane-color changes. Closed portals select constant
sprite clip arrays. Texture pegging uses line flag 8 and line`+24` for upper
walls, and flag 16 for the one-sided middle texture. Lower texture anchoring
retains the original behavior even though its code reads flag 16 without using
the result.

Mip choice uses signed reciprocal-depth thresholds 4999/5031 and 9999/10063,
with the original data tables in the transition bands. For a portal with a
lower wall, its descriptor supplies the shared width/height sampling parameters
even when drawing the upper portion. Every read remains bounds checked against
the actual native image allocation. Middle-wall fractions above`F0000000` are
clamped to zero; the upper/lower paths do not apply that clamp.

The original ARM sampler's final-pixel correction can read one row before the
framebuffer when a top-row wall becomes one pixel tall. This occurs in genuine
level 7 portal data, not only synthetic inputs. `framebuffer_offset` therefore
identifies world pixel(0,0) inside a caller-owned storage span. Reserve a 120-byte
prefix and initialize it explicitly. The native renderer reads those bytes
exactly; it does not invent a black border or wrap to the bottom row. The scene
adapter owns 14,520 bytes, zero-initializes the prefix, and preserves it on clear.

`ssa_wall_render` returns 0 on success and -1 on invalid or exhausted storage.
Earlier plane/pixel effects survive a later error and the update guard clears.
The retained `ssa_wall_render_solid` wrapper returns -2 before effects for portal
commands; the general function handles them natively. Invalid original memory
accesses and unsupported malformed dimensions become native errors. This is not
a promise to reproduce undefined reads outside the declared allocations.

## Whole-routine differential validation

Run `tests/test_wall_renderer_rom.py` with the workspace Linux Python. It builds
the actual C sources with C11, warnings as errors and UBSan, and executes the
whole original `BE60` to its return. It compares all framebuffer bytes including
the prefix, all 120 column limits, all 80 plane records, plane handles, all mutable
wall-record fields, opening bytes and cursor. Camera/map data are genuine native
loads and original map texture descriptors are replaced with the same recovered
binding values.

The only external effects modeled are BIOS/DMA memory copies. Original fixed
math, plane checks/splits and the real ARM column rasterizer execute. Unicorn's
translation cache is synchronized after the sampler's actual self-modifying
store; no sampled pixels or projection results are supplied by the harness.

Final proof: **14,322 complete original BE60 comparisons**:

- 7,207 solid-wall and 7,115 portal-wall runs across all 16 maps.
- Every 5,927 valid one-sided and 5,067 valid portal segment in the supplied maps.
- 2,048 additional live-sector/closed-portal/color/pegging/missing-texture cases.
- 239,251 actual ARM column calls and 145,281 BIOS/DMA copy effects.
- 12,104,309 changed pixels, 9,154 plane splits and 243,048 clip bytes compared.
- Four actual top-row predecessor-read cases and 128 atomic native guards.

Results and hashes are in `analysis/native-wall-renderer-validation.json`.
The test deliberately seeds valid wall endpoints to isolate BE60. Its input
record generation is not independently claimed as BSP construction; that stage
has its own proof and is included in the connected scene test below.

## Connected static-world differential validation

`tests/test_world_scene_rom.py` runs the original
`B6B0 -> 017CC -> BE60 -> AB10 -> AA4C/A98C` chain, with all actual world raster
instructions and native equivalents connected through `world_scene.c`.
`D2D0` and `01828` are explicit excluded actor-pass boundaries. BIOS division,
copy and DMA are hardware models. The oracle does not replace wall submission,
projection, plane generation, sky copying or textured rasterization.

It compares 128 complete static-world views: all 16 original player-start XY
locations, four cardinal heading offsets, four further heading/eye-height
variants and a live containing-sector ceiling change. The explicit camera
inputs are shared with the original routines; this is not a full gameplay-frame
or original camera-initialization claim.

All **1,843,200 world pixels** match, together with the prefix, column clips,
all plane storage, opening bytes/cursor, row starts and plane-renderer globals.
The views contain 1,280 submitted walls, including 532 portals, 754 planes and
19,259 original ARM textured columns. There are 48 native update guards.
Hashes are recorded per view in
`analysis/native-world-scene-rom-validation.json`. Native palette-index buffers
are written under `build/native-tests/world-scene-rom-level-XX-frame-N.indices`.
These are actual reconstructed world pixels, not a mock scene or a raycaster.

The decomp.wtf engine receives these findings as notes on`sub_0800BE60`. The
behavioral port is separate from byte-identical GBA matching progress.
