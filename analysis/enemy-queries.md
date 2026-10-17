# Native enemy query integration

`src/enemy_queries.c` supplies the three query callbacks used by the native
type-CD controller: sector rejection (`B18C`), nearest eligible enemy (`036C4`)
and the dual navigation rays (`1271C` with `A240`/`A041`). Bind its `ops` to
`SsaEnemyCdRuntimeOps.queries`. The controller still supplies recovered turning,
world movement, gravity and hazards. This completes these previously controlled
query boundaries without changing the controller API.

Initialize after script-spawn initialization with an explicit array of the
externally owned entities in original slot order. The adapter copies that array
and validates unique registered bodies. It must not infer this order from
spatial lists: nearest selection preserves the first exact-distance tie, and
release swaps the last active pool slot into the released slot's position.
Owned scripted actors are resolved from current active slots on every query.
Externally owned entities must remain registered and stable while bound.

Keep the adapter at a stable address because its ops context points to itself.
Free it before script-spawn/world teardown. Queries are allowed while the enemy
controller holds `spawn.updating`; initialization and teardown are not. World,
level, mover, VM and nested query reentry are rejected. No callbacks are exposed
inside the query pipeline. Outputs on successful path queries include the
original offset vector and mask; prior path effects remain if a later ray fails.

The ray owns separate visited-line and intercept buffers. It never writes the
world's last blocking entity or movement/camera scratch. Sector floor/ceiling
heights are read live, while map layout remains fixed. Capacity is one intercept
per unique map line; this avoids reproducing an unsafe original fixed buffer
overflow. The original 64-cell traversal cap is retained. The ray handles walls
only, matching the navigation call's flags=1; it does not supply weapon/entity
tracing. See `enemy-ray.md`, `ray-traversal.md` and `ray-intercepts.md`.

## Validation

The constituent suites pass 263,969 original-ROM comparisons: targeting and
reject tables 214,185, wall collection 22,400, traversal 11,000, and opening/
wall predicates/full rays 16,384. Those counts describe behavioral validation,
separately from the decomp.wtf byte-matching catalogue.

The sector-rejection routine also matches exactly in decomp.wtf: 34 bytes,
17 instructions. `sector-reject-match-attempts.json` preserves the seven stored
attempts and final score zero. The export now rebuilds all 8 MiB byte-identically
with 65 C functions, 1,712/87,648 catalogued code bytes (1.95%). The catalogue is
incomplete and is not a measure of whole-game coverage. All 39 native CMake
libraries build; this remains separate from the GBA rebuild.

`test-enemy-queries` passes 53,384 assertions under ASan and UBSan, using the
established WSL `-no-pie`, `detect_leaks=0` settings. It runs all 16 original maps
with native queries and controller/world movement: 2,753 controller frames,
2,603 chase frames, 1,774 actual enemy moves and 2,593 walk calls. Its 4,096
player frames include 3,376 airborne and 1,536 third-person frames. Additional
path sweeps yield 33 clear and 2,525 blocked dual rays. It exercises corpse
release, target removal, slot reuse, exact ties, two reserved entities, invalid
ownership/projection, nested reentry and last-blocker preservation. The helper's
assertions are included in the total; the report's named guard/order counters
cover only the main fixture. Driver execution also passes.

The integration uses synthetic spawn instructions in a private ROM copy to
construct controlled fixtures. It executes no ROM instructions, while the
separate differential suites do. Results and source hashes are in
`native-enemy-queries-validation.json`. Combat, audio and animation playback
remain callback boundaries. Other controllers, the outer entity dispatcher,
rendering, assets and OS input remain work; there is no playable native build.
