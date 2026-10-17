# Static map collision

`src/geometry.c` is a native behavioral port of map lookup and static collision.
It operates on all sixteen real maps decoded by `ssa_map_assets_load`; see
`analysis/map-assets.md`. No ROM instructions execute in the native library.

## Recovered routines

| Address | Native operation | Scope |
| --- | --- | --- |
| `0800B1B0` | `ssa_bsp_side` | Whole point classification routine |
| `0800B220`, `0800B388`, `0800B370` | `ssa_map_leaf` | Node field adapter, BSP traversal and point wrapper; replaces scratch stack with bounded traversal |
| `0800B2C8` | `ssa_map_sector` | Leaf-to-sector lookup |
| `08003B0C` | `ssa_line_side` | Whole wall-side routine |
| `08003B98` | `ssa_box_line_side` | Whole box-side routine |
| `08003C6C`, `08009C98` | internal wall visitor | Bounding checks, special contacts, solid walls and adjacent sector opening |
| `0800975C` | internal block traversal | Original cell order and once-per-query line visitation |
| `08003E5C` | `ssa_map_query` | Static-map query; dynamic entities are supplied by the separate world layer |
| `080043D0` | `ssa_map_try_move` | Query, clearance, step checks and position commit; entity index unlink/relink is not implemented |
| `0800B14C`, `08005F14` spawn path | `ssa_map_player_start` | First type-1 thing, heading and BSP floor/ceiling; position recovery only |

Positions and radius use signed 16.16. Sector floors/ceilings are signed integer
map units. Native structures contain host pointers and map-relative offsets;
they are not binary overlays of GBA RAM.

BSP node fields are origin X/Y at `+0/+2`, direction X/Y at `+4/+6`, child
indices at `+24/+26`. Child bit `0x8000` selects a leaf. The root is the last
node. The original helper first discards position fractions, then subtracts
integer origins, applies a sign shortcut and compares wrapped 32-bit products.
Wall-side tests instead keep position fractions and use signed fixed-point
products. These two paths must not be combined into one floating-point test.

The query expands the old-to-new position rectangle by radius, obtains the
destination sector through BSP lookup, then visits blockmap cells with X as
the outer loop and Y as the inner loop. The 128-unit cells contain ordered line
indices. A scratch visitation array replaces the original line counter field;
the map itself is not mutated by a query. Keeping the order preserves early
blocking, partial height results, and the special-line contact list.

For a two-sided line, the opening uses the lower ceiling, higher floor and
lower floor of its sectors. Contact with a solid line or an opening whose
ceiling equals its floor fails the query. The subsequent move routine checks
body clearance and the original 48-unit maximum step. Entity flag 4 has its
original special clearance behavior, including a downward velocity adjustment
on certain failures. A successful move updates X/Y and floor/ceiling, without
changing Z; the existing vertical physics updates Z afterward.

Player-start recovery follows `08005F14` calling `0800B14C` for thing type 1.
The 12-byte thing record stores signed X/Y at `+0/+2`, heading at `+4` and
type at `+6`. The original later sets spawn Z to the BSP sector floor. The
native `ssa_map_player_start` returns those values; it does not spawn an entity.
Player setup at `08005F62/08005F68` sets radius 32 and height 100. Level 0 starts
at (-4672, 2048), heading `0x4000`. All sixteen resources contain one type-1 start.

The earlier trace suggesting `080043D0` returns an entity pointer on failure
was incorrect: all failure paths reach `08004488`, which sets return value zero.
The collision-classification global is loaded on one path but overwritten.

## API and integration boundaries

Zero-initialize `SsaMapAssets` and `SsaMapCollision`, load a map, then call
`ssa_map_collision_init`. Keep the map alive and its layout/counts stable while
using the workspace. Call `ssa_map_collision_free` before freeing the map.
Sector height values may change; queries read them each time.

`ssa_map_query` and `ssa_map_try_move` return 1 for clear/success, 0 for blocked,
and -1 for invalid geometry. An invalid result must be treated as failure, not
as a truthy success. Negative radius is rejected. Native BSP traversal rejects
bad indices and cycles rather than reading arbitrary memory or hanging.
Queries expose ordered special-line contacts; they do not execute doors,
switches, damage, exit triggers or other game events.

This API remains the static portion. The separate world/horizontal/slide modules
now implement the original timestep, dynamic entity predicate and host spatial
lists, and three-corner wall sliding. See `analysis/world.md` and
`analysis/sliding.md`. `ssa_map_query_visit` provides a visitor after sector
initialization and before walls; the world uses it for original entity ordering.
`ssa_map_commit_move` applies clearance/commit to a successful query. The static
query alone still cannot block against enemies or other entities.

## Validation

Run `test-geometry` through the workspace driver. The test compiles native C
with warnings-as-errors and undefined-behavior checks and runs:

- 6,000 BSP side, 6,000 wall side and 6,000 box side comparisons, including
  negative coordinates, fractions, axis-aligned directions and integer limits.
- 15,659 leaf and 15,659 sector lookups on real maps: every vertex plus 500
  further positions per map, including positions outside blockmap bounds.
- 6,400 full map-query comparisons. Original code runs without call hooks;
  its dynamic entity grid is empty. Results, all floor/ceiling outputs,
  ceiling-limiting line and ordered special contacts match.
- 6,400 move comparisons against the full original routine. Only entity-index
  unlink/relink calls are hooked, and their order is checked. Complete entity
  bytes are compared, covering both changed and unchanged fields.
- 16 player-start checks against the original thing scanner and sector lookup.
- 16 new jump scenarios using a clear position and actual map heights on each
  map, and 16 walking-jump scenarios from the original player starts using the
  original radius/height. Held jump launches once, rises, stays within clearance
  and lands with the floor-contact flag. Blocked moves preserve X/Y.
- 112 guard checks: cycles, invalid node/leaf indices, negative radius, null
  body, workspace capacity mismatch and malformed blockmap size.

Total: 62,118 ROM comparisons, 16 player-start checks, 32 real-map jump scenarios
and 112 guard checks.
This proves the stated isolated scopes, not a running game, dynamic collision,
wall sliding or Windows input. Grounded downward velocity is retained from the
original gravity routine; landing does not imply velocity is exactly zero.
Evidence and source hashes are in `analysis/native-geometry-validation.json`.

The original BSP classifier matching candidate is at `recovered/bsp_side.c`.
It is 98.6% similar (score 75, 56 instructions), not byte-identical. Twenty
bounded attempts are recorded across the decomp.wtf database and
`analysis/bsp-match-attempts.json`; its claim was released with specific notes.
