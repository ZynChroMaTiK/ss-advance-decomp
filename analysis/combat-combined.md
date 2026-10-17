# Combined explosion, damage and reward verification

`tests/test_combat_combined_rom.py` compares a native C composition of
`ssa_combat_explode`, `ssa_combat_damage` and `ssa_combat_reward` against the
complete original call chain `080035A8 -> 080037F4 -> 080125E0`. Original reject
lookup, unsigned division and fixed multiply also execute. Only the network
send `08005850` and sound request `08008D5C` are replaced with observation
callbacks. Hooks at damage/reward entries count calls without replacing those
instructions.

The test builds a small composition shim in `build/native-tests`, with strict
warnings and UBSan. Each explosion uses a randomized ordered active pool of up
to 100 entities. Player records are shared by player index, as in the original.
The test compares full original pool/game/player/settings byte projections and
hurt-flash globals, retaining random bytes in fields the recovered routines
should not touch. It also compares those complete snapshots at every ordered
network and sound callback. Score uses the actual native reward function.

Coverage includes player/enemy health and flags, armor layers, protection
timers, difficulty scaling, god mode, local-player gates, initial winner gates,
shot counters, per-enemy hit/kill counters, credited player kills and score.
Credits, difficulty, source selection and radius modes vary independently.
The explosion's position source and the damage routine's global current source
are distinct in 3,072 cases. Some origins are active pool entities, exercising
identity exclusion. Source-owner projections, null multiplayer sources and
unsigned wrap inputs are included.

The final run passes **4,096 whole-chain comparisons**, 256 on each of all 16
actual map reject tables. It executes **53,417 damage calls** and **9,508 reward
calls**, matching **887 network events** and **830 sound events**. There are
1,024 zero-radius sector-mode cases. Source hashes, the composition-shim hash
and per-map counts are in `native-combat-combined-validation.json`.

Run `.venv-linux/bin/python tests/test_combat_combined_rom.py` under WSL. Unicorn
is only the test oracle. This test does not add a native network backend,
sound playback, entity lifecycle dispatch or a playable runtime; those are
separate integration concerns.
