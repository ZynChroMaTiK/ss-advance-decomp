# Native entity collision and horizontal movement

`src/world.c`, `src/horizontal.c` and `src/slide.c` now connect static maps,
dynamic entity collision, spatial links, the original horizontal timestep and
wall sliding. The APIs operate on native host structures and execute no ROM
instructions. The test oracle alone runs isolated original instructions.

## Entity collision

`ssa_entity_contact` recovers the entire predicate at `08003D7C`. It skips the
same entity, certain player/player and flag combinations, and the original
projectile-owner exemption. Other entities use square X/Y radius overlap;
exactly touching the summed radius is clear. Differences, absolute values and
radius sums retain the original 32-bit wrapping and unsigned comparisons.

Two important original behaviors are preserved:

- An encountered type-`0xE1` entity invokes `0801B99C` before distance tests.
  The native predicate instead returns a special-contact result. The world
  exposes those entities in encounter order and optionally calls a synchronous
  executor. The native pickup dispatcher can be connected through this callback;
  see `tests/test_pickup_world.c` for the host adapter and `ssa_pickup_touch`.
- The flag-`0x200` height test loads the moving entity's **X**, not Z, as its
  vertical coordinate. This is what the actual instructions do. It remains
  unchanged in the native compatibility path. Ordinary player collision does
  not acquire a new vertical-overlap exemption just because jumping is enabled.

`ssa_world_query` obtains the destination BSP sector and initializes floor and
ceiling, then traverses entity cells before visiting walls. Entity cells use
the swept radius box extended by 32 units, with X outer/Y inner ordering and
head-to-tail traversal. A blocking entity stops the query before wall contacts.
Its pointer is exposed as `blocking_entity`; static wall blockages leave that
pointer null. The hard-coded 32-unit broadphase extension is original behavior,
not a new promise to find arbitrary giant-radius entities in neighboring cells.

## Spatial ownership and move commits

`SsaWorld` owns cell heads, sector heads, query scratch storage and a bounded
special-contact array. The caller owns each `SsaEntity` and the loaded map.
Initialize structs to zero, retain stable entity storage and stable map layout,
and call `ssa_world_free` before freeing the map. Freeing a world detaches its
entities without freeing their storage.

Insert resolves BSP leaf/sector, initializes floor/ceiling, clears step adjustment
and pushes the entity onto its cell and sector lists. Out-of-grid entities still
have BSP/sector membership. Remove repairs neighboring links and heads. The
native API clears removed links and rejects duplicate insertion/removal;
the original leaves some detached fields stale. Sector pool allocation handles
are replaced with host pointers; the original allocator's internal bookkeeping
is not ported by this layer.

Use `ssa_world_try_move` or `ssa_world_horizontal_step` to change registered X/Y.
Writing those fields directly would invalidate spatial membership. A successful
move uses the recovered clearance rules, commits coordinates and heights, then
relinks at the destination. Failure preserves position, with the original
flag-4 downward velocity adjustment where applicable. Queries return 1 for
clear/success, 0 for blocked and -1 for invalid integration/geometry. Treat -1
as an error, not truthy success. Structural world mutation invalidates pending
contact references; queries overwrite result lists. Do not reenter a traversal.
During a query, insertion, movement, nested queries and reinitialization are
rejected, and `ssa_world_free` is a no-op. Removal is permitted only for the
special currently being executed. These guards do not make direct edits to
the public structure safe; callback owners must still honor the contract.

The optional `observe_special` callback reports every encounter, including
contacts from internal horizontal/slide retries. It must not mutate the world
or entities. Use it to collect a complete frame's events; `special_contacts`
alone retains only the latest query. Whole-frame observer order is compared
with the original handler call order in the integration tests.

The optional `execute_special` callback runs immediately after observation,
including during horizontal/slide retries. It receives the moving entity and a
mutable special, returns zero on success or negative on error, and can change
external inventory or remove **only that special** with `ssa_world_remove`.
It must retain the removed object's storage until the query ends and cannot
insert, move, free, reenter or mutate other entities. `special_contacts` may
therefore include detached entities. Errors release query guards and propagate
as -1 without rolling back earlier inventory changes or removals.

Original cell traversal `08009870` loads the current entity's next link **after**
the gameplay callback. Pickup removal eventually zeroes its storage, so removing
a special ends traversal of that cell, even if other entities remain there.
Traversal continues with the next cell. The native path explicitly preserves
this behavior; it must not cache the next link before dispatch. The cycle guard
uses fixed capacity, not the shrinking registered count. A refused pickup keeps
its links and traversal continues normally.

