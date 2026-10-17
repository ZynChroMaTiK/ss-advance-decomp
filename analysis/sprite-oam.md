# Runtime sprite OAM construction

`src/sprite_oam.c` recovers the complete original `0800FCE4`, including its
three preparation helpers: E648 resets OAM, FC3C resets layer sentinels, and
FC58 builds the linked sprite order. It consumes the actual `SsaWeaponObject`
projection, `SsaSpriteAssets` pieces, shared cache entries and recovered math
tables. The output is128 raw8-byte OAM records with interleaved affine matrix
words. This module constructs descriptors; it does not approximate hardware
pixels, replace tile residency, or implement a completed presentation pipeline.

## Inputs, sorting and visible state

FCE4 uses the84-slot sprite pool's active index order and the16-byte layer
mapping at0844BBEC. FC3C links17 sentinel nodes; FC58 prepends each active object
to its selected layer. Equal-layer objects therefore occur in reverse active
order. Layer15 is emitted first with hardware priority0. Layer14 and then
layers2..13 have priority1, layer1 has priority2, and layer0 has priority3.
The native API supports the known layer range0..15, rejecting an out-of-range
layer rather than reading unrelated bytes after the original mapping table.

The host's `object.link` remains a32-bit opaque field. Its normalized native
references are0 for end,1..17 for sentinel nodes, and
`80000000 | (slot+1)` for objects. They are never host or ROM pointers. Tests
normalize the original GBA links to this representation and compare every
active link and all sentinel nodes. The object array and bindings remain stable
while constructing OAM.

Each visited object first receives `screen_x = signed16(x >> 8)` and the same
conversion for Y. Objects beyond the128-piece output limit are not visited,
so their old screen coordinates remain. Linked-list construction still occurs
for the complete active prefix before that limit is reached.

The image source is **previous_frame**, not the animation's current `frame`.
Bits8000/4000 select horizontal/vertical flipping, and the lower14 bits select
one of311 images. Drawing requires flag1, a non-FFFF animation, a non-3FFF
image, flag4 visibility, and either owner0 or `owner-1 == local_index`.
Counter mask4 suppresses objects with flag2. A zero cache residency byte skips
the image. The cache tile base and each piece's relative tile are added and
doubled, preserving the original conversion from64-byte8bpp tiles to32-byte
OAM units. Tile allocation, transfers and cache timing belong to the separate
native cache owner.

## Piece projection and affine state

Piece offsets use the original signed9-bit positions. Flipping negates the
offset and subtracts the piece dimension. Non-default scale changes the
offset about the piece center; enlarged sprites receive the original extra
half-dimension subtraction for double-size affine bounds. Signed scale0 means
4096. Negative scales are retained, including wrapped multiplication and
arithmetic right shifts.

Vertical culling preserves the exact unsigned test `y + 64 <= 224`, admitting
positions from-64 through160. X wraps to9 bits and Y to8 bits in OAM. There is
no extra native horizontal cull. Attribute shape/size bits come from the real
asset descriptors. FCE4 sets8bpp mode and maps object flag04000000 to attribute0
bit0400, the original semitransparent-object request. Interpretation of that
request belongs to the display compositor.

Up to32 objects receive affine indices. Scaling uses signed division of
16777216 by the signed scale, incorporating frame flips. Rotation uses the
original094E8 sine/cosine data and the recovered signed fixed multiplication.
The final matrix words are arithmetic-shifted by4 and truncated to16 bits.
OAM reset initializes every matrix to the8.8 identity. Matrix words share the
fourth halfword of every OAM entry; later attribute writes preserve them.

Two unusual original behaviors are retained. A matrix is written only while
processing descriptor0. If that descriptor is vertically culled but later
pieces are visible, the object still consumes an affine index without writing
a matrix. The matrix write cursor advances only when a matrix is written, so
it can diverge from the affine index. After32 affine objects, following objects
still use their scaled positions but fall back to non-affine OAM flip bits.
Neither behavior is corrected by guessing a different visual result.

At entry, the original update flag gains bit1 and transfer_flag becomes0. E648
resets all128 entries to disabled objects and identity matrices. The MOSAIC
register is constructed as the low16 bits of `(mosaic << 12) | (mosaic << 8)`.
The update flag's low bit is cleared on exit; all other bits persist. Current
real image parts do not set the object-mosaic enable bit, but the register
value is still preserved for the external display state.

## Native contracts

Zero-initialize `SsaSpriteOam` and call `ssa_sprite_oam_init` with the user's ROM
to copy its16 layer buckets. Before each build, project the original local
index, counter and mosaic fields. Call `ssa_sprite_oam_build` after sprite
controller/visibility work, with stable dispatch objects, assets, cache entries
and math tables. The module owns no external storage and retains no ROM
pointer. It does not change cache residency or tile contents.

The build rejects invalid active counts, duplicate slots or object pointers,
missing active objects, unsupported layers, invalid eligible image IDs,
malformed piece storage and nested updates. These checks precede mutations,
so failures leave OAM, list links and screen coordinates unchanged. Inactive
bindings need not be populated. Inputs must not alias output storage and must
not be modified concurrently. Native guards replace the original unbounded
reads or cyclic lists for malformed inputs; they do not represent additional
original game behavior.

## Validation and matching

`tests/test_sprite_oam_rom.py` executes **6,036 complete original FCE4 calls**,
including real E648, FC3C, FC58, division, sine/cosine and relocated ARM fixed
multiplication. No callee is replaced. A read-only completion hook records the
original stack counters. Tests compare all1,024 output bytes, every affected
object field, normalized links, sentinel state, MOSAIC and update flags.

Coverage includes every311 image, four flip combinations, positive/negative
and default scales, rotations, all16 layers, owner/visibility/cache gates,
arbitrary active ordering, clipped first pieces, and128-entry/32-matrix limits.
The oracle produces45,603 OAM entries and10,054 affine objects. Ten native
guards verify atomic rejection. The standalone ASan+UBSan fixture runs4,536
frames and13,183 assertions, freeing the ROM before using copied assets and
math. The report `native-sprite-oam-validation.json` contains exact source and
fixture hashes. Sanitizer settings use `-no-pie`, `detect_leaks=0`.

The bounded decomp.wtf attempt for FC3C stopped after two manual compiles at
**83.9%, not exact**. `recovered/sprite_layers_reset.c` preserves the candidate.
Its loop instructions agree, but the pinned compiler adds a link-register
push/pop sequence where the original uses a leaf `bx lr`, shifting offsets.
The claim was released with this evidence and notes. This pass adds no exact
matching function; native whole-routine validation is separate from matching
progress.
