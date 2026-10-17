# Original sprite image assets

`src/sprite_assets.c` decodes the actual weapon/HUD sprite resource table and
gameplay palettes from the user's AENP ROM. It owns all copied data and provides
a static indexed raster for each image. It does not run ROM instructions, draw
a scene, implement the world renderer, or supply invented artwork.

The table at **0844B710** contains **311** pointers, ending at0844BBEC. Every
entry points to an `AGB!` image. Together they contain **1,261 pieces**, **6,730
8x8 tiles**, and **430,720 pixel-index bytes**. The existing233sprite animation
sequences reference image IDs through310, independently agreeing with this
table boundary. Some images are selected directly by controllers rather than
appearing as animation sequence words.

## Format and original consumers

| Offset | Size | Meaning |
| --- | --- | --- |
| 00 | 4 | ASCII `AGB!`; original loader also recognizes compressed `AGB*` |
| 04 | 2 | Piece count |
| 06 | 2 | Number of64byte tiles |
| 08 | 4 | Four signed-byte culling bounds consumed by DBF4 |
| 0C | 16 | Reserved/unused by the inspected path |
| 1C | count*8 | Piece descriptors |
| 1C+count*8 | tiles*64 | Uncompressed8bpp tile payload |

Each piece is `(u16 attribute0, u16 attribute1, u16 tile, u8 width, u8 height)`.
The low9bits of attribute0 and attribute1 are signed Y and X offsets, respectively.
Their top2bits encode the original OAM shape and size. All actual resources use
only those position/shape/size bits; runtime color, flipping and affine flags
are supplied separately by FCE4. Width/height redundantly encode the shape's
dimensions and are validated against it. The tile index is relative to this
image's payload. Tiles within a piece are in row order; each8x8tile stores one
palette index per pixel. Index0 is transparent.

`DBDC` is **sprite visibility/cache preparation**, not the world renderer:

1. `DBF4` computes screen bounds and updates sprite flag4 visibility.
2. `DDA8` retains cached frames and queues missing frames; weapon type2 has
   its own cache request path.
3. `DEF8` ages entries and releases expired cache allocations.
4. `DF80` processes up to8pending requests against a10,000byte budget.

DF80 reads the image pointer, piece count and tile count, then computes the
payload exactly as above. It allocates tile storage with E0A8, marks the cache
entry, and calls **E320** with `(payload, tile_count, destination_tile)`.
E320 appends a12byte transfer descriptor `(source, destination_tile, count)`
followed by a zero source sentinel, subtracting `count << 6` from the budget.

`E358` later executes those descriptors. For normal images it copies
`tile_count*64` bytes to `06010000 + destination_tile*64`, using DMA for single
player or BIOS CpuSet for multiplayer. The signed high bit of count selects
BIOS decompression for the AGB* path. **No actual entry in this311image table
uses AGB*.** The native decoder explicitly rejects it rather than claiming
untested compressed sprite support. Existing map LZ77 support is separate.

`FCE4` builds hardware OAM from the cached images. It advances descriptors in
their original order, adds each image-relative tile index to the cache slot,
and doubles that tile number because OAM uses32byte tile units while these
pixels use64byte8bpp tiles. It supplies the8bpp bit and handles runtime position,
flips, scaling, clipping and object flags. Those runtime operations remain
unimplemented here. Earlier native frame enum `SSA_FRAME_RENDER_WORLD` still
names DBDC for compatibility; the name must not be read as recovered world
rendering.

## Palette evidence

Original level setup **1F44** passes these pointers/counts to **07014**:

| Use | Source | Selection |
| --- | --- | --- |
| Sprite palette | 0879EFFC,256RGB555 halfwords | Both families |
| Background palette0 | 08454A5C,256halfwords | Levels0..5,12,13 |
| Background palette1 | 08454C5C,256halfwords | Levels6..11,14,15 |

07014 copies them to `05000200` and `05000000`, then explicitly clears entry0
in each destination. The native loader retains that zeroed result. Host RGBA
conversion expands each five-bit RGB channel by bit replication; index0 has
alpha0 and other entries alpha255. This conversion is a host convenience,
not an original software color-conversion routine. Runtime palette effects,
brightness/blending and menu-specific palettes remain separate.