The world does not itself allocate inventory or bind pickups to entity metadata.
The small integration adapter in `tests/test_pickup_world.c` connects these
pieces explicitly. Script execution, sounds, HUD messages and network transport
remain callback boundaries, and damage/sector actions/doors need recovery.

## Horizontal timestep and wall sliding

`ssa_horizontal_step` implements `08004BDC` with move/classification/slide
callbacks; `ssa_world_horizontal_step` supplies the native world and recovered
slide implementation. Give it `SsaSlide` initialized for the same map and math
tables loaded from the user's ROM.

Both velocity components clamp to +/-30 units per tick. Zero ticks still clamp
velocity. Remote multiplayer player bodies are skipped. The initial clamped
velocity remains the position increment across the entire tick batch, even if
later contacts change stored velocity. Blocking entities cause Y-only then
X-only attempts. Wall collisions invoke the original three-corner sliding path
when flag 8 permits sliding. Without that flag, failed motion clears velocity.
See `analysis/sliding.md` for trace/projection details and original quirks.

The native callback contract propagates -1 for invalid moves, classification
or sliding. Errors stop further movement and may follow earlier committed
substeps; there is no transaction rollback.

An integration frame runs horizontal movement, vertical/jump simulation,
`ssa_camera_step`, then `ssa_view_pose`. Give the H-toggle camera its own query
workspace so camera obstruction checks cannot overwrite pending gameplay
contacts. These components form movement integration, not a complete game loop.
Entity spawning/AI, combat, rendering, texture/sprite bindings, audio and the OS
keyboard/mouse backend are still absent.

## Validation

`test-world` compares against original routines on all sixteen real maps:

- 14,000 entity predicate cases, including wrapped/extreme arithmetic and flag
  combinations. Only the `0801B99C` special handler is replaced by a recorder.
- 4,800 whole movement queries and 4,800 whole clearance/move routines. Original
  grid insertion/removal and BSP execute; only sector-pool insertion/removal
  (`0800D1B4`/`0800D214`) and the special handler are recorded.
- 2,048 complete horizontal updates with real entity grids, original wall
  traces/sliding and native equivalents. Original fixed math executes; the BIOS
  integer-division wrapper is serviced by the oracle.
- 848 cell/sector list snapshots, including head/middle/tail removal, destination
  relinking and out-of-grid entries. Original grid pointers/order match;
  native sector membership and doubly linked invariants are checked separately.
- 2,048 native frames combining horizontal motion, jumping, original eye updates
  and H toggles. Camera queries preserve gameplay query results.
- 400 native ownership/removal guard checks.
- 1,024 whole original queries with a controlled special handler that unlinks
  and zeroes selected entities. Across all sixteen maps, head/middle/tail
  removals and retained pickups exercise traversal in multiple cells. Original
  `03E5C`, `09870` and `03D7C` execute. This establishes callback mutation order,
  not the original pool allocator, DMA clearing or pickup effects.
- 144 native synchronous callback guards cover reentry, forbidden mutation,
  error propagation and guard cleanup after partial effects.
- 64 native pickup/world queries on all maps connect the recovered dispatcher
  and inventory. Full-health refusal continues to a weapon pickup; accepting
  a pickup removes it and ends its cell; later queries consume retained health
  pickups once. Observer, dispatch, sound/message and removal order are checked.

An additional 2,048 complete `080064B8` updates now execute controls, multiplayer
push, body synchronization, real horizontal/sliding physics and vertical physics
on all sixteen maps. Full player bytes, body fields, contacts and spatial links
match. See `player-motion.md` for its generic-boundary suite and native options.

That is 28,720 ROM comparisons plus spatial/integration/guard checks.
`test-horizontal` separately verifies 16,760 timestep comparisons with controlled
collision callbacks, complete entity bytes and ordered calls, plus 12 API guards.
`test-slide` adds 29,400 ROM comparisons and 64 guards. Combined new ROM
comparisons: **74,880**. Native tests compile with warnings-as-errors and UBSan.
Evidence is in `analysis/native-world-validation.json`,
`analysis/native-horizontal-validation.json` and
`analysis/native-sliding-validation.json`.

The validated callback at `08003D7C` was added to decomp.wtf through its supported
function discovery tool: 224 bytes, ending exactly at `08003E5C`. Its matching
candidate is 92.0% similar (score 880), not exact, after twenty bounded trials.
The catalogue now has 526 functions; 63 exact functions cover 1,616 of 87,648
catalogued code bytes (1.84%), including the two player/body copy routines.
The catalogue is still incomplete.
