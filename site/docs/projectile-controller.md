# Projectile controllers

`projectile_controller.c` recovers the complete original dispatch and active
controllers for entity types 65, 66, 67, 69 and 6B. These are hexadecimal entity
types, not weapon numbers. The mapping was read from the jump table at
`08004E68` inside `08004E04`, and tests also decode each branch's actual BL:

| Entity type | Table branch | Controller | Initial animation | Impact |
| --- | --- | --- | --- | --- |
| 65 | 08005078 | 08010E5C | 0F, period 5 | Radius 20000 hex, damage 100, particle 4 |
| 66 | 08005080 | 08010EF0 | 44, period 5 | Radius 20000 hex, damage 5, particle 4 |
| 67 | 08005098 | 080111C0 | 5A, period 5 | Radius 20000 hex, damage 100, particle 4 |
| 69 | 080050A8 | 080113C4 | 5F, period 5 | Release only |
| 6B | 08005090 | 08011120 | 87, period 5 | Radius 20000 hex, damage 5, particle 15 |

All five initialize only in state zero, setting animation before state one.
Type 67 then sets health to 160. Initialization immediately runs the active
controller in the same call. States other than zero and one do nothing. Types
65 and 67 explode immediately when speed is zero, without calling motion.
The other types call motion even at zero speed. A nonzero motion result keeps
the projectile alive; zero requests impact/destruction.

For exploding projectiles, the original owner byte at +54 and weapon byte at
+55 become global credit and hit-stat weapon index before explosion. Both
globals clear to zero after explosion, before particles and destruction.
Original callbacks are synchronous. The adapter exposes these values in
`SsaProjectileControllerGame` so the composed runtime can bind them to combat.
The explosion includes players. Its radius argument is squared distance, as
specified by the native explosion module, not a linear world distance.

`SsaProjectileController` has a separate update guard from `SsaProjectile` so
the motion callback can enter the native whole `10A20` function. Motion may
change the projectile; other events are observe-only except their external
effect data. Animation id, period and completion byte are set before the
animation event; the runtime resets the separate sprite frame counters.
Destruction is deferred: its successful event marks `destroy_requested`, and
the runtime then unlinks/releases after the controller returns. Storage and
all addresses must remain stable throughout a call. A failed event retains
earlier changes; explosion credit is still cleared on failure as native error
cleanup. No later events execute. A failed destruction request leaves the
projectile intact. A previously requested destruction cannot be stepped again.

Types 68 (`11350`) and 6A (`11090`) require child projectile construction and
are deliberately rejected by this adapter; their omission is explicit.
The outer entity dispatcher is not implemented by this file.

`tests/test_projectile_controller_rom.py` executes whole original routines
against controlled motion and effects, comparing every projected entity field,
preserved raw bytes, both credit globals and ordered callback snapshots. Motion
mutates position and speed on both sides to verify subsequent event arguments.
The suite covers every state byte, signed speed extremes, every owner byte,
weapon byte boundaries, both motion outcomes and randomized active frames.
It passes **25,600 ROM comparisons**, **28,969 callback reentry checks**,
**23 callback failure paths**, and **12 native guards**, with UBSan enabled.
Counts and source hash are in `native-projectile-controller-validation.json`.
These boundary tests do not substitute for native world/combat integration.
