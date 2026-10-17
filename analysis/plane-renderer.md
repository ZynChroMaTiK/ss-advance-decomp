# Native floor, ceiling and sky planes

`plane_renderer.c` recovers A844/A870/A888/A900 and the complete
AB10 -> AA4C -> A98C drawing path. It consumes the original plane coverage,
generates horizontal spans, and writes the actual120x120 indexed framebuffer
through native row fill and sky copy primitives. It does not invent textured
floors: a nonzero sector selector is an opaque palette index; zero selects
the original sky sampling path.

## Plane pool and wall integration

Each original plane record is exactly252bytes:

| Offset | Type | Meaning |
| --- | --- | --- |
| 00 | s16 | Plane height |
| 02 | s16 | Minimum covered X |
| 04 | s16 | Maximum covered X, inclusive |
| 06 | u16 | Color/sky selector; A98C consumes its low byte |
| 08 | u8[122] | Top edges, including sentinel columns |
| 82 | u8[122] | Bottom edges, including sentinel columns |

ColumnX is `top[X+1]` / `bottom[X+1]`. Thus original writes at+9+X
and+83+X correspond to the public `SsaPlane` arrays. Slots0and121 represent
columns-1and120. Valid coverage is within0..119; a top value255 denotes an
empty column. The inclusive bottom edge remains stored even for empty columns.

A844 allocates80records for level2 and64for every other game level. Native
`SsaPlanePool` contains stable storage for80 and selects the original capacity.
Its initialization clears native storage; original allocator contents were
unspecified. A870 rewinds the active-plane pointer and WORLD+340 opening cursor
to WORLD+70; the native reset clears `count` and `opening_cursor`, retaining
all plane bytes. The opening-buffer contents remain owned by world rendering.

A888 searches in active order for the first height/color match. Height is
truncated to16bits and input color to8bits. A new record sets minX120,maxX-1,
height/color and only the120 interior top bytes to255. Bottom bytes and both
top sentinels retain their previous values. `ssa_plane_find` preserves this.

A900 examines the intersection of an existing Xrange and a requested range.
If every overlapping top byte is255, it expands the existing min/max union
and returns the same handle. Otherwise it appends a new plane with the same
height/color, the requested range, and cleared interior top bytes. It does not
test bottom edges when deciding whether to split. `ssa_plane_check` returns a
stable `SsaPlane *`; full capacity returnsNULL without modifying the pool.
BA04's floor/ceiling handles and BE60's split results can therefore refer to
the same pool. Callers must propagate a returned replacement handle.

## Span generation and drawing

AB10 first calculates the sky scroll offset from the **display-local player's
heading at+B8**, not an independently supplied camera sine/cosine:

```
sky_offset = (uint16_heading * 5) >> 9;  // 0..639
```

For every nonempty plane it records the absolute difference between the signed
plane height and signed camera halfword0300402A (the high halfword of cameraZ).
It copies the plane selector's low byte to the current color, forces top
sentinels at minX-1 and maxX+1 to255, then visits columns minX through maxX+1.
The distance remains observable state even though A98C does not use it.

AA4C compares consecutive inclusive vertical intervals and performs four loops
in original order: close rows at the top, close rows at the bottom, open rows
at the top, open rows at the bottom. Closing a row emits its saved startX
through currentX-1; opening stores currentX in WORLD+344's row-start array.
This merges adjacent columns into horizontal spans without filling gaps.
Native `ssa_plane_edges` preserves ordered partial effects if a later span
fails validation. There are no external callbacks in the complete native path.

A98C skips rows greater than119. For a nonzero color it fills every byte of
the inclusive span, including palette index255. A zero color copies consecutive
sky bytes. These are native calls to `ssa_raster_fill_row` and
`ssa_raster_copy_sky`, corresponding to original ARM08000348 and08000358 copied
to030005AC and030005BC. Neither operation introduces transparency.

## Exact sky sampling and its bounds

The row-offset table at0844AAEC is exactly `row*768`; framebuffer offsets at
0844ACCC are exactly `row*120`. A98C samples:

```
source = family_base + row*768 + sky_offset + left;
count = right-left+1;
```

