# General damage

`src/combat_damage.c` recovers the complete control flow of `080037F4`.
`include/ssa/combat_damage.h` exposes explicit host projections instead of a
packed original entity or player record. This is behavioral recovery, not an
exact matching candidate.

## Ownership and integration

`SsaCombatDamageTarget` points at a stable entity, canonical signed 16-bit health,
entity timer and player inventory. A flag-`0x10` target requires the timer and
inventory pointers. Inventory supplies the original player-record `+C6` and
`+C4` armor values; it does not implicitly supply the target health pointer.
The native player adapter should pass authoritative `inventory.health`, then
synchronize its entity-health mirror, including after a callback failure.
Movement flags at player record `+BC` are distinct from body flags at entity
`+30`; never synchronize those two fields with each other.

`SsaCombatDamage` explicitly projects the original globals: current damage source
and its entity `+20` owner/target pointer, optional selected weapon-hit counter,
twelve enemy hit/kill counters, player count, local-view index and local player
flags, difficulty, god mode, frame counter, hurt-flash countdown, kill credit,
selected credited player's kill byte, kill limit and winner. The optional
shot counter points at the record selected by `03003F4C`; it is not selected
from a weapon field guessed from the source entity. Credit comes separately
from `03003F40`. Original source identity is not inferred from world ownership.

Callbacks represent `5850` multiplayer damage delivery, `8D5C` hurt sound and
`125E0` reward. They receive the already-mutated target at the exact call site.
The reward callback can invoke the recovered reward helper. Negative callback
results stop later operations and retain earlier effects. A context-wide
`updating` guard rejects callback reentry, including a nested NULL-target call.
Neither callback nor caller may free or change projected storage during a call.
No world query, unlink, pool release or group notification occurs here.

The original dereferences a NULL source for a player target with at most one
player. Native code rejects that malformed input; it does not manufacture an
enemy source. Missing required host pointers also return `-1`. A NULL target
otherwise retains the original no-op behavior.

## Recovered behavior

- Multiplayer winner state suppresses damage. Single-player weapon-hit counting
  happens before the target's flag-1 immunity gate, so protected hits still count.
- A multiplayer player target with a non-NULL source only sends damage when the
  source, or its explicit `+20` referent, is a player controlled by the local
  player. The network amount truncates to 16 bits. This path does not locally
  change armor or health. A NULL multiplayer source follows local damage instead.
- Single-player player damage records the enemy source type. Direct sources
  include `CD..D6`; indirect sources include `CD..D8`. God mode zeroes damage.
  Difficulty 0 halves damage, 1 computes unsigned `damage / 3 * 2`, and 2 adds
  unsigned `damage / 2`. Other difficulty bytes leave it unchanged. Non-player
  targets instead receive 1.5x on difficulty 0 and `damage / 3 * 2` on 2.
- Armor runs `C6` before `C4`. Each nonzero signed halfword subtracts the signed
  low halfword of doubled damage, wraps, then halves unsigned damage. Negative
  remaining armor refunds its signed division by two and is cleared. Negative
  input armor and large unsigned damage therefore have deliberately unusual
  results. No saturation is substituted for original wrapping.
- Player timer becomes 30 in single-player. A local player without record flag 8
  gets hurt flash 9 and sound `15 + (frame_counter & 1)` only for nonzero remaining
  damage and an initially zero flash timer. This precedes health subtraction.
- Health subtraction wraps to signed 16 bits. A result at or below zero becomes
  zero, increments `CD..D8` kill statistics and applies nonzero kill credit.
  Multiplayer kill bytes wrap; equality with the kill limit sets the winner.
  Single-player credit calls the reward boundary. Credited dead player bodies
  get timer 3000 and flag 1 after that boundary.
- After local damage, single-player sets body flag 1 even for survivors. Every
  completed local-damage path clears flag `0x800`. Early immunity, network and
  winner exits do not clear it. The enemy controller remains responsible for
  subsequent death transitions and corpse handling.

## Validation

Run `.venv-linux/bin/python tests/test_combat_damage_rom.py` under WSL, or the
workspace `test-combat-damage` driver once registered. The test compiles native
C with strict warnings and UBSan, executes entire original `037F4` including
the original unsigned division helper, and controls only the three explicit
effect boundaries. It compares full target, game, player-record and settings
byte snapshots plus ordered effect snapshots.

The final suite passes **13,186 ROM comparisons**, **6,586 callback reentry
checks**, **10 invalid-input guards** and **3 callback failure cases**. It covers
all scaling branches, 32-bit damage and 16-bit health/armor boundaries, wrapped
statistics, direct/indirect source types, multiplayer routing, local protection,
god mode, winner gates, NULL targets and credited deaths. There are 232 network,
1,052 sound and 2,009 reward observations. Evidence and source hash are in
`native-combat-damage-validation.json`. Unicorn is only a differential oracle;
the native function contains no ROM execution.