## Native API and safety

Zero-initialize `SsaSpriteAssets`, call `ssa_sprite_assets_load`, and free with
`ssa_sprite_assets_free` before reuse. The load is atomic: malformed data or
allocation failure leaves the destination unchanged and frees partial ownership.
A loaded destination is rejected. ROM storage may be released after success.

Each `SsaSpriteImage` owns a part array and its original tiled pixels, plus its
ROM offset, original culling bounds, and the union rectangle of all full pieces.
The union includes transparent tile padding and can differ from culling bounds.
`min_x/min_y` locate that rectangle relative to the original sprite origin.

`ssa_sprite_image_indices` expands pieces into a caller-owned row-major indexed
raster. It validates the whole image and output span before writing, retains
row padding, and gives earlier OAM pieces priority over later nonzero pixels.
It supplies unscaled, unflipped asset pixels; runtime transforms are not applied.
Pointers and metadata must remain valid while referenced, and output cannot
alias asset storage. `ssa_sprite_palette_rgba` converts the copied palette.

The loader checks the fixed table span, every ROM pointer/header/payload span,
nonzero bounded counts, shape/size/dimension agreement, reserved attribute bits,
and every piece's tile range. Counts are bounded to128pieces and512tiles to
avoid unreasonable allocations from malformed resources. All original images
are smaller:1..21pieces and1..138tiles. Unsupported compressed/malformed assets
fail explicitly. Neither ROM identity verification nor downloading assets is
part of this library; the existing ROM ownership/identity contract still applies.

## Validation

`tests/test_sprite_assets.py` and its ASan/UBSan C fixture verify:

- All311images,1,261descriptors and430,720tile bytes against exact original data.
- **622 complete DF80/E320/E358 chains**, one per image for both original copy
  paths. Only tile allocation and BIOS/DMA hardware transfers are controlled;
  real queue construction, cache bookkeeping and source/size computation execute.
- **311 full FCE4 calls producing1,261 original OAM descriptors**, using one
  prepared non-affine sprite at a time. Only linked-list preparation helpers
  are controlled. Every emitted position, shape, size and tile offset agrees.
- **32 whole07014 palette setups**, covering all16levels and both copy paths,
  with hardware waits/setup callbacks controlled and final palette bytes compared.
- Independent tile-block expansion for every image, including preserved row
  padding; **2,183 malformed inputs**, **939 guards**, and **2,488 sanitizer
  assertions**. ROM storage is freed before the standalone decoder checks.

`native-sprite-assets-validation.json` records hashes, counts and every image's
decoded index raster hash. Temporary weapon/HUD previews under ignored
`build/native-tests/` were visually inspected and contain actual recognizable
ROM artwork. No extracted image is copied into source directories or published.
This validates asset data and inspected original consumers, not a native runtime
renderer, affine pipeline or playable executable.

## Matching and subsequent renderer work

Through the requested local decomp.wtf driver, **0800E320 matched exactly:
56bytes,28instructions, first manual compile_and_diff attempt**. Its source is
preserved in `recovered/sprite_transfer.c`; the engine automatically released
the matched claim. DBDC was already matched and is not counted again. The new
export includes E320 among69 exact C functions and independently rebuilds the
entire ROM byte for byte; see `rebuild-validation.json`.

The trace also found concrete world texture archive leads for subsequent work:
resource table0844AEF4 entries16..18 and19..21 are uncompressed WOD0 archives
loaded by AFF4. They contain101and121named textures in three resolution levels,
with16byte records `(name[8], width, height, relative_payload_offset)`. Sources
begin at08526F48,085FEDE0,08635278 and08643090,08749868,0878B840. AF18 joins
matching names across resolutions; the family selection follows1F44 above.
Their dimensions and raw8bit payloads are visible in the ROM, but raster order,
world sampling, sky/wall/floor behavior and actor image formats need their own
recovery. They are not decoded or substituted with sprite assets in this module.