Family0 base is **0846285C** for levels0..5,12,13. Family1 base is
**0846DC5C** for levels6..11,14,15. The bases differ46,080bytes, equal to
768*60, although A98C itself accepts all120 rows. No image dimension header
or wrapping operation is read by this routine. It must not be replaced with
an assumed640wide modulo sampler or a fabricated lower sky half.

Direct data inspection additionally confirms that each of the first60 rows
contains640 panorama columns followed by an exact copy of its first128 columns.
That padding lets a120pixel span read across the scroll boundary without an
explicit modulo operation. Local previews `sky-family0-top60.png` and
`sky-family1-top60.png` under ignored `build/native-tests/` were visually checked:
they contain the original pyramids/obelisk and green ruins horizons.

`ssa_sky_assets_load` copies two **overlapping92,160byte sampling windows**
from the authorized ROM. This is enough for every accepted row, heading and
viewportX. It deliberately preserves the original reads even when a zero
selector causes a lower-half row to access bytes beyond the apparent60row
panorama. These windows are not claimed to be two independent120row images.
The intended semantic extent of adjacent data remains unresolved. The native
loader and renderer execute no ROM instructions; Unicorn appears only in tests.

## Native contracts

Initialize a stable `SsaPlanePool` once per level, reset it once before each
world visibility pass, then use find/check and populate top/bottom coverage.
Draw planes after walls, matching original B6B0's ordering. Keep the pool at
the same address while wall records retain handles. `SsaPlaneRenderer` contains
the120 row starts, sky offset, distance, current color and a render guard.
Zero-initialize it and `SsaSkyAssets`. Sky storage is embedded and has no heap
allocation; after successful loading the caller may release the ROM buffer.

`ssa_plane_render` requires a120x120 framebuffer and validates queue bounds,
coverage and required sky data before changing state. Empty planes are skipped.
It mutates original sentinel bytes and retains row-start/current-state results.
Rendering returns1/-1; errors do not roll back previous effects and the guard
is cleared. Direct span/edge calls reject nested use. Storage cannot be resized,
freed or aliased with framebuffer output during calls.

Native guards reject negative/out-of-viewport drawing spans, reversed spans,
invalid handles and exhausted queues. Coverage bottoms must be within0..119;
tops must be0..119 or255. This supported domain matches coverage produced by
the native wall renderer. Malformed byte intervals could make original AA4C
index unrelated WORLD globals; the native code does not reproduce that memory
corruption. Original zero-length raster loops likewise are not permitted.

## Validation and matching

`tests/test_plane_renderer_rom.py` executes complete original routines with
actual ARM fills/copies installed at their original IWRAM addresses. There are
no controlled rendering callbacks. Only A844's allocator is replaced. Results:

| Original routine | Comparisons |
| --- | --- |
| A844 allocation size | 16 |
| A870 reset | 256 |
| A888 find/create | 6,144 |
| A900 reuse/split | 4,096 |
| A98C direct span pixels | 8,195 |
| AA4C edge transitions and pixels | 4,096 |
| Complete AB10 queue, spans and pixels | 1,152 |

That is **23,955 whole-routine comparisons**. The AB10 cases include128 cases
using actual sector heights and floor/ceiling selectors from all16 maps, with
explicitly controlled column coverage. They do not claim BSP-derived visibility
or complete game scenes. All mutated plane bytes, row starts, globals and every
framebuffer byte are compared. Sky cases include both families, lower-half
reads, maximal offsets and all valid levels. An ASan+UBSan fixture adds
**230,583 assertions**, including ROM-buffer independence and full-width sky
and flat-plane output. Additional guards and prefix-failure counts are recorded
in `native-plane-renderer-validation.json` with source hashes.

Using the requested decomp.wtf engine, **A870 matched exactly:24bytes and
12instructions**, on the second manual compile. `recovered/plane_reset.c`
contains that source. The engine released the matched claim automatically.
Native behavior recovery for the larger routines is separate from byte-identical
matching. Plane pixels alone do not provide a playable native executable;
wall projection, BSP orchestration and frontend integration remain separate.
