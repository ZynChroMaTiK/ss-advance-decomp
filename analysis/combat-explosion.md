# Explosion damage requests

`src/combat_explosion.c` recovers the complete `080035A8` traversal. The native
entry is `ssa_combat_explode` in `include/ssa/combat_explosion.h`. It produces
synchronous damage requests through an explicit `037F4` callback; this module
does not itself change health, play sounds or allocate an explosion visual.

The caller supplies the source and the original active-pool order as stable
entity pointers. Spatial list order is not a substitute. The source is skipped.
Types CD through D8 qualify independently of the include-players argument;
other types require flag 10 and that argument. Flag 20 excludes either kind.

With squared radius zero, the routine calls the map reject-bit query using
source and target sectors, then excludes target types above D6. It does not
perform a wall ray or distance test. Otherwise it performs no visibility test.
Wrapped XY differences are arithmetic-shifted by 16, squared and summed modulo
32 bits. The unsigned squared distance must be strictly below the radius.

Nonzero-radius attenuation preserves the original instruction sequence:
`fraction = ((distance << 12) / radius) << 4`, with unsigned 32-bit wrapping.
The fraction and `damage << 16` are signed inputs to the original fixed
multiply. Its wrapped result is logically shifted by 16 and subtracted from
damage. This can produce surprising values for large inputs; it is not replaced
with floating-point falloff or clamped damage.

When mode is 1 and the source type is 67, encountering a qualified player-flag
target divides the routine's running damage by four **before** checking range.
This persists for all later targets, including when that player is outside the
radius. Several players can quarter it repeatedly. The zero-radius branch
does not apply this rule.

The original loop reloads active count each iteration. The original damage
routine and its reward helper do not allocate or release entities, so this
native API requires active count/order and body storage to remain stable
during callbacks. It still reads source and target fields live: earlier damage
may change flags observed later. Reentry using the same workspace is rejected.
Callbacks return negative on failure; prior effects remain, and the updating
guard is always released. Invalid map sectors/reject data fail instead of
reading outside the map. Nonzero radius needs no map.

## Verification

Run `.venv-linux/bin/python tests/test_combat_explosion_rom.py` under WSL.
The test executes whole original `035A8`, original B18C reject lookup, original
unsigned division and original IWRAM signed fixed multiply. Only damage `037F4`
is hooked to record exact ordered target/damage requests. Native code builds
with strict compiler warnings and UBSan.

The final run passes **72,325 ROM comparisons**:

- 65,686 type/flag eligibility cases, including all 65,536 type values;
- 4,096 randomized active arrays, including self and empty lists;
- 431 distance, attenuation, strict-boundary and persistent-quartering cases;
- 2,048 cases using all 16 actual map reject tables;
- 64 cases with callback-driven changes to later body flags and source type.

It compares 61,671 damage requests, checks 61,681 rejected reentries, nine
invalid-input guards and three callback failure points. Final source hashes and
per-map counts are in `native-combat-explosion-validation.json`. This verifies
request generation; actual damage and runtime integration require their own
tests. No ROM instructions execute in the native implementation.
