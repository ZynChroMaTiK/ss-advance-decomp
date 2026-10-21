# Weapon 7 conversion and periodic drain

## Healthy enemy targeting (2026-10-21)

The existing frontend `nearest_converted` policy now also selects targets for
healthy enemies, including both bosses. They compare living players with living
Prion-affected enemies using the same 3D body-center distance; other healthy
enemies are excluded. Converted actors still consider every living combatant.
Selection refreshes each behavior pass, without waiting for the old retarget
timer. Equal distances retain active-pool order with no explicit player bias.

The selected actor drives facing, pursuit, attack checks and projectile direction.
The opt-in `prion_targets` controller field permits an unconverted ranged actor
to fire when its aim trace reaches its selected Prion target. Ordinary allies
do not satisfy that new attack gate. Both boss projectile paths use the selected
actor too. If a charging CE contacts Sam while pursuing a Prion target, knockback
still applies to the actual player contact. The temporary controller player
projection is restored after each pass. Damage, conversion health, converted
behavior speed and ranged tactics retain their existing policies below.

The targeting regression covers healthy actors choosing and switching between
Sam and Prion enemies, healthy ally/dead/corpse exclusion, vertical distance,
multiple converted candidates, no live targets, all six ordinary ranged types,
both bosses firing away from Sam toward a converted target, and the policy-off
original facing behavior. No campaign playthrough is claimed.

Validation: 8,083 targeting/combat assertions over 662 frames, including 114
healthy-enemy cases and 14 ranged/boss shots. The default-off controller suites
retain 27,738 ROM comparisons; all eight focused CTests and the 415 boss
self-projectile checks pass. The Windows frontend and all 128 libraries rebuild.

## PC Prion balance (2026-10-21)

The playable adapter enables `combat.prion_balance`. A successful first Prion
conversion doubles the victim's **current remaining health**, once, before E2
allocation. Positive health saturates at32767; dead actors are not revived, and
another hit on an already converted actor does not boost it again. The original
conversion flag, drain period/countdown and effect-allocation failure semantics
remain intact. The recovered default-off runtime preserves ROM behavior.

The subsequent user correction removes the conversion-based damage multipliers.
Melee, projectiles and explosions now retain their normal damage regardless of
whether either enemy is Prion-affected. Contact deals20 normally,40 for D0/D6;
there is no half-melee or double-other-damage adjustment. This also applies when
both enemies are converted. Original splash attenuation, difficulty scaling,
player combat and boss self-projectile immunity remain intact. The current-health
boost, targeting and pursuit/ranged policies are unchanged. The damage matrix
checks all four conversion combinations with the policy both enabled and disabled.

Converted enemies chasing other enemies use half their previous movement
coefficient. The six ranged types CF/D1/D2/D3/D4/D6 stop advancing when within
768 units and a native aim trace reaches their selected enemy. They keep facing
and firing; occluded/out-of-range targets still cause pursuit. Gravity and
contact queries continue while holding position, so nearby melee remains
possible. Melee-only types keep approaching at the reduced speed. The existing
two behavior/animation updates per tick, firing cooldowns and drain clock are
unchanged. Pursuit of the player is unchanged.

`test-prion-targeting` adds real repeated Prion impacts, low-health doubling,
saturation, dead-target and default-off checks for all ten types; an enemy/player
damage matrix for contact, projectiles and explosions; and actual ranged holds
and pursuit. The older special-weapon behavior-clock fixture disables this new
policy to continue isolating its one-versus-two-update assertions.
No campaign playthrough is required or claimed.

Validation:6887 checks,540 controller frames,270 damage cases,50 additional
conversion-health cases,6 ranged holds and10 pursuit cases pass with identical
MSVC and GCC ASan/UBSan/leak-check results. The existing60 enemy melee contacts
and80 target selections also pass. All8 focused CTests,32316 special-weapon
checks,415 boss-projectile checks and446 Gunrilla assertions pass. All128 Windows
native libraries/frontend rebuild, and a hidden60-frame1920x1080 launch succeeds.

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
toward the selected actor. Enemy melee contact is hostile whenever either body
has conversion flag400, so converted enemies can both deal and receive melee
damage, including contact with an enemy other than their selected target.
The existing combat path applies20 damage, or40 from D0/D6. Each walking update
captures its actual enemy collision before a crowd sidestep can replace it;
successful sliding therefore no longer suppresses melee. Contact is reset for
each move and runtime call, so stale blocking pointers cannot cause a hit.
Self, player bodies, corpses, released slots and nonpositive canonical target
health are rejected by this additional enemy-only path. Ordinary enemy pairs
remain nonhostile, and the original player-contact path is unchanged.
Native floor hazards run first. With no
living candidates, the target clears and the actor does not attack.

This is a requested PC policy. The recovered navigation, contact and controller
routines retain their original behavior when the runtime option is off.
The healthy-enemy extension above supersedes the earlier player-only policy.
The existing behavior-clock
fixture explicitly disables nearest selection to isolate its timing assertions.

`test-prion-targeting` performs actual Prion impacts on all ten eligible enemy
types. The expanded melee fixture covers both directions and two converted
enemies with crowd sliding on/off, ordinary nonhostile pairs, dead targets,
disabled policy, and stale contact after both skipped movement and free walking.
It also covers 80 target selections, prompt retargeting, nearest-player selection,
dead/corpse rejection, vertical distance, six ranged attack types, a boss target
and no-target cases. Reports:
`prion-targeting-validation.json` and `windows-prion-targeting-validation.json`.
The 2026-10-20 run passes 3,512 checks over514 controller frames, including60
verified melee hits, with identical GCC ASan/UBSan/leak and MSVC counters.
The Windows crowd suite passes20,456 checks, and all eight focused CTests pass.
All127 native Windows libraries and the playable frontend rebuild successfully.
Full campaign replays were not repeated; earlier route reports describe their
recorded build.
