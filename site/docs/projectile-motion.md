# Projectile motion

`src/projectile_motion.c` recovers the complete control flow of `08010A20`.
`SsaProjectile` is a host projection; it is not a ROM memory overlay. Bodies,
source pointers and owner-player bodies must remain stable during callbacks.
The timer uses unsigned storage and signed halfword comparisons, as in the ROM.

Every frame first saves XYZ and all three velocities, then adds the unsigned
tick byte to the timer with halfword wrapping. For a player-owned projectile,
flag `4000` remains set while the wrapped signed product of timer and speed is
at most 400 fixed-point units. While set, projectile XY follows the owner's
current full heading and position. Muzzle offsets differ for types `67` and
`69`. Z follows the owner's Z plus the muzzle offset and `(timer-1)*velocity_z`.
The resulting velocities are rewritten from the heading. These changes happen
even if the subsequent signed timer test exceeds 480 and expires the projectile.

Otherwise XY advances by velocity times ticks. A successful `043D0` move is
followed by `04CE0` vertical physics. Only success from both returns continued
flight immediately. Failure samples `0400C`, the current blocking entity:

- Type `68` applies 100 damage, temporarily writes owner/weapon to credit and
  hit-stat globals, clears both, and continues flight.
- Type `69` converts types `CD` through `D6` when flag `400` is absent. It sets
  that flag before the callback retained under the legacy name `freeze`.
  The callback sets target health-drain period `+34` to 30, preserves the
  separate countdown `+36`, and attempts an `E2` allocation with source=target, zero velocity,
  Z=target Z+height+16, flag `20`, animation `5B`, period 5, then insertion.
  Allocation exhaustion still leaves the target converted. The projectile stops.
  Flag `400` selects other enemies through shared navigation; it does not stop
  movement. See `projectile-conversion.md` for retargeting and drain timing.
- Player-owned type `67` penetrates other bodies unless they are `D7` or `D8`.
  It subtracts the projectile's current signed health from the target's old
  signed health with halfword wrapping, applies damage, and stores the negation
  of that difference as its own health. A nonnegative result continues flight,
  including zero. Otherwise it clears health and speed and stops. The damage
  callback receives sign-extended health as a 32-bit unsigned argument.
- Other entity collisions stop flight. Type `67` without an owner, or striking
  `D7`/`D8`, clears health and speed.

A player-owned type `67` colliding without a blocking entity unlinks itself,
restores the six saved XYZ/velocity fields, reinserts, then invokes `04718` and
returns its result. The timer and guided-flight flags are not restored.

Collision, vertical physics, target health access, damage, conversion/effect creation,
unlink/reinsert and bounce are explicit callbacks. The standalone routine does
not silently substitute a wall response or allocate mock effects. `-1` reports
invalid integration or callback failure; already performed changes remain.
Credit/hit-stat fields clear even when the type `68` damage callback fails.
The projectile update lock always clears. Callback owners must preserve body
storage and prohibit ownership changes except documented unlink/reinsert.

Run `tests/test_projectile_motion_rom.py` under the WSL Python environment.
It compares 16,384 whole original calls, including original trig/fixed multiply,
all written projectile bytes, target flags/health/drain period, allocated
effect bytes, callback order and credit globals. Collision/vertical/effects are
controlled boundaries in this test. It checks 28,510 rejected reentries,
11 invalid-input guards and nine callback failure paths under UBSan.
The report and source hash are in `projectile-motion-validation.json`.
This is behavioral recovery, not a byte-identical matching claim or a playable
runtime.
