# Original world rasterizer: textured columns

`src/world_raster.c` recovers the complete ARM routine at ROM `080002F4`:
68 bytes copied by `07014` to IWRAM `03000270`. It draws actual indexed texture
pixels into the original world framebuffer. This is a native raster primitive,
not a complete world renderer or a playable executable. It does not execute ROM
instructions at runtime.

## Established render call chain

The exported assembly establishes these calls and formats:

| Stage | Original address | Evidence / scope |
| --- | --- | --- |
| Display-local view selection | `08002B88` | Clears via `07C94`; BC bit8 selects `08420` automap. Otherwise builds camera sine/cosine at `03004020+10/+14`, then calls `B6B0`. |
| World BSP traversal | `0800B6B0` | `B574` setup; `B220` side, `B428` bounding-box checks, `B18C` reject, `B7B0` leaf preparation. Calls `017CC`, then `D2D0`, then `01828`. |
| Wall submission | `080017CC` | Starts with `00A84`, traverses indices from `03003F60` through 60-byte records at pointer `03003F68`, calls optional `00E3C`, then `BE60` per record. Calls `AB10` after the wall list. |
| Wall projection / clipping / texture selection | `0800BE60` | Large unrecovered routine. Calls the relocated ARM sampler through interworking thunk `0800036C`. |
| Indexed vertical texture sampler | `080002F4` / `03000270` | Fully native and differentially verified here. |
| Floor/ceiling span preparation | `0800AB10` / `0800AA4C` | Processes 252-byte records with per-column top/bottom limits, closes spans through `A98C`. Not recovered here. |
| Flat-color or sky span | `0800A98C` | Nonzero byte `03004930+708` selects solid fill `030005AC`; zero selects sky copy `030005BC`. This is not a textured-plane sampler. |

`DBDC` prepares sprite visibility/cache state and `FCE4` constructs OAM state;
neither is the world renderer. The later `D2D0` and `01828` render operations
remain to be characterized. The root render-mode module has its own bounded
recovery; the table above does not claim that all listed stages are native.

`07C94` clears `0xE10` words (14,400 bytes) for the ordinary world view. Column
output stride is 120 bytes and vertical clipping in `BE60` uses 119, establishing
a 120 by 120 buffer of palette indices. The automap has a separate larger clear.
Presentation, hardware scaling, sprites and palette effects are separate stages.

## Texture data and sampling

The WOD0 archive loader and mip name joins are native in `texture_assets.c`.
Their independent tests execute the original `BE60` block `C9DA..CA4A` for every
real image column, both BIOS-copy and DMA paths: 55,440 bounded block executions.
See `analysis/texture-assets.md` and `native-texture-assets-validation.json`.
Those blocks establish column-major data:

```
column_height = original_height >> mip
column_source = selected_mip_pixels + selected_column * column_height
```

The `C9DA..CA4A` branch copies that column to scratch at `03005080+298`.
Other wall portions use separate scratch offsets. `BE60` then supplies six words
at `03005080` to the ARM routine:

| Offset | Meaning |
| --- | --- |
| `00` | Destination pixel pointer |
| `04` | Texture-column pointer |
| `08` | Unsigned 32-bit texture fraction |
| `0C` | Unsigned 32-bit texture step |
| `10` | ARM `LDRB` instruction used to select the logical shift |
| `14` | Pixel count (wall caller stores the low byte) |

For column heights 4, 8, 16, 32, 64 and 128 the caller chooses shifts 30, 29, 28,
27, 26 and 25, respectively. Its default instruction is logical shift 32, which
always selects texel zero. The sampler does not divide by a texture dimension or
apply a separate modulo: it extracts the high fraction bits and wraps additions
at 32 bits. Texture index zero is an ordinary pixel, not transparency.

For each pixel, the ARM routine reads `texture[fraction >> shift]`, adds step to
fraction, then stores the byte and advances destination by 120. The logical
shift-32 case is handled explicitly in C, avoiding an undefined C shift.

After all pixels, the routine compares **signed step** to **signed final
fraction**. Unless step is strictly greater, it overwrites the last pixel with
the preceding framebuffer pixel. This applies after arithmetic wrap and even
when count is one: then it reads the preexisting byte at `destination-120`.
It is not equivalent to duplicating the previous texture sample before drawing.
The native implementation preserves read/write order, including aliased source
and destination, and implements the signed comparison without overflowing C
signed arithmetic or relying on implementation-defined integer conversion.

## Native contract

`ssa_world_raster_column` takes framebuffer storage/capacity, destination byte
offset, texture storage/capacity, fraction, step, count and sample shift. Its
fixed stride is `SSA_WORLD_RASTER_STRIDE` (120). The supported shift set is the
actual wall caller's 25..30 and 32. Count is bounded to the caller's 1..120.

It validates every sampled index and destination position before any pixel
changes. Invalid parameters return `-1` atomically; success returns zero. A
one-pixel call requiring the predecessor correction must include that earlier
row byte in the provided framebuffer span. The original accepts invalid counts
and addresses and can run until counter wrap; the native API rejects them.
Source/destination overlap is supported and preserves original sequential
propagation. The caller must supply valid, stable allocations of the stated
capacities. Projection, clipping, mip/column selection, palette conversion and
presentation remain caller responsibilities.

## Differential proof

Run under the workspace Linux environment:

```
.venv-linux/bin/python tests/test_world_raster_rom.py
```

The test compiles the actual native raster and native texture loader with C11,
warnings as errors and UBSan. It executes all original ARM instructions at their
real IWRAM address, checks every framebuffer byte and source byte, input argument
preservation, callee-saved registers, stack and final accumulator. No original
routine or effect is replaced by a hook.

The ARM routine self-modifies its sample instruction, with two following NOPs
for the hardware pipeline. Unicorn otherwise executes a stale instruction from
its cached translation block. The test therefore executes the first three real
instructions (including the modifying `STR`), invalidates that translation cache
range, then resumes at the unchanged original PC. It verifies the actual patched
opcode. This test-engine synchronization is not part of native game code and
does not supply the sampled pixel or replace any instruction.

Final result: **67,678 complete ARM comparisons**, including every legal column
length, every original shift, fraction/index/sign/wrap boundaries, 4,096 random
cases, 1,050 aliasing cases and 2,681 one-pixel cases. The final overwrite branch
runs 39,288 times; its opposite runs 28,390 times. Another **44 native guards**
check atomic failures and bounds.

Of those complete comparisons, **7,992 use all 666 real mip images**, with first,
middle and last columns and four phase/step/count combinations. The texture
pointers passed to native raster code come directly from the native archive
loader's allocations. Original ARM execution receives exactly those real bytes;
all full framebuffer bytes agree. This connects genuine resources to genuine
native raster pixels, but does not prove a whole projected scene. Results and
source hashes are in `analysis/native-world-raster-validation.json`.

The findings were recorded with the actual decomp.wtf `add_note` tool on
`sub_0800BE60`. No new exact match is claimed: the recovered ARM routine is
outside the current Thumb catalogue, and no speculative function split was made.

## Remaining rendering work

Wall projection/clipping, BSP visibility and plane/sky composition now connect
in `world_scene.c`; see `world-scene.md` for complete geometry pixel comparisons
and `native-viewer.md` for the interactive Windows geometry viewer. This module
remains the bounded column primitive. Later actor/sprite passes, HUD composition,
original hardware scaling and the animated third-person body still require work.
