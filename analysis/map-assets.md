# Native level asset loading

`src/map_assets.c` now reads all 16 level resources from the supplied AENP ROM,
decompresses them in native C and prepares the line fields used for collision.
The native loader contains no emulator and leaves the ROM unchanged. Its output
is owned memory with relative offsets; it does not put 32-bit GBA addresses into
host pointers. Initialize `SsaMapAssets` to zero, load once, then call
`ssa_map_assets_free` before reusing it. The caller must identify its ROM;
the loader's address table is specific to this ROM version.

This is map data loading, not a complete level initializer or playable game.
Texture lookup/binding is supplied separately by `map_texture_bindings.c`.
BSP parent bookkeeping, entity creation and renderer allocation remain separate
work. Header pointers and sector list pointers stay
relative to the decoded data. Texture descriptors retain their original bytes;
host resource bindings are described in `analysis/map-texture-bindings.md`.

## Evidence and format

The level initializer at `0x08001FEC` passes the selected index to `0x0800ACC8`.
That routine indexes the resource address table at `0x0844AEF4`, reads the upper
24 bits of the first word as its allocation size, then calls the LZ77 BIOS
wrapper at `0x080C5324`. Map indices 0–15 are compressed; indices 16–21 are
uncompressed texture packages handled separately by `0x0800AFF4`.

The map stream uses GBA type-0x10 LZ77: a four-byte type/decoded-length header,
then flag bytes interpreted from bit 7 downward. Clear bits encode one literal;
set bits encode a two-byte big-endian pair with length `(pair >> 12) + 3` and
back distance `(pair & 4095) + 1`. Back references can overlap their destination.
The native decoder rejects truncated input, references before the beginning,
output overflow and back references crossing the declared output length.

Decoded headers are 0x48 bytes, little endian, with magic `WOD0` at +0.
The halfword at +4 is decoded size divided by four. Header arrays are:

| Count offset | Relative offset field | Record bytes | Meaning |
| --- | --- | --- | --- |
| +06 | +1C | 12 | Things/spawn records |
| +08 | +20 | 16 | Texture descriptors |
| +0A | +24 | 20 | Sectors |
| +0C | +28 | 32 | BSP nodes |
| +0E | +2C | 8 | BSP leaves/subsectors |
| +10 | +30 | 16 | Segments |
| +12 | +34 | 12 | Sides |
| +14 | +38 | 40 | Lines |
| +16 | +3C | 4 | Vertices: signed X,Y halfwords |
| +18 | +40 | 2 | Reject bitset storage, count in halfwords |
| +1A | +44 | 2 | Blockmap storage, count in halfwords |

The original loader relocates nonempty record arrays into GBA RAM pointers.
Sector records have a list count at +8 and relative list pointer at +C;
`0x0800ACA8` relocates that pointer. The native loader bounds-checks these lists
without claiming that their full semantics have been recovered. `0x0800ACA0`
and `0x0800ACA4`, called after loading nodes and leaves, are empty routines.

Node children at +18/+1A select another node or a leaf, with bit 15 indicating a
leaf. Every actual node child references an earlier node, so the native loader
checks that ordering to reject cycles. Leaves contain segment count at +0,
first segment at +2, sector index at +4 and runtime bookkeeping at +6.

The blockmap starts with signed origin X,Y and unsigned width,height halfwords.
Then `width * height` halfword entries give list offsets in halfword units from
the start of the blockmap. `0x0800975C` skips a leading dummy word in each list,
then visits line indices until `0xFFFF`. Cells cover 128 world units. The loader
checks every list against the declared blockmap section and line count. Grid
iteration/deduplication and runtime entity grids are not part of this loader.

## Line preparation

The code at `0x0800AD4C` through `0x0800ADF4` computes these line fields:

| Offset | Field |
| --- | --- |
| +00, +02 | Start/end vertex indices |
| +04, +06 | Signed 16-bit delta X,Y |
| +0C, +0E | Adjacent sector indices; second requires two-sided flag |
| +10, +12 | Maximum/minimum Y |
| +14, +16 | Minimum/maximum X |
| +18 | Slope classification |
| +1A | Flags, including bit 2 for two-sided lines |

Slope classification is 1 for zero delta X, 0 for zero delta Y, otherwise 2
when signed integer `deltaY / deltaX > 0`, and 3 otherwise. The division detail
matters: a positive shallow slope with truncated quotient zero is classified 3.
Native code preserves this behavior and 16-bit delta wrapping. Bounds and array
overlap checks run before any preparation writes.

## Validation

Run `.venv-linux/bin/python tests/test_map_assets.py` inside Ubuntu/WSL.
`analysis/native-map-assets-validation.json` records per-level ROM offsets,
compressed/decompressed sizes, counts and raw decoded SHA-256 hashes.

- All 16 streams agree byte for byte between the native decoder and an
  independent Python decoder; decoded maps range from 22,916 to 147,960 bytes.
- All 7,632 line records agree with the original Thumb preparation code. The
  only hook in that comparison supplies signed integer division; the original
  line instructions execute unchanged in the test oracle.
- 164 malformed/truncated input cases cover headers, output size, back
  references, section bounds/aliasing, BSP indices/cycles, leaf sectors, line
  vertices/sectors and blockmap bounds. Native compilation enables strict
  warnings and undefined-behavior checks.

These checks verify decoded data and collision setup. They do not verify texture
rendering, spawn behavior, the complete original level initializer or full-game
integration. Names and campaign ordering have not yet been mapped to resources;
the public loader accepts the original numeric resource index.
