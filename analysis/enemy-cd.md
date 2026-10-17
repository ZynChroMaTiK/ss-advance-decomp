# Native type-CD controller and world adapter

`enemy_cd.c` recovers the complete control flow of `08013978`, its active-state
helper `08013924`, damage/proximity response `08013868`, and sound postlude
`080137DC`. Type CD is the exploding enemy path. Its presentation and damage
effects remain explicit callbacks; this is not yet a complete playable enemy.

## Original ordering and transitions

State zero invokes the recovered initializer, retaining the 60-then-30-tick
scripted-spawn countdown. State one runs shared navigation `1288C`, then the
CD branch of contact helper `12638`, then periodic health drain `12E1C` and
damage/proximity handling. State two invokes corpse update `12D9C`. Unknown
states skip those handlers. Every completed state path still runs the sound
postlude, including the frame that destroys the corpse.

Contact damage requests 20 damage to the latest blocked player entity when
navigation's last move result is zero and phase is two. `combat_runtime.c` now
binds this callback to recovered general damage `037F4` and authoritative health.

Flag one requests a damage reaction. The enemy faces player-record XY using
`0456C`, copies the desired direction to facing, and either proceeds to death
when health is nonpositive or clears flag one and selects animation 8/period 6.
Player-record position is deliberately separate from the player body used by
navigation and proximity. The original uses those two different pointers.

The proximity target is the alternate entity at original +20 when flag 400 is
set and that pointer exists; otherwise it is the player body. Original table
distance `045C4` triggers detonation when its **unsigned** result is at most
127. Extreme wrapped coordinate differences can produce negative signed
distances; they must not be mistaken for a nearby target.

Detonation enters state two, sets flag 20, selects animation 1C/period 6, invokes
shared death entry (health zero, flag two clear, group release, timer 360),
requests sound 27, then requests `035A8` with radius argument 20000 hex, damage
30 and flags one. `combat_runtime.c` supplies actual radial damage; presentation
effects remain external.
Failures preserve earlier state and stop later effects; no rollback is implied.

The postlude starts sound 3A when navigation phase is two, signed distance is
nonzero and at most 1023, state is not two, and no other entity owns that loop.
It stops the current entity's loop when phase differs from two, signed distance
exceeds 1024, or state becomes two. The asymmetric 1023/1024 thresholds and
negative-distance behavior are preserved.

`037E0` unlinks through `A65C` and calls `051C8`, which clears the complete
116-byte payload through `052B0`. The host controller defers actual clearing
until it returns, but its postlude observes the equivalent cleared phase/state
when destruction is requested. Thus it still stops a loop owned by the released
entity without accessing freed host storage. Check `destroy_requested` even
when a later stop-sound callback fails.

## Native world integration

`enemy_cd_runtime.c` binds one controller to one newly scripted CD actor. Its
stable navigation and controller views reference the existing actor storage;
it does not allocate a second body. Bind before state-zero activation. The
runtime must not be copied, and its slot must not be externally released or
reused while bound. It becomes inactive after native corpse release.

The adapter provides recovered turning, actual world walking/vertical physics,
live moving-sector and floor hazards, animation metadata updates, group
notification and ordered world unlink/pool release. It refreshes the shared
blocker from the world at frame entry as well as after its own movement, so
visibility early exits still observe the most recent collision query.
Group-release and destroy events are consumed internally. Animation events
observe reset native metadata before presentation callbacks.

Call it in the entity phase before the once-per-world script/mover update.
The caller supplies the shared movement-call counter, original frame counter,
level counter and ROM speed coefficient, and updates player body/record
positions. The outer active-list dispatcher and global scheduling remain
separate work. Do not mix manual activation or manual defeat with this bound
controller's state updates.

Visibility `B18C`, nearest-entity selection `036C4`, and single-ray traversal
`A240/A041` are supplied by `enemy_queries.c`, including native `1271C` dual-ray
geometry. Ordinary point movement clearance is not equivalent. The optional
`combat_runtime.c` wrapper consumes contact/explosion callbacks with actual
damage and reward routines. Audio and animation playback remain external.
Query/effect callbacks must retain storage and respect their public contracts.

## Evidence

`test-enemy-cd` passes 7,278 whole-controller comparisons against original ROM
instructions, with navigation `1288C` controlled equally on both sides. The
real activation, contact gate, face/distance math, drain, death, corpse and
sound control flow execute. External effects are observed; native destruction
is projected as cleared payload only after its callback. Coverage includes
different player-record/body coordinates, sound thresholds, damage reactions,
contact gating and final corpse removal. Twenty guards, 8,206 callback reentry
checks and seven effect-failure paths also pass under UBSan.

The shared modules separately pass 88,226 motion/hazard, 10,725 navigation/path
and 15,031 lifecycle ROM comparisons. Read `enemy-motion.md`,
`enemy-navigation.md` and `enemy-lifecycle.md` for their exact oracle boundaries.

`test-enemy-cd-runtime` composes the real native modules with script spawning
and all 16 maps under ASan and UBSan. It checks actual enemy movement/collision,
player jump/H-view updates, group waits, corpse expiry, slot reuse, callback
failures and reentry. Visibility/nearest selection and ray clearance are
controlled dependencies in this integration test; its path-offset calculations
are native. See `native-enemy-cd-runtime-validation.json` for final counts and
source hashes. The established `-no-pie`, `detect_leaks=0` WSL workaround does
not claim LeakSanitizer coverage.

The final integration result is 59,319 assertions over 1,959 controller frames,
including 1,583 chase frames and 1,205 actual enemy moves. It covers 2,048 player
frames, 36 complete corpse-expiry cycles, 37 releases, 16 reused slots, 84 guards,
four failure paths and 5,019 callback reentry checks. The blocker regression uses
an actual world query followed by a visibility early exit and verifies that
contact damage still requests the correct player target.

`sub_08013978` also compiled byte-identically through decomp.wtf on its first
manual attempt: 31 instructions, 62 bytes. The straightforward switch candidate
is `recovered/enemy_cd_dispatch.c`; the saved matcher result is
`enemy-cd-match-attempts.json`. This small exact match is separate from the larger
behavioral controller/native adapters and their explicit external dependencies.

The original sector-reject/nearest/navigation-wall-ray backend is now supplied
by `enemy_queries.c`; see `enemy-queries.md` for its separate native integration.
The earlier controlled-boundary test remains useful for forced controller branches.

Remaining work includes other enemy
controllers, weapon firing/projectiles, animation playback, the outer entity
dispatcher, rendering/audio and OS input. No native playable executable exists.
