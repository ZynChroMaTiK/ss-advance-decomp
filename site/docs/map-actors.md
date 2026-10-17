# Original map object ownership

`src/map_actors.c` composes the recovered01984 construction phase, native entity
pool/world insertion and00908 animation clock. It allocates stable objects in
the original type/file order before player construction. All16 maps contain385
ordinary objects and83 start/teleport markers; the largest object count is67.
No artificial truncation is needed to leave room for the local player.

The owner borrows its world, map and animation tables and owns its spawn plan,
pool and animation states. It rejects a nonempty world, active queries and
partial prior ownership. Free detaches only its own objects before releasing
storage, preserving separately owned player bodies. Free this owner before its
world/map/tables. Getter pointers are stable until free. Current display frames
come from the owned animation states, not the initial spawn metadata.

Map objects retain the original construction phase's zero dimensions, health
and flags. The viewer uses them as stationary visual objects and advances their
original animation clock. This owner does not activate enemies, run AI, attacks,
death, pickup dispatch or scripts. Those gameplay stages must be connected to
the actual canonical runtime during full game integration.

`test-map-actors` compares16 complete original01984 runs,385 entities,83 markers
and16 grids, then2048 whole00908 frames and49280 animation records. Its119
native guards cover malformed state. The ASan/UBSan fixture adds5404 assertions
on all16 maps, including2048 animation frames and16 real separately spawned
players that remain the sole registered world body after owner teardown.
Evidence and source hashes: `native-map-actors-validation.json`.
