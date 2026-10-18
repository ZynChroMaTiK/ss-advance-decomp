# Weapon 7 conversion and periodic drain

The earlier description of type `69` as freezing an enemy was incorrect.
The recovered instructions implement enemy retargeting and periodic health
drain. There is no demonstrated freeze/thaw transition, and no missing flag-400
freeze gate should be invented in the entity dispatcher.

The original impact routine `08010A20` accepts enemy types `CD` through `D6`
without flag `400`, sets that flag at `08010C74`, and writes 30 to entity `+34`
at `08010C78`. It does **not** overwrite the separate halfword `+36` or the
target-selection timer `+56`. Allocation of its attached `E2` effect follows
these mutations; allocation exhaustion does not undo conversion.

Shared navigation `0801288C` checks flag `400` at `080128D6`–`080128DE`. If set,
it subtracts ticks from `+56`. A null target, signed timer at or below zero, or
target flag `20` causes a fresh nearest-enemy search `080036C4`, with timer
reset to 600. Entity `+20` stores the selected target. The nearest search
considers other `CD`–`D6` bodies without flag `20`, in original active-pool order;
it does not exclude other converted enemies. A missing target falls back to
the player. This selection happens after the existing player-sector rejection
and flag-800 early exits, so it does not bypass those navigation gates.

The target affects path destinations. Phase zero still faces the player, as
the original routine does. Type-CD's active controller additionally selects
its alternate target for its proximity explosion. It keeps moving; it is not
frozen. `08012638` still performs contact damage only when the current blocking
entity has player flag `10`: 20 damage for most types, 40 for `D0`/`D6`.
Conversion does not introduce a universal team-based damage exclusion.

`08012E1C` interprets `+34` as the nonzero drain period and `+36` as its current
countdown. It subtracts the unsigned tick byte with halfword wrapping. Only a
strictly negative signed result decrements signed health by one. Surviving
health reloads `+36` from `+34`; nonpositive health sets `+36=0` and body flag 1
for the controller's ordinary hit/death path. Period 30 therefore produces a
31-call cadence when ticks is one and the freshly reloaded countdown is 30.
Neither expiry clears conversion flag `400` nor restores a previous target.

Read-only inspection found these calls in all ten eligible enemy controllers:

| Type | Call to drain `12E1C` | Call to navigation `1288C` |
| --- | --- | --- |
| CD | `0801386E` | `08013928` |
| CE | `0801346C` | `080134DA` |
| CF | `08012EC0` | `08012FA4` |
| D0 | `080130DC` | `08013144` |
| D1 | `080139F8` | `08013A7E` |
| D2 | `08013218` | `080132B6` |
| D3 | `08013E64` | `08013EE2` |
| D4 | `08014040` | `080140D2` |
| D5 | `08013BE8` | `08013D08` |
| D6 | `08013674` | `080136E6` |

This address inspection does not claim that all those controllers are ported.
The native shared navigation and drain routines were already recovered and
differentially validated. The integration bug was field ownership: projectile
impact wrote `actor.parameter34`, while the CD runtime used an unrelated
`controller.drain_period` copy. `SsaScriptActor` now owns canonical `parameter34`
and `parameter36`; the CD runtime imports both at initialization and frame
entry and exports both after the call, including error paths. Existing tests
that set drain state now use those canonical actor fields.

`test_projectile_conversion.py` passes 686 ASan/UBSan assertions on real map 0:
an actual weapon-7 shot converts a scripted CD, preserves an initial countdown
of 17, and creates the original attached effect. The converted CD runs 49
updates with actual native visibility, nearest-enemy, path, world movement and
combat. It moves in 48 updates, selects another CD, refreshes its target timer,
and drains health twice with strict-negative timing. A real native damage call
weakens it before the second drain triggers its ordinary death/explosion/group
notification. The attached effect follows for 45 updates and is removed after
the target gets flag `20`. An injected animation-observer failure verifies that
the countdown remains synchronized after a partial update. Corpse animation
completion is controlled by the test; animation playback is not claimed.

The preexisting CD-runtime and native-enemy-query integration suites also pass
after the canonical-field migration: 59,319 and 53,384 assertions respectively,
both covering all 16 maps. The new result and source hashes are recorded in
`native-projectile-conversion-validation.json`. These are native integration
assertions, not additional ROM comparisons or exact compiler matches.

## Requested PC behavior speed

The playable frontend enables `fast_converted` on entity and actor-animation
runtimes. A living CD..D6 with flag400 gets two complete controller updates per
world tick, including walking, facing, navigation, attack timers and periodic
drain. Its actor animation likewise advances twice. Per-hit damage and original
period/countdown fields retain their values; drain follows this doubled behavior
clock too. Projectiles, player input, music and other actors keep their normal
clock. Paused frames advance neither pass. A death/release/error in the first
pass stops the second; dead actors and corpse animation/expiry use normal time.

Default runtime policies remain off, preserving the original recovery APIs.
The original conversion integration still passes686 checks; entity scheduling
passes85,982 and actor/corpse playback75,088. The extended special-weapon suite
fires a real Prion shot at each of all ten eligible types, then verifies one
versus two actual walks/retarget/drain/animation increments, unchanged unconverted
actors, pause and one-tick corpse expiry. GCC sanitizers and MSVC agree.

## Requested PC nearest-target behavior

The playable frontend now enables `entities.nearest_converted`. Before each
living converted controller pass, it selects the nearest living combatant from
the active pool by three-dimensional body-center distance. Candidates include
players, ordinary enemies, bosses and other converted enemies. Self, dead actors,
corpses, items and projectiles are excluded. Equal distances retain active-pool
order, with no player preference. Selection refreshes every behavior pass,
including both passes of the existing doubled behavior clock, rather than
retaining the original 600-tick target preference.

The controller temporarily sees the chosen actor as its target for facing,
visibility, distance checks, reactions and attacks. The real player fields are
restored after the call, including errors. Converted ranged attacks launch
toward the selected actor. Converted melee contact can damage that enemy through
the existing combat path, using 20 damage, or 40 for D0/D6. It requires an actual
blocked walk during this update and contact with the selected target; stale
blocking pointers cannot cause a hit. Native floor hazards run first. With no
living candidates, the target clears and the actor does not attack.

This is a requested PC policy. The recovered navigation, contact and controller
routines retain their original behavior when the runtime option is off.
Unconverted enemies still target the player normally. The existing behavior-clock
fixture explicitly disables nearest selection to isolate its timing assertions.

`test-prion-targeting` performs actual Prion impacts on all ten eligible enemy
types and passes 1,602 ASan/UBSan checks: 80 target selections, prompt retargeting,
nearest-player selection, dead/corpse rejection, vertical distance, enemy melee,
six ranged attack types, a boss target and no-target cases. Windows agrees on
every counter. The special-weapon suite still passes 32,318 checks, and the
default entity-runtime integration passes 85,982. All 112 Windows libraries and
the playable frontend rebuild; launcher/startup/restart checks pass. Reports:
`prion-targeting-validation.json` and `windows-prion-targeting-validation.json`.
Full campaign replays were not repeated for this targeting change; earlier route
reports describe their recorded build.
