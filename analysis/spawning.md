# Native map entity construction

`src/spawn.c` ports the complete map-thing construction routine at `08001984`.
It loads the actual 64-entry translation table from the user's ROM, constructs
stable native objects, records multiplayer/teleport starts and inserts the
objects into the native world in the original order. This is the initial
construction phase, not complete enemy AI activation, pickup ticking or player
setup. Pickups legitimately retain zero collision dimensions; see the runtime
dispatch trace below.

## Recovered behavior

The game calls `08005238` to initialize a 100-slot entity pool, then `08001984`
to enumerate map things. Type is the halfword at thing offset +6. Enumeration
loops types 0 through 63, then scans matching things in file order using
`0800B14C`. Consequently file order alone is not entity allocation order.
Types above 63 are ignored; zero-valued table entries are still instantiated
if a matching map thing exists. Pool exhaustion drops subsequent ordinary
objects, but continues collecting start markers.

The table begins at ROM offset `44A3D4`. Each eight-byte entry contains four
halfwords: entity type, variant, animation ID, animation tick period. Native
loading keeps this data in the supplied ROM rather than embedding asset data.
Map types 26 through 63 translate to entity type `0xE1`; their animation ID is
also the ID consumed by the pickup handler at `0801B99C`.

Construction initializes the original fields as follows:

| Original entity field | Initialization |
| --- | --- |
| +00/+04 | Signed thing X/Y converted to 16.16 |
| +08 | BSP sector floor converted to 16.16 after insertion |
| +3C | Translation-table entity type |
| +3E | Low byte of table variant |
| +3F | `(thing heading >> 8) + 128`, truncated to a byte |
| +58 | Animation ID via `08005394`; also the pickup item ID |
| +5C/+5D/+5F | Animation counters reset to zero |
| +5E | Low byte of animation tick period |
| +68 | Thing halfword at +8; used by trigger processing |

The original animation setter accepts IDs 0 through `0x8D` and `0xFFFF`;
other IDs leave the initially zero animation fields unchanged. Thing +A is
not consumed by this routine. Native metadata also retains the source thing
index for later recovery/debugging.

Original level index 10 contains a hard-coded coordinate correction:
objects at X=2240 or X=3008, Y=-2304 instead receive Y=-2108. This correction
applies to ordinary objects before BSP insertion, not start markers.

Entity types `0xC9` through `0xCC` become player-start records, and `0xD9`
becomes a teleport-start record. Records retain signed whole-unit X/Y, zero Z
and the upper heading byte. Single-player initialization skips the `0xC9`
pass entirely, since `08005E4C` later locates thing type 1 separately. Other
player marker types are still collected. Full player state, player-body
construction, weapon inventory and respawn logic are separate work.

The original insertion routines `0800A5D0` and `0800A6CC` resolve the BSP leaf,
sector, floor/ceiling and linked block-cell membership. The native world
provides those operations with host pointers. Entity pool slot/active indices
are replaced by stable native storage. At this exact stage, radius, height,
flags and health remain zero. Later enemy AI updates are responsible for enemy
activation. Pickups retain zero dimensions in their normal update path;
assigning plausible dimensions here would change the recovered behavior.

## Ownership and use

Load `SsaSpawnDefinitions` with `ssa_spawn_definitions_load`. Zero-initialize
`SsaSpawnPlan`, then call `ssa_spawn_plan_load` with an already validated map,
the level index, player count (1 through 4), and available original pool slots
(0 through 100). The plan owns its object and marker arrays; its map remains
caller-owned. A successful plan load leaves its objects unregistered, with
positions and floor/ceiling already computed.

`ssa_spawn_plan_insert` registers objects in original allocation order. The
world must reference the same map and have enough remaining capacity. It
refreshes each floor/ceiling and places Z on the current floor. Failed insertion
removes any objects inserted by that call. It does not roll back body metadata.
Do not modify registered coordinates directly or move/reallocate object storage.

`ssa_spawn_plan_free` refuses while any object remains registered. Free/detach
the world first, then free the plan, then the map. Never free plan storage from
inside a world-query callback, including after removing a contacted pickup.
This layer does not execute AI, pickup effects, damage, animation stepping or
graphics. Spawned enemy bodies still need activation before normal gameplay.

## Validation

Run `.venv-linux/bin/python tests/test_spawn_rom.py` inside Ubuntu/WSL.
The native build uses warnings-as-errors and UBSan. The test executes the entire
original `08001984` routine with its real allocator, thing scanner, animation
setter, BSP resolution and block-cell insertion. Only sector-pool allocation
at `0800D1B4` is replaced; native host sector lists are a separate representation.

- 256 complete initialization comparisons: all 16 maps, both one/four-player
  settings, four pool capacities, plus 128 synthetic thing arrays on real BSP
  geometry. Synthetic cases cover full-range coordinates, headings/triggers,
  ignored types and both level-10 coordinate fixes.
- 3,128 complete original 116-byte entity records compared with projected
  native state, including original pool indices and native-to-GBA link mapping.
- 728 start-marker records and 256 complete block-cell-head snapshots compared.
- 370 native guards cover registered-storage lifetime, duplicate insertion,
  capacity, invalid settings, malformed sections and partial-insert cleanup.

