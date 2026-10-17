# Multiplayer pickup respawn

`src/pickup_respawn.c` recovers complete `0801B854` and the occupancy helper
`08005330`. `src/pickup_respawn_runtime.c` binds them to the native shared entity
pool and world. The original frame routine `0800244C` calls this stage at
`08002494`, after entities, moving sectors and the level VM, before `08000908`
and the sprite/weapon controllers. This is behavioral recovery, not an exact
compiler match.

## Original behavior

The stage returns without changing counters when the player count is exactly
one, or the level is outside 12–15. GAME (`03004010`) holds the elapsed halfword
at +7E, schedule halfword index at +80 and marker index at +82. Elapsed time
adds the tick byte at +7 with 16-bit wrap. An attempt occurs only when elapsed
is **strictly greater** than the current schedule threshold.

The four schedules are 34 halfwords each, at `0844D204`, `0844D248`,
`0844D28C` and `0844D2D0`. They currently contain identical data: sixteen
threshold/item pairs followed by a zero sentinel. Thresholds are 300; item
animation IDs are 32,32,33,53,53,49,48,34,58,58,37,59,59,53,53,58. The native
loader copies this data from the user's ROM. It does not execute ROM code.

The marker array is the original D9 array at `03004040`, with count at
`03004014`. Actual maps 12–15 have 22,17,15,17 markers, respectively. Both
schedule and marker indices advance before the occupancy query. The schedule
wraps on its zero sentinel; the marker index wraps at its count. Elapsed time
resets after an attempt, including occupied markers and allocation exhaustion.

`05330` walks the active slot prefix in order. Any active entity whose whole
X and Y equal the marker blocks spawning. Fractional coordinates, entity type,
flags, height and Z do not matter. This is not a radius or collision query.

Successful allocation sets X/Y from the marker, type E1, state zero, facing
to the low marker-heading byte, trigger zero, timer 60 and flag 80. It calls
animation setter `05394` with the schedule ID and period 6, inserts via `A5D0`,
then sets Z from the signed inserted floor. Particle 5 is requested last.
Dimensions and health remain the allocator's zero defaults. The ordinary E1
timer later clears flag 80 and resets its timer to 30; it does not invent
pickup dimensions.

Allocation failure still requests particle 5. The original reads Z at address
8 through the null entity pointer. Native callers supply an explicit
`fallback_z`; this value is not claimed to be a defined original coordinate.

## Native ownership and integration

Initialize `SsaPickupRespawnRuntime` from zero with stable combat runtime,
spawn plan and loaded schedules. It borrows these objects and uses the combat
runtime's explicit reserved slot mapping and script-spawn pool. Run one step
after the level mover/VM step per world frame. The runtime takes player count
from combat and level number from the bound level runtime.

Occupancy rebuilds the original active order from the pool. Allocation uses
the real 100-slot pool; insertion uses native world lists and floor lookup.
Spawned E1 actors are owned by `SsaScriptSpawn`, including their animation
metadata and timer projection. Release them through `ssa_script_spawn_release`
when actual pickup/destruction processing requests removal. Freeing this adapter
does not remove its actors. Free the adapter before its dependencies.

Particle presentation is the only external operation. Its callback observes the
already-inserted entity and final floor Z, with runtime, combat, spawn and all
player-runtime locks held. Mutation, ownership changes and callback reentry are
prohibited. Earlier effects remain on failure: a failing particle callback leaves
the inserted pickup, advanced indices and elapsed value. Explicitly release any
unlinked allocation retained after an insertion error before another step.
World/pool storage and marker storage must remain stable.

## Evidence

`tests/test_pickup_respawn_rom.py` passes **73,734 original-routine comparisons**:
69,638 complete `1B854` cases plus 4,096 isolated `05330` cases. It covers all
65,536 elapsed values, gating, index wrap, occupied markers, allocation failure,
full game/entity snapshots and ordered callbacks. `05330` and `05394` execute
unmodified inside the whole-routine oracle; allocation, insertion and particles
are controlled boundaries. Address 8 is mapped only in the test oracle to make
the explicit fallback input observable. There are 186,977 callback reentry
checks, 16 input guards and five callback failure paths. See
`pickup-respawn-validation.json` for the ROM and source hashes.

`tests/test_pickup_respawn_runtime.py` passes **6,314 ASan/UBSan assertions**,
268 frames on all four multiplayer maps with two actual player bindings. It
visits all 71 original D9 markers and every schedule entry, creates 175 ordinary
pickups plus four retained presentation-failure pickups, observes 180 particles,
tests 144 timer steps, 69 slot reuses, four occupied attempts and full 100-slot
pool exhaustion. It checks 736 guards and four presentation failures. The
binary uses `-no-pie` and `ASAN_OPTIONS=detect_leaks=0`; hashes are in
`native-pickup-respawn-validation.json`.

The fixture releases pickups explicitly to test ownership and reuse. It does
not implement multiplayer packet delivery or particle rendering. This stage
does not by itself provide a playable native executable.
