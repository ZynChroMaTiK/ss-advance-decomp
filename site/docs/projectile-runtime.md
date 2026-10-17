# Native projectile runtime

## Gunrilla launch collision correction

The native PC runtime now lets type67 cannonballs leave their typeD6 Gunrilla
shooter. The original80-unit muzzle offset lies inside the combined90-unit
shooter/16-unit projectile square collision bounds; the first16-unit move can
hit the shooter and immediately detonate. Original32-unit entity broadphase
padding can hide that overlap in some cell/heading combinations, making the
failure position dependent.

The motion adapter calls `ssa_world_try_move_ignoring` only for unowned type67
shots whose registered typeD6 source currently overlaps their body. The query
skips that one source, preserving all other entities, specials, walls and
floor/ceiling commits. It retains no exclusion and changes no shooter flags or
list membership. Once the current bounds separate, subsequent moves use the
original query again; returning into the shooter can still collide. Player
cannonballs and other enemy/projectile types retain their old behavior. The
standalone recovered03D7C predicate and10A20 routine remain unchanged. This is
the user's requested native gameplay correction, not a new byte-exact recovery.

`decomp.ps1 test-gunrilla` passes418 ASan/UBSan/leak assertions over39 projectile
frames on actual map geometry. It constructs/activates the realD6 dimensions,
reproduces the original shooter blocker in all8 headings, verifies escape
without damage/particles, then proves8 return impacts and one real wall/player/
enemy impact each. Static clearance chooses a flat launch fixture; shooter
relocation is test setup, not a change to spawning or AI. Creation and impacts
run native shared-pool/combat code. The original world regression passes28,720
ROM comparisons plus its native pickup/jump/H cases, and the existing projectile
integration remains37,132 assertions/3,270 frames on16 maps.

MSVC's `ssa_native_gunrilla_check` agrees on every focused counter. The game and
all106 libraries rebuild; PC/reference180-frame hidden smoke launches pass.
See gunrilla-validation.json, windows-gunrilla-validation.json and the refreshed
world/projectile reports. Full campaign completions were not rerun for this fix.

## Existing integration contracts

The playable composition enables `ballistic_player_prion` in addition to the
existing rocket option. Player type69 detaches at commit, and its native weapon
adapter removes the sideways muzzle displacement, restores the actual sight
heading after original no-target aim probes, and projects its192-unit forward
offset from live eye height along the aiming slope. It no longer follows later
player yaw/position. Default recovered runtimes leave the option zero, and the
original conversion/E2 logic remains unchanged. See weapon-fixes.md for launch
alignment and trajectory tests.

`projectile_runtime.c` connects weapon creation, projectile controllers and
motion to the native world, shared entity pool and combat adapter. It contains
no ROM instruction execution. Supported controller types are `65`, `66`, `67`,
`69` and `6B`; weapon creation currently routes the actual weapon 6, 7 and 10
requests (`65`, `69`, `67`). Attached conversion effects use the recovered `E2`
controller. Unsupported types/effects are errors at their integration boundary.

Initialize after `combat_runtime`, bind each existing weapon runtime with
`ssa_weapon_runtime_bind_projectiles`, then run individual projectile slots in
the original active pool order during the entity phase. The separate
`entity_runtime` now binds the native `04E04` scheduler to players, CD enemies,
projectile/E2 controllers and the owned-E1 timer wrapper. Other recognized
handlers still fail explicitly; this is not a complete game loop or every
enemy controller. A successful projectile step returns 0 while active or 1 when destroyed;
negative returns report invalid integration or callback failure. The frame order
remains entities before movers/VM before weapons; projectiles created in the
weapon phase first receive their entity update on the next frame.

Creation uses `10830`/`109F0` and inserts through `ssa_world_insert`. The source
and one-based player-owner fields are assigned after insertion, preserving ROM
ordering. The weapon controller subsequently commits its travel speed, owner
and weapon fields to the same stable allocation. In particular, near geometry
can set travel to zero, making the next appropriate controller update explode
without a motion call. Pool/world exhaustion returns a null allocation; the
existing weapon-controller contract treats a missing mandatory projectile as an
error. Failed insertion retains the allocated slot for explicit cleanup.

The adapter owns `projectiles[100]`, `effects[100]` and active flags but uses
`script_spawn.actors` and its existing 100-slot pool for bodies. It neither
duplicates active entities nor infers original slot order from world lists.
Actor storage stays stable. Allocation clears an unused payload, assigns its
activation entity pointer, and retains ownership until explicit or controller
release. Release unlinks, performs the original swap-last pool operation and
then clears both actor and projection. It sends no script group notification.

