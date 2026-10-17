# Native shared enemy lifecycle

`src/enemy_lifecycle.c` recovers three shared enemy helpers. They use the
existing `SsaEnemyActivation` host projection rather than another copy of enemy
health, state and timer. No general combat damage, attack simulation, rendering
or full enemy dispatcher is implied by these helpers.

| Original address | Native API | Behavior |
| --- | --- | --- |
| 08012D5C | `ssa_enemy_death_begin` | Clear health and solid flag 2, notify script group, assign corpse countdown |
| 08012D9C | `ssa_enemy_corpse_step` | Sink corpse, handle animation completion and final flashing/removal countdown |
| 08012E1C | `ssa_enemy_drain_step` | Periodic one-point health drain using entity halfwords +34/+36 |

Death entry sets health to zero, clears flag 2, then calls original group
release `0801C6B0`. After that callback it sets timer +4E to 720 for D7/D8,
otherwise 360. It does not change animation or state: individual enemy
controllers do that before invoking this helper. The original helper releases
the group on every invocation; the caller is responsible for calling it once
on the transition to death. A callback failure retains the health/flag changes
and leaves the old countdown untouched.

Corpse updates subtract `ticks << 16` from Z with 32-bit wrapping, then perform
a signed clamp against the signed 16.16 floor. If animation byte +5F is nonzero,
flag 1 is cleared and flag 20 is set. Timer subtraction wraps at 16 bits. A
signed countdown of 119 or less sets flag 40; a negative countdown then becomes
zero, clears flag 40 and requests destruction. Countdown zero alone does not
destroy the corpse. The subtraction/clamp order is preserved even at 32-bit
wrap boundaries.

The destruction boundary is original `080037E0`, which first calls world
unlink `0800A65C` and then pool release `080051C8`. Those operations already
have native world/pool implementations, but storage ownership belongs to the
caller. The lifecycle callback therefore requests destruction and the caller
must perform ordered unlink/release after the helper returns. It must not
clear or free the activation view from inside the callback. A successful
destruction request returns 1; a retained corpse returns 0. Failed callbacks
return -1 while retaining the preceding timer, flags and Z changes.

Periodic drain uses the original halfwords at +34 (period) and +36 (countdown).
A zero period performs no work. Otherwise, countdown subtraction wraps; only
a negative signed result decrements health by one, also wrapping at 16 bits.
Positive resulting health resets countdown to the period. Nonpositive health
sets flag 1 and sets countdown to zero. This helper does not clamp health,
notify script groups or start a death animation. In particular, subtracting
one from health -32768 wraps to +32767, matching the original. This is distinct
from the substantially larger general damage routine `080037F4`, which includes
player/network/scoring behavior and remains outside this module.

## Ownership and integration

All helpers reject null view/body, reentry, ticks outside byte range, and active
world queries. Animation completion must fit the original byte. Death and
corpse updates require an explicit callback rather than silently dropping
group release or destruction. Callbacks receive read-only views and must not
mutate the view/body or reenter the helper. The shared activation `updating`
guard protects callback boundaries. Caller storage and entity addresses remain
stable; corpse updates only change Z and do not alter spatial XY membership.

Periodic drain receives a separate caller-owned halfword countdown; it does
not overwrite the corpse/activation timer. Its period and countdown correspond
to original +34/+36, while the shared view timer corresponds to +4E. The enemy
controller owns all three fields and animation completion +5F.

## Validation

Run `tests/test_enemy_lifecycle_rom.py` with the workspace Ubuntu Python.
It builds the native code with warnings-as-errors and UBSan and executes the
three whole original routines in the test-only Unicorn oracle. Group release
and destruction are controlled boundaries; their full entity snapshots and
order are compared at callback entry, and all 116 entity bytes are compared
after every call. This does not assert that group bookkeeping, world unlink or
pool clearing execute inside this isolated differential test.

- 15,031 original-routine comparisons: 4,355 death entries, 5,860 corpse updates
  and 4,816 periodic drain updates.
- Boundary coverage includes boss/nonboss types, all important timer thresholds,
  zero/nonzero completion bytes, extreme signed floors and Z wrap, zero drain
  periods, signed health wrap and randomized complete entity records.
- 20 native argument/lifetime guards, 21,201 callback reentry checks and two
  callback failure paths preserving preceding effects.

`native-enemy-lifecycle-validation.json` records the source hash and counts.
These are behavioral ports; no new byte-identical matching claim is made.