Evidence is in `analysis/native-spawn-validation.json`. These are behavioral
comparisons; no byte-identical recovered C match is claimed for `08001984`.

## Runtime dispatch trace and next recovery targets

This section records read-only instruction/table inspection, not newly ported
or differentially validated behavior. Frame routine `0800244C` calls entity
dispatcher `08004E04` at `08002488`. The dispatcher walks the active index list
at `03004470`, resolves 116-byte entity slots through `[03004644]`, writes the
current entity to `[0300443C]`, then indexes the 128-entry branch table at
`08004E68` with `(entity.type - 100) * 4`. Types outside 100..227 use its default
path. It calls `080083E4` before visiting entities. Do not replace this frame
dispatcher with arbitrary file-order updates: both its ordering and mutations
of the active list still require full recovery.

The exact type targets selected by this table are:

| Entity type | Per-entity routine | State-zero initializer |
| --- | --- | --- |
| C9..CC | `08005CCC` | Separate player initializer `08005E4C` |
| CD | `08013978` | `08013940` |
| CE | `080135C4` | `08013584` |
| CF | `08013040` | `08013008` |
| D0 | `0801319C` | `0801315C` |
| D1 | `08013B40` | `08013B08` |
| D2 | `08013350` | `08013318` |
| D3 | `08013FA4` | `08013F48` |
| D4 | `080141E8` | `08014158` |
| D5 | `08013DB0` | `08013D78` |
| D6 | `08013780` | `08013748` |
| D7 | `08014AB8` | `0801485C` |
| D8 | `08014648` | `080143B0` |
| D9..E0 | Default, no per-entity call | None in this dispatcher |
| E1 | `0801B848` | **No dimension initializer** |
| E2 | `080113E4` | Not traced |
| E3 | `08011424` | Not traced |

`0xE1` selects branch `08005118`, which calls `0801B848`. That wrapper only calls
`08012CFC` and returns. The helper updates the halfword countdown at entity +4E
by subtracting the game tick byte `[03004010]->+7`, preserving 16-bit wrapping.
If entity flag `0x80` is clear and the signed countdown expires, it sets the
countdown to zero and returns 1. With flag `0x80` set, expiration clears that
flag, resets the countdown to 30 and returns 0; positive countdowns also return
0. The pickup wrapper ignores the return value. Neither routine assigns
radius, height or health. Thus ordinary map pickups retaining zero dimensions
is intentional original behavior, consistent with `08003D7C` dispatching type
E1 before radius checks. Do not give pickups guessed enemy-style dimensions.

The separate multiplayer pickup respawner is `0801B854`, called by the frame
at `08002494`. It uses the D9 marker array at `03004040`, has level-specific
tables for levels 12..15, allocates an E1 entity, sets countdown 60 and flag
`0x80`, sets its animation/item ID, inserts it and places it on the floor.
This is a later respawn path, not `0801BF14`, which sets up sector scripts.

Enemy initializers use the same countdown helper, then advance byte +3E from
state 0 to state 1 and assign collision dimensions/health. Concrete examples:
type CD's `08013940` sets height 120, radius 32, health 30, and ORs flag 2;
type CF's `08013008` sets height 152, radius 48, health 50, and ORs flag 2;
type D1's `08013B08` sets height 140, radius 50, health 100, and ORs flag 2.
Dimensions are 16.16. These three routines call `080125BC`, which increments a
per-enemy-type counter in game state at `+0x130 + (type-CD)*12`. Boss type D8's
`080143B0` additionally sets boss globals, calls `08014328` and emits music/UI
effects; it is not safely represented by only assigning three constants.

Player initialization is already directly reachable after map spawning:
level setup calls `08001984` at `0800206E`, `08000890` at `08002072`, then
`08005E4C` at `08002076`. `08005E4C` allocates/clears four 256-byte player
records, selects single-player type-1 starts or multiplayer marker positions,
allocates player bodies, assigns type C9+index, index byte +54, radius 32,
height 100, flags 0x1A (0x9A for the local player), health 100 and animation
0x65/period 4. It then inserts each body and copies floor Z to both body and
player state. Player eye offset +D0 starts at 96 units, flag +BC includes 4.
Full weapon/inventory setup happens separately; copying this body initializer
alone is insufficient to initialize the complete player.

Player frame routine `08005CCC` calls death/respawn logic `08006154`, movement
`080064B8`, camera `080062D4`, local-camera-state copying, interaction/hazard
wrapper `08005680`, and sector trigger `0801C624`, followed by timer and
movement-animation handling. These paths are now connected in `player_runtime.c`,
`player_gameplay.c` and `level_runtime.c`; see their analysis documents. There is
no hidden common pickup activation assigning nonzero dimensions.

All twelve CD..D8 state-zero initializers, including both boss paths, are now
native and verified in `enemy_activation.c`; see `enemy-activation.md` for exact
effects and validation. `script_spawn.c` connects script creation to the original
100-slot pool, native world insertion, activation and explicit group notification.
See `script-spawn.md` and `entity-pool.md`. Type-CD control flow, shared navigation
and death handling now have native implementations; see `enemy-cd.md`. Original
sight/nearest/ray queries, other enemy controllers and the outer dispatcher remain.
