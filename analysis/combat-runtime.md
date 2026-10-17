# Native combat integration

`combat_runtime.c` binds recovered damage `037F4`, explosions `035A8` and score
rewards `125E0` to native player and scripted actor storage. Its CD wrapper
consumes contact and explosion events internally; animation and audio still
go to presentation callbacks. Supply `enemy_queries.ops` to use the recovered
navigation queries. Direct damage entry points are available for future weapon
and projectile callers; this does not implement firing or hit selection.

## State and ownership

Initialize after players, world and script-spawn construction. Supply existing
entities in original reserved-slot order and player runtimes indexed by player
ID. Pointer arrays are copied; their objects remain externally owned. Owned
script actors resolve through their stable pool slots. Other reserved bodies
can participate in explosion iteration, but this adapter has no health view for
them: a damage request to an unsupported body fails explicitly. Initial map
enemy construction/dispatch still needs a corresponding binding.

Player `inventory.health` is canonical; `entity_health` synchronizes before
external damage callbacks and after every exit, including failure. Armor uses
the inventory's C6 then C4 fields; the hit timer uses `runtime.frame.timer`.
Player movement flags (+BC) and entity body flags (+30) are different fields;
damage never copies body flags into movement flags. Scripted enemy health and
timer refer directly to `actor.activation`; script flag/timer mirrors refresh.

Source and owner explicitly model damage's current-source globals. They are
independent of an explosion's origin and restored after each call. The local
view's movement flags are sampled from its bound runtime; the previous context
value is restored. Difficulty, mode/player count, kill attribution, hit/kill
statistics and the hurt-flash counter remain explicit shared context. This
adapter does not infer attribution from a damage source or implement the outer
loop that decrements presentation counters.

Combat locks player runtimes and scripted ownership while applying effects.
The CD wrapper leaves the spawn lock to the existing controller while retaining
its own/player locks. Callbacks must only observe gameplay state and perform
their presentation/network operation. Direct world/level APIs do not consult
all these locks: the callback contract, rather than universal API-level guards,
forbids using them to mutate or tear down the world. Keep all storage stable.

Damage changes health/flags immediately. An enemy's own controller later performs
its death transition and script-group release. Corpse expiry subsequently unlinks
and releases its slot. Killing a neighboring CD enemy therefore schedules its
detonation for that enemy's update; it does not recursively destroy it inside
the explosion loop. Pool order stays fixed during damage, as in the recovered
damage/reward routines. Rewards update the shared score without spawning objects.

Network damage events remain explicit: a locally attributed outgoing hit can
emit a packet request without changing remote health. A received hit with NULL
current source applies locally. No networking transport is implemented here.

Failures preserve effects that already occurred. For example, a player hurt-sound
failure retains consumed armor, the hit timer and flash, but happens before
health subtraction. An explosion failure can occur after the source enemy has
entered its corpse state and notified its script group. The caller must handle
that error; retrying the controller does not replay its earlier explosion.

## Evidence

New behavioral suites pass 155,517 original-ROM comparisons:

- Damage: 13,186 whole-routine cases, including armor, difficulty, attribution,
  statistics, signed halfword wrapping and ordered effects.
- Explosion selection/attenuation: 72,325 cases, including all 16 maps.
- Reward: 65,910 cases, including every 16-bit type and score overflow.
- Combined explosion/damage/reward: 4,096 whole chains on all 16 maps, executing
  53,417 original damage calls and 9,508 original reward calls. Only network and
  sound boundaries are controlled; original damage/reward instructions execute.

`test-combat-runtime` passes 26,435 native assertions with ASan and UBSan
(`-no-pie`, `detect_leaks=0`, no LeakSanitizer claim). Its tests cover all 16 maps,
130 combat-controller frames, 32 enemy chain deaths/rewards, corpse release,
script-group updates, 16 player deaths and 512 subsequent death frames. Another
2,560 player frames include 1,872 airborne and 1,792 third-person frames. A real
world collision exercises contact damage followed by same-frame explosion
suppression through flag one. Additional fixtures cover two native players,
network routing, invalid bindings, 681 guards and three failure scenarios.

Synthetic spawn scripts modify a private ROM copy only. Direct damage is a test
trigger, not an implemented weapon. The native integration executes no ROM
instructions. Reports record exact source hashes; the PowerShell test driver
and all 43 native CMake libraries pass.

This pass adds behavioral native code, not byte-identical matching candidates.
The exact catalogue remains 65/526 functions, 1,712/87,648 code bytes (1.95%);
the last full ROM export remains unchanged. Reward `125E0` is outside that
incomplete imported catalogue; it was not added speculatively. Remaining work
includes weapon firing/projectiles, other enemy attack controllers, initial
enemy binding/outer dispatch, rendering, audio playback and OS input. There is
no playable native executable yet.
