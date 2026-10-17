# Native player construction

`src/player_spawn.c` implements the complete player construction phase at
`08005E4C`. It fills four stable caller-owned `SsaPlayerSpawn` records and
registers active player bodies in the native world. Each record composes the
shared `SsaPlayerMotion`, vertical state and inventory structures. This phase
does not grant a starting weapon or initialize its animation machinery.

The original allocates and clears four 256-byte player records. Single-player
finds the first map thing of type 1 using `0800B14C`. Multiplayer uses consecutive
eight-byte records in the marker array produced by map-object construction;
their order is significant, and their stored heading is shifted left by eight
bits. A single-player map without a type-1 start leaves every player record zero
and allocates no body. The original nevertheless publishes its requested player
count; native callers already hold that count and can inspect `active`.

For each existing start, initialization sets these recovered values:

| Field | Initial value |
| --- | --- |
| Player movement/vertical flags | 4 |
| Player eye offset | 96 units |
| Entity type/index | C9 + player index / player index |
| Entity radius/height | 32 / 100 units |
| Entity flags | 1A, or 9A for the local player |
| Entity health | 100 |
| Animation/period | 65 / 4 |
| Facing byte | Upper byte of player heading |

Original helper `08005E04` copies position and velocity from the player to its
body and derives the facing byte. After spatial insertion, construction replaces
both player and body Z with the resolved sector floor. Multiplayer marker Z is
therefore transient. Inventory health stays zero here; body health is a distinct
field. Weapon setup and subsequent synchronization must not be inferred from
the body-health initializer.

The native API uses `SsaPlayerMotion` directly for later movement integration.
Its `movement.flags` is the authoritative shared player flag field, with vertical
flags initialized consistently. The `entity_health` and animation fields retain
values that `SsaEntity` does not yet represent. Remaining original player fields
are cleared but have no additional nonzero behavior in this routine. Renderer,
weapon animation and respawn state still require their own recovered interfaces.

## Ownership and invalid inputs

Pass four zero-initialized records, an initialized `SsaWorld`, and a spawn plan
referencing the exact same validated map to `ssa_players_spawn`. Keep their
addresses stable while any body is registered. Detach the bodies or free the
world before reinitializing or freeing the records. Player count is 1 through 4;
the local index must be within it. The caller owns the requested count and local
index; the original global writes are not duplicated as hidden native globals.

The original fails unsafely if its entity pool is exhausted, and reads past the
populated marker array when multiplayer starts are absent. Native code rejects
insufficient world capacity or marker counts before changing outputs. Registered
output records, active collision queries, malformed thing spans and mismatched
maps are also rejected. Unexpected geometry failure removes any newly inserted
bodies and clears the output records. No placeholder bodies are introduced.

## Validation

`tests/test_player_spawn_rom.py` runs with strict compiler warnings and UBSan.
It executes the entire original `08005E4C`, including its thing scanner, entity
allocator, position synchronization, animation setter, BSP lookup and grid
insertion. Test boundaries substitute heap allocation, the two hardware/BIOS
zero-fill paths, and original sector-pool allocation. Native sector lists use
host pointers; those allocator internals are not claimed as recovered.

The suite passes 688 complete original-routine comparisons across all 16 maps:
each player count from 1 through 4 and every local index, 512 additional random
multiplayer-marker cases, and 16 missing single-player-start cases. Maps lacking
enough multiplayer starts receive explicit synthetic markers for the multiplayer
oracle cases; they are not claimed to support multiplayer unchanged. Marker
coordinates/Z cover signed 16-bit values and heading covers all 16 bits.

It compares all four complete original player records each run (2,752 records),
all 2,000 allocated 116-byte entity records, and 688 full grid-head snapshots.
All original padding, unused fields and pointer links are included in the
projected-state comparison. A further 679 native guards cover registered output,
invalid player/local settings, missing multiplayer starts, full world capacity,
query reentry and geometry failure. Evidence is recorded in
`analysis/native-player-spawn-validation.json`.

This is a native construction phase, not a playable game initializer, and no
byte-identical C match is claimed for `08005E4C`.
