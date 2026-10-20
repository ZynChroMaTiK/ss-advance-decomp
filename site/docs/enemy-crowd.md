# PC enemy crowd movement

The playable frontend enables local body sliding for CD and CE..D8 walking
controllers. A blocked enemy moves along the blocking enemy's square collision
face, retaining its chosen side across frames so it can get around the body
instead of steering immediately back into it. Centered head-on pairs choose
opposite world directions. If a wall blocks that side, the other side is tried.
The original navigation, turning, speed, attack logic and Time Strike freeze
gate remain in place. This is a requested PC behavior extension, not original
ROM behavior or a new matching function.

`ssa_enemy_walk_world_sliding` uses the recovered walking routine's existing
movement callback. The movement-call counter and vertical physics still run
once per walk, including blocked walks. Horizontal displacement is split into
steps of at most four units per axis. A lateral step uses the larger requested
component, so its length cannot exceed that requested step's length. Every
candidate goes through world entity contacts, static walls, sector clearance,
steps and normal spatial unlink/relink. It moves only the current enemy and
does not push other bodies, clear collision flags, or exclude a neighbor.

Already overlapping enemy pairs can move gradually apart. The PC query permits
this only when they were already colliding, neither absolute separation shrinks,
at least one grows, and neither center is crossed. Other bodies are checked
independently: a previously clear pair cannot gain overlap under this exception.
The exception does not apply to players. Player blockages retain the original
blocked result for contact attacks; failed side probes restore the original
enemy blocker instead of leaving a later probe's wall/player result behind.
For Prion melee, crowd movement additionally records the first real enemy
contact before trying sidesteps. This transient contact resets on every move;
it is distinct from the persistent side-preference blocker. Combat consumes it
within the same controller call, allowing melee when either body is converted
even if the attacker subsequently slides successfully. It is never reused as
a target on a later frame or dereferenced after actor retirement.

The PC query extends entity-cell searches by128 units to cover all original
enemy radii, including Gunrilla's90 units across cell boundaries. Original
`ssa_world_query/try_move` retain their32-unit search margin and exact recovered
contact behavior. The world structure and original API layouts are unchanged.
The crowd API's coverage contract is original enemy radii up to128 units.

Zero-initialize one `SsaEnemyCrowd` per enemy lifetime. Its cached blocker is an
identity for the side preference only and is never dereferenced. Generic runtime
bindings reset this state on slot reuse; CD initialization clears its own state.
`crowd_slide` defaults off in both runtime adapters. The native frontend enables
the generic adapter and `game.enemy_crowd_slide`, which propagates to newly bound
CD actors. Shared recovered controller/motion entry points stay unchanged.
State/world query guards reject reentry; callbacks must honor existing world
ownership and synchronous-special contracts. Stable storage remains required.

Validation commands:

```powershell
wsl -d Ubuntu --exec .venv-linux/bin/python tests/test_enemy_crowd.py
powershell -ExecutionPolicy Bypass -File scripts/test-enemy-crowd.ps1
```

The dedicated fixtures cover a stationary blocker, two head-on enemies,
coincident spawns with monotonic separation checks, wall-side reversal, a fully
blocked corner, player contacts, one-call gravity, large-displacement collision
substeps, large-radius cell coverage and reentry guards. Diagnostic room fixtures
use the real world/geometry code. Separate walking fixtures cover all16 original
maps and all enemy type IDs; a512-tick first-level native input run verifies
live AI movement and automatic CD policy binding. These are focused integration
fixtures, not an uninterrupted campaign completion proof.

Reports: `enemy-crowd-validation.json` and `windows-enemy-crowd-validation.json`.
Original world and enemy-motion ROM comparisons remain separate regressions.

Current dedicated result:20,456 ASan/UBSan/leak assertions,1,503 diagnostic
crowd frames,158 overlap separation checks,6,144 real-map walks and512 native
input ticks with274 observed enemy moves. MSVC and GCC agree. Original motion
still passes88,226 ROM comparisons; original world predicates/queries/moves,
synchronous pickups, spatial lists and player/jump/H integration still pass.
The Windows special-weapons checker also retains22,349 passing checks.

The normal-input first-level run now finishes in5,110 frames with24,000 score,
eight pickups,19 spawns and100 health, using13 pistol and21 shotgun shots.
Its earlier28,000 minimum score failed despite a successful exit: crowd motion
changes how many kamikazes detonate without player credit. The completion test
now requires positive earned score in original2,000-point increments, zero
living enemies, zero outstanding script groups, all19 spawns, survival, pickups
and the actual VM exit. It neither grants score nor changes the input route.
Windows compares every completion field against the current source-hashed
sanitizer run. The profile completion/save test uses that same current result
instead of its old4,977-frame assumption. The full nine-map campaign remains
historical evidence and was not replayed for this movement change.

## PC player clearance

Campaign probing exposed the same32-unit broadphase limit in player movement.
A Gunrilla's90-unit radius can overlap Sam before its center enters that search.
Once the overlap is detected, the original predicate can reject retreat as well.
`ssa_world_try_move_player` searches128 units for enemy centers and permits only
monotonic separation of an already overlapping player/enemy pair: neither axis
distance may decrease, at least one must increase, and centers cannot cross.
New contacts, walls, floor/ceiling checks and spatial relinking remain active.
The enemy is never moved or made nonsolid.

Only CD..D8 bodies participate in the additional cells. In particular, E1
pickups retain their original cell-based touch range and script timing. The
original query, try_move, horizontal and player-motion entry points retain their
ROM behavior. `ssa_world_horizontal_step_player` and
`ssa_player_motion_world_step_pc` bind the policy to the existing PC player
runtime without changing public structure layouts.

The12 focused scenarios now include reproducing the missed Gunrilla contact,
blocked approaches, gradual retreat, a second enemy obstructing retreat, center
crossing, wall limits, unchanged pickup coverage and invalid/reentrant calls.
An isolated player-runtime fixture runs16 movement/jump/H frames out of a
controlled overlapping spawn with presentation/hazard boundaries controlled;
the separate512-frame full-game fixture still runs real native AI.
GCC ASan/UBSan/leaks and MSVC agree on20,456 checks. Original world tests and
8,672 original player-motion comparisons also pass. Windows gameplay smoke,
both startup renderers and the normal5110-frame first-level completion pass.

The tenth campaign map also completes from its original Normal-difficulty
standalone loadout with the real mixer and both renderers. This is a separate
development proof, not campaign admission. Reproduce it with
`tests/test_campaign.py --fresh 9` or
`ssa_native_campaign_check.exe --fresh 9`. The default Python command still
requires an uninterrupted campaign run and writes a different report. The
current full replay reaches the ninth map, where the bot dies; the frontend
therefore remains capped at nine maps. See `campaign-standalone-validation.json`.