`activation.health`, `activation.timer`, `activation.state` and facing are
canonical between calls. The projectile projection retains speed, source,
owner and weapon. Animation IDs/periods and completion flags are mirrored to
the actor metadata; frames/counters reset on animation changes. This adapter
does not provide entity animation playback. Projectile and script mirrors
synchronize on callback failures as well as successful updates.

Motion composes actual `ssa_world_try_move`, vertical physics, the world's
blocking entity and a dedicated wall-reflection workspace. The type-67 wall
path unlinks, restores its frame-entry XYZ and velocity, reinserts and executes
the original three-ray reflection. These ray queries do not replace movement
contacts or the world's blocking entity. Detailed original behavior and
isolated comparisons are recorded in `projectile-motion.md`,
`projectile-bounce.md`, `projectile-create.md` and `projectile-controller.md`.

All updates run under the private combat scope, holding combat, all players and
script-spawn locks through callbacks. The weapon helper reuses the weapon's
already-held scope instead of attempting to lock twice. Damage source is the
projectile body and `source_owner` is its firing source: original `04E30`–`04E40`
stores each dispatched entity at `0300443C` before invoking its controller.
Credit and selected hit counters follow the motion/controller callbacks;
type-67 direct penetration leaves their incoming values unchanged while
explosions select the projectile owner and weapon. The runtime restores the
caller's shared credit/hit-counter/credited-kill pointers after its step.

Bind `hit_counts[player]` to ten persistent weapon counters and
`credited_kills[player]` to that shooter's persistent multiplayer byte. Keep
the weapon and counter storage alive until the projectile runtime is freed.
Damage uses canonical player inventory health and actor activation health.
Sound/network requests use the existing combat presentation callback; particles
use the required projectile callback. These observers must not mutate/reenter
world, pool, actor, player or adapter state. Earlier damage and effects remain
if a later presentation callback fails. Destruction is deferred until the
controller returns, keeping its source body valid during every callback.

Conversion sets target body flag `400` and `actor.parameter34=30` before attempting
the `E2` allocation, preserving `actor.parameter36`. Allocation exhaustion still
leaves the target converted. The effect
starts at target Z+height+16, with flag `20`, animation `5B` and period 5. Its
subsequent `113E4` updates unlink, follow target XYZ at Z+height+48, then reinsert;
target flag `20` requests its destruction instead. Flag `400` activates alternate
enemy targeting through shared navigation; it does not freeze movement.
`parameter34` is the health-drain period, and `parameter36` is its countdown.
The CD runtime now consumes and synchronizes these canonical actor fields,
including partial failures. Only CD has that complete native integration so
far. No freeze/thaw gate should be added to the dispatcher. The impact callback
name `freeze` is retained for compatibility; its documented behavior is
conversion. See `projectile-conversion.md` for the recovered instructions,
strict-negative drain cadence and native conversion-to-death integration.

Do not externally release/reuse bound projectile actors through script-spawn,
or release/reuse a converted target while an `E2` still follows it. The adapter
validates target pointer membership, current ownership and type before following;
the original host projection has no generation identity to detect a same-type
slot reuse. A violated ownership contract yields an error where detectable.
Free this adapter before weapon, combat, script-spawn and world teardown.

`test-projectile-runtime` runs the C integration fixture under ASan and UBSan
with `-no-pie` and `ASAN_OPTIONS=detect_leaks=0`. Its finalized counts and source
hashes are recorded in `native-projectile-runtime-validation.json`; the fixture
uses real map geometry, native weapon creation, flight, damage and shared-pool
ownership. Presentation remains observed callbacks and controlled enemy corpse
animation is not claimed as playback. Independent original-ROM comparisons
remain separate from native integration assertions and exact compiler matching.

The final PowerShell driver passes 37,132 assertions on all 16 maps, covering
3,270 projectile frames and 3,202 actual moves. There are ten verified wall
reflections (unchanged position, an intercept within range and changed velocity),
two penetration cases, a conversion effect following its target and releasing on
death, a full 99-slot allocation test with reuse, and two callback/allocation
failure scenarios. Direct cannon penetration checks both zero incoming credit
and a separate selected hit counter changing from41 to42 without replacement.

Two multiplayer rocket kills credit distinct shooter counters and preserve the
caller's borrowed counter. Four network requests are observed; these tests do
not apply remote packets or claim a network transport. Another 2,048 player
frames retain jumping (1,760 airborne) and H-view (1,536 third person). The older
weapon suite still passes 100,466 assertions and combat integration passes
26,435 assertions. At that validation checkpoint, all 54 native CMake libraries
built. The later entity scheduler and conversion integration have their own
validation reports.
