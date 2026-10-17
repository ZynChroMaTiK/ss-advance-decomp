# BSP visibility and ordered wall commands

`src/world_visibility.c` recovers the original world-list construction stage.
It produces actual front-to-back wall commands from the decoded map, camera,
live sector heights and side records. It does not draw pixels or submit actors.
`ssa_world_visibility_build` ends at the three original drawing boundaries:
`017CC`, `D2D0`, and `01828`. Native wall/plane rendering is a separate module.

| Original | Native behavior |
| --- | --- |
| `0800B574` | Reset visibility bitsets, horizontal clipping, wall queue and plane pool |
| `0800B6B0` | Front-first BSP traversal, far-child bounding-box culling, sector reject |
| `0800B220` / `0800B1B0` | Signed node-vector projection and original BSP side test |
| `0800B428` / `08000CEC` | Bounding-box angular frustum and solid-span rejection |
| `0800B7B0` | Mark visited sector and traverse leaf segments |
| `0800B818` | Segment backface, cached vertex angles, angular clipping and screen endpoints |
| `0800BA04` | Plane selection, portal/solid classification, perspective endpoints and wall enqueue |
| `08000AC8` / `08000C10` | Inclusive visible ranges; solid insertion/merging versus portal clipping |
| `08000F44` / `08000F78` / `08000FB8` | Reset, allocate and append the original wall queue |

The isolated helpers use existing native `ssa_bsp_side`, `ssa_sector_reject`,
`ssa_sincos`, and `ssa_plane_find`. The 32-bit vertex-angle calculation follows
`093E8` without truncating the difference between two signed 16-bit coordinates.
`BA04` uses the original tangent and reciprocal tables, signed 64-bit dot product,
fixed-point products and 32-bit wrapping texture coordinates. These table copies
are data, not executed instructions.

## Important original behavior

- The camera's sector is original `03004020+18`, distinct from heading at `+0C`.
  Coordinates use their signed high halfwords for vertex and bbox projection.
- Traversal appends walls in BSP visitation order. There is no distance sort.
- Projected screen endpoints can be `0..120`. Clipping ranges are inclusive and
  end at 119 because the solid list starts with offscreen sentinel intervals
  `[-32767,-1]` and `[120,32767]`. Bounding boxes query `right-1`.
- `AC8` compaction copies the word at its old exclusive end pointer too. This
  leaves an extra trailing entry beyond the upper sentinel. Native storage
  preserves that behavior; interval traversal stops at the covering sentinel.
- Plane selection happens before wall visibility-range rejection. A closed
  front sector (equal floor and ceiling) preserves the previous cached plane
  handles. `B574` resets plane count but does not clear those cached handles.
- Side `+6` is the middle texture index. Equal-height portals with an absent
  middle texture and equal packed floor/ceiling color bytes are omitted.
- Segment `+0A` supplies horizontal texture origin; zero falls back to side
  `+0`. Line flag `0x200` adds the low game-counter byte to that origin.
- Wall allocation and clipping occur before the `0x80000` endpoint-depth
  rejection. A rejected wall can consume a slot and ranges without entering
  the linked list. Follow `head/next`, and validate indices against `wall_count`.
- `high_water` records the maximum **previous** frame's consumed wall count
  during reset. It is not the current frame's allocation extent.
- Original `B574` also resets renderer column clips and texture-cache IDs.
  Those belong to the drawing context, not this visibility projection; the
  world-scene adapter resets the columns before constructing this list.

## Ownership and bounds

Zero-initialize `SsaWorldVisibility`, then initialize it with borrowed decoded
map, math tables, visibility tables and a stable-address plane pool. The
workspace owns its vertex-angle cache, bitsets and traversal stack. Its wall,
range and solid arrays remain stable until free. Map layout and count metadata
must stay fixed; sector heights, line flags and side texture IDs are read live.
Use one workspace per simultaneous view. Do not modify or release its resources
during a build, and do not render through its plane pool concurrently.

Successful operations return 1, except clip returns the number of new ranges
and box/segment return 0 when rejected. Invalid input returns -1. Later failures
retain earlier effects, clear the update guard and do not silently complete.
The original 100-wall limit is enforced: original `F78` returns `FFFF`, which
`BA04` would use as an unchecked record index. Native code fails instead. Plane,
range and solid-array exhaustion likewise return errors. Invalid node cycles,
section bounds, leaf/segment/side/sector references and output pointers are
guarded. Free before freeing the borrowed map or lookup tables.

## Evidence

Run `tests/test_world_visibility_rom.py` with the test environment. It compiles
the native implementation and runs complete original `B6B0` list construction
on all 16 maps, including original `B574/B428/B7B0/B818/BA04`, clipping, plane
allocation, trig and ARM fixed-point helpers. The only computational hooks are
GBA BIOS division/copy and DMA semantics. The subsequent drawing calls are
explicitly intercepted and their ordered addresses are checked.

Current results:

- 2,048 complete original list constructions; 1,024 use changed live sector
  heights, including equal floor/ceiling cases.
- 175,904 whole `B818` calls, 8,192 `B428` calls and 32,768 `AC8/C10` calls.
- 21,723 consumed wall records and 22,028 emitted ranges compared, alongside
  links, bitsets, cached angles, plane records and shared front/back scratch.
- 206,121 native ASan/UBSan assertions across 4,096 additional frames on all
  16 maps, with 485 malformed-state, reentry and capacity checks. Includes
  explicit 101st-wall failure and partial plane-allocation failure.

Sanitizer binaries use `-no-pie` and `ASAN_OPTIONS=detect_leaks=0`. Exact file
hashes and metrics are in `native-world-visibility-validation.json`. Unicorn
executes original instructions only in these tests, never in the native port.

The requested decomp.wtf matcher also verified `B220` byte-identically on the
first manual compile: 42 bytes / 21 instructions. Source is
`recovered/visibility_side.c`; `scripts/match_visibility_side.py` uses the real
engine/agent matcher, and `visibility-side-match-attempts.json` records the
result. The automatically released claim has recovery notes. The rest of this
native implementation is behaviorally recovered, not claimed as an exact match.
