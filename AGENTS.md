# Project instructions

The objective is a native PC port of Serious Sam Advance with keyboard/mouse
controls, FPS movement, a new jump feature and an H-toggle third-person view.
The user explicitly rejected an interim emulated
playable build. Do not present a wrapper, mock game, or partial library as the
completed port.

Original feature and behavior parity is the user's acceptance criterion. Keep
all original content, gameplay, screens, progression and timing as the target,
with the user's explicitly requested PC controls/rendering additions. A prototype
or convenient host substitute is not proof of parity. In particular, pre-level
start music belongs to the original map view, never ongoing gameplay.

The user confirmed on 2026-10-14 that `orig/SSA.gba` is their own legally obtained
copy. That authorization persists; do not ask again. Keep the original ROM
unchanged. Its SHA-1 is `4ff48d8c18b70a5184758f1917343ae85a70858e`.

Use the requested decomp.wtf checkout in `tools/decomp-wtf`. Its upstream skill
is `plugins/decomp-wtf/skills/decompile/SKILL.md` within that checkout. The
workspace driver calls its actual engine/agent tools and byte matcher, without
requiring a globally installed MCP server:

```powershell
.\decomp.ps1 status
.\decomp.ps1 tool -ToolName claim_function -ToolArgs '{"function":"sub_08003028"}'
.\decomp.ps1 rebuild
.\decomp.ps1 test-input
```

Read `README.md` and `analysis/porting.md` before continuing. Use live status,
not old prose counts, to report progress. `decomp/project.json` names the active
project; `decomp/serious-sam-3/source/` currently holds the rebuildable export.
Its unchanged remainder is assembly/data, not recovered C. Discovery is
incomplete, so the imported-function denominator is not whole-game coverage.

The native source is `src/` plus `include/ssa/`. `recovered/` contains manually
reconstructed matching candidates; these are not necessarily exact matches.
Native behavior can be validated against isolated original ROM routines using
the differential tests. Unicorn is a test-only dependency, not a game runtime.

`rebuild` preserves the exported tree. `export` regenerates it from the database
and archives editable files first. Apply upstream fixes reproducibly through
`patches/` and `scripts/patch_luvdis.py`; do not hide failed checks or count a
near-match as matched. GBA builds use Ubuntu/WSL and the pinned local toolchains.

The pre-collision movement controls/vector phase is now ported and verified;
read `analysis/movement.md` and run `.\decomp.ps1 test-movement`. Native math
loads data tables from the user's ROM. It does not execute ROM instructions.
Vertical physics (0x08004CE0) and camera (0x080062D4) are ported and verified.
Read `analysis/physics.md`; `.\decomp.ps1 test-physics` runs 31,567 ROM
comparisons plus 36 native jump scenarios. Jumping is a requested new native
feature, not original ROM behavior. The PC/Win32 input adapter now maps Space;
physical-device delivery and a playable frontend remain unverified.
All 16 maps now decode natively with verified wall preprocessing. Read
`analysis/map-assets.md`; `test-maps` validates 7,632 wall records and 164
malformed cases. Native BSP/static collision and move commits pass 62,118 ROM
comparisons; read `analysis/geometry.md`, run `test-geometry`. It also verifies
16 original player starts and 32 real-map jump scenarios. Entity collision is
excluded, and original unlink/relink calls are only hooked in move tests.
Entity collision, host cell/sector lists, horizontal timestep (0x08004BDC) and
wall sliding (0x08004A38) are now native; read `analysis/world.md` and
`analysis/sliding.md`. Run `test-world`, `test-slide`, `test-horizontal`:
71,808 additional ROM comparisons, including 2,048 integrated horizontal frames.
Another 2,048 native frames combine movement, jump and H-camera updates.
Type-0xE1 dispatch 0x0801B99C is now native in pickup.c with inventory helpers;
script/audio/HUD/network/nearest/destruction remain explicit callbacks. See
analysis/pickups.md and analysis/inventory.md; test-pickup and test-inventory
pass 5,420 and 31,740 original-routine comparisons. World execute_special can
dispatch synchronously and remove only the current special entity. Removal
ENDS that cell's traversal (original entity clear zeros its next pointer).
test-world now has 28,720 ROM comparisons, including 1,024 controlled synchronous
release cases, plus 64 native world/pickup queries and reentry guards.
It now includes 2,048 complete 064B8 player-motion frames on real maps.
Original flag-0x200 entity collision uses X in its height test; preserve it.
Use separate world collision, slide and view workspaces. Entity storage and map
layout must stay stable while registered. See the ownership contracts.
The catalogue now has 526 functions / 87,648 bytes: validated 0x08003D7C was
added through split_function, not a speculative scan. 69 functions are exact,
covering 1,932 bytes (2.20% of the incomplete catalogue). Sprite transferE320
is the latest exact match (56 bytes); actor animation05394 (60 bytes), aimed projectile109F0 (48 bytes),
animation setter FC04, sector reject B18C
and CD dispatcher13978 preceded it.
All were verified through decomp.wtf compile_and_diff.
Map-object construction 08001984 is now native in spawn.c; read
analysis/spawning.md. test-spawn passes 256 whole-routine comparisons, 3,128
entity records, 728 markers and 370 guards. The construction phase leaves
dimensions/flags/health zero. Enemies initialize separately; ordinary E1 pickups
retain zero dimensions throughout original runtime. Do not treat
spawn-plan insertion as a complete playable-level initializer.
Weapon acquisition/selection 11588/11604/11FFC/12038/12050 is native in
weapon_select.c: test-weapon-select passes 29,176 ROM comparisons, 34 guards,
18 pickup-to-acquisition scenarios. See analysis/weapon-selection.md. Requested
weapon handoff and firing now exist; see the weapon runtime section below.
Player initialization 05E4C, complete movement 064B8 and frame wrapper 05CCC are
now native: player_spawn.c, player_motion.c, player_frame.c. Their test commands
test-player-spawn, test-player-motion, test-player-frame cover 688, 8,672, 11,656
ROM comparisons respectively. World suite covers another 2,048 full motion frames.
Read analysis/player-spawning.md, player-motion.md and player-frame.md.
The native player_runtime.c composes these with jump, full-precision relative yaw,
camera and H-view, with dedicated camera collision scratch. test-player-runtime
checks actual maps and external callback contracts; read analysis/player-runtime.md.
inventory.health and motion.movement.flags are runtime authoritative fields;
entity_health/vertical.flags are mirrors. Original GAME+68 motion/protection
index and 0300464E local-view index differ; never conflate them in adapters.
Shared entity timer 12CFC is native (test-entity-timer: 147,456 ROM comparisons);
its matching candidate remains 80.8%, not exact after 20 attempts. Pickup E1 wrapper
1B848 was already exact. See analysis/entity-timer.md.
Death/respawn 06154/05FEC are native in player_lifecycle.c: 10,494 ROM comparisons
of 85 known fields and ordered callbacks, 20 guards. Read analysis/player-lifecycle.md.
Respawn selector 01B18 is native in respawn_select.c: 4,998 whole-ROM comparisons;
preserve cap 10000, earliest tie, zero-XY skip, output Z=marker Y and signed wrap distance.
Hazards/interactions 05680/055BC/05548 plus 03464 sampler and 0429C contacts are native
in player_hazards.c: 24,576 ROM comparisons, 96 guards. Interaction queries continue
past solid walls; never substitute ordinary movement queries. Local flag 8 is the
display-local player's gate; weapon_action is record +10 value, not weapon ID.
player_gameplay.c connects these to runtime with native remove/select/insert,
dedicated interaction scratch and external script/presentation callbacks.
test-player-gameplay passes 211,701 assertions / 10,016 frames / 32 complete respawns on
16 maps with ASan+UBSan (-no-pie, leak scanning disabled). Jump-held latch and custom speed,
H-view mode and stable body ownership are preserved. Inventory health is authoritative.
Original respawn uses OLD floor before insertion; physics follows. Selector failure is
a native error instead of original uninitialized stack data. Failures may leave the body unlinked.
Read analysis/player-gameplay.md and player-hazards.md before changing adapters.
Moving sectors and activation are native in sector_movers.c: init 1BF14, commands,
1C0D4 activation, 1C13C height update and hazard list, 20,735 ROM comparisons.
Read analysis/sector-movers.md. Preserve strict endpoint comparisons, wrapped
16-bit heights, duplicate closing-door hazards and first-match/swap removal.
switches.c recovers 1C3B4/1C374/1C518 with 6,582 ROM comparisons. Texture names use
the original case-insensitive 8-character comparison. Native allocation zeroes
unspecified bytes; missing records/pairs are guarded. Activation latches before
queueing and audio; errors retain earlier effects. See analysis/switches.md.
level_script.c recovers 1C5A8/1C624/1C670/1C6B0/1C6DC with 17,116 ROM comparisons,
including 116 real script entries over 2,072 frames with controlled spawn/effects.
Five queue slots execute one opcode each per frame. Removal swaps last slot
and skips it until next frame. Opcode 9 repeatedly requests exit/toggles state.
Explicit alive_before preserves original group-zero underflow; fallback_z input
represents original undefined particle Z on allocation failure. See level-script.md.
level_runtime.c connects these with native player gameplay. Initialize before
entity/player construction. Run player steps first, then ONE level step per
world frame: movers BEFORE VM, established from 0244C. Scripts issued this frame
change mover flags after heights; line activation can affect same-frame heights.
The live hazard list is temporarily bound to player gameplay and restored.
Script allocation/animation metadata/insertion now have the script_spawn adapter
described below. Particles and outer exit handling remain presentation/runtime
boundaries; no silent no-op runtime implementation.
All 68 portable native CMake libraries build. Matching is 69/526 (1,932/87,648 bytes).
test-level-runtime passes 43,622 assertions over 4,128 player frames on all 16
maps with ASan+UBSan (-no-pie; leak scanning disabled). Includes 16 moving-sector
and closing-door deaths, 4 real switches, 12 tagged sectors, 27 failures and
126 callback reentry checks. Jump/H-view persist. The driver command also passes.
Read analysis/level-runtime.md before changing ordering or ownership.
entity_pool.c recovers 05238/05170/051C8 index bookkeeping: 36,697 complete ROM
comparisons and 25 guards. Release swaps active indices, preserves stable payload
storage and DOES NOT unlink world or notify groups. Caller clears payload after.
enemy_activation.c recovers all 12 CD..D8 state-zero initializers, including
boss helpers and counter/timer behavior: 14,576 ROM comparisons, 29 guards,
3,983 reentry checks and 7 callback failure paths. Only state 0 may use this API;
later AI is not implemented. Flag 80 expiration resets timer to 30 before activation.
script_spawn.c binds level runtime, world, pool and activation. Initialize after
initial entity construction; existing world entities reserve slots but remain
externally owned with fixed count. 100 total slots. Stable actors contain native
body, script fields, activation and animation metadata. Flush VM's post-insert
floor Z before particle callback. Defeat explicitly notifies group once; release
unlinks/clears without notification. Free before level/world; no query/reentry.
Read analysis/script-spawn.md, entity-pool.md and enemy-activation.md. New driver
commands: test-script-spawn, test-entity-pool, test-enemy-activation. Integration
covers all 116 original scripts/native world spawns; test-driven defeat/release is
not enemy AI or combat. Results and hashes: native-script-spawn-validation.json.
Final integration: 82,853 assertions, 3,139 script frames, 3,072 player frames,
1,983 frames of horizontal movement with an actor still registered on all 16
maps, 192 initializations, 32 blocked queries, 20 failures, 676 guards and 1,666
callback reentry checks. ASan+UBSan (-no-pie, detect_leaks=0); PowerShell driver
passes. Jump and H-view persist. Native source count is not exact-match progress.
enemy_motion.c recovers 04614/0456C/045C4/04498/0354C: 88,226 ROM comparisons,
4,096 native world-walk frames on 16 maps, 31 guards, 4,097 reentry checks.
Equality still decrements facing; walk runs vertical after blocked XY; distance
can be negative at wrapped 32768; 0354C changes only health. Counter is 03004358.
enemy_navigation.c recovers 1288C/12850 and 1271C/083F0: 10,725 ROM comparisons,
53,548 reentry checks, 31 guards, 8 failure paths. Phase 0 faces player body even
with an alternate target; retarget 600 wraps; phase 2 mask 1/2 detours use doubled
offsets; reset distance check is signed. Single ray A240/A041 remains a callback.
enemy_lifecycle.c recovers 12D5C/12D9C/12E1C: 15,031 ROM comparisons, 20 guards,
21,201 reentry checks, 2 failures. Group release BEFORE timer 360/720; corpse
fades at <=119 and destroys strictly <0. Drain is not general combat 037F4.
enemy_cd.c recovers 13978/13924/13868/137DC control flow: 7,278 ROM comparisons,
20 guards, 8,206 reentry checks, 7 failures with navigation controlled. Keep
player-record XY separate from player body. Animation event observes done byte
already reset. Sound loop starts at 1023/stops above 1024; its postlude still
runs after corpse destruction.
enemy_cd_runtime.c connects navigation/turn/world walk/gravity/hazards, animation
metadata, script groups and deferred pool release for newly scripted state-0 CD.
Refresh game.blocking_entity from world at frame entry AND after move: an early
visibility exit must observe the previous world query's blocker. 37E0 clears via
051C8. Destruction may release the actor even if its sound postlude fails; the
binding becomes inactive. Do not externally release/reuse/manually activate a
bound actor. There is no outer dispatcher here.
Final test-enemy-cd-runtime: 59,319 assertions, 1,959 controller frames, 1,583 chase
frames, 1,205 actual enemy moves, 2,048 player jump/H frames, 36 corpse cycles,
37 releases, 16 reuses, 84 guards, 4 failures and 5,019 reentry checks.
ASan+UBSan, -no-pie, detect_leaks=0; driver passes. Visibility/nearest/ray clearance
are controlled in this test, not native world sight. Read analysis/enemy-cd.md,
enemy-motion.md, enemy-navigation.md and enemy-lifecycle.md.
New drivers: test-enemy-motion, test-enemy-navigation, test-enemy-lifecycle,
test-enemy-cd and test-enemy-cd-runtime. Matching candidate: recovered/enemy_cd_dispatch.c.
Concrete next paths:
other enemy controllers and the outer dispatcher,
remaining projectile types and initial weapon loadout,
multiplayer pickup respawn 1B854. Enemy dispatch/initializer addresses are mapped
at the end of analysis/spawning.md.
Next substantive work is enemy AI, scripted gameplay effects,
combat, renderer/textures, game loop and OS input. Preserve scope limits in reports.
There is no playable native executable yet.

The enemy query backend is now native: enemy_targeting.c (036C4/03758/B18C),
ray_traversal.c (A240 walls/9C00), ray_intercepts.c (975C/9A88) and enemy_ray.c
(9C98/A040 plus pipeline). 263,969 additional ROM comparisons pass, including
all 16 maps/every sector pair and 8,192 complete wall rays with live heights.
9C98 sets max floor AND min floor, not entity floor. A040 tests their signed
wrapped absolute delta <=16 units; INT_MIN wrapped abs passes. One-sided 9C98
clears only range; one-sided A040 changes no opening globals. The 64-cell cap
and stable intercept ties are retained. Read analysis/enemy-ray.md,
ray-traversal.md, ray-intercepts.md and enemy-targeting.md.
enemy_queries.c composes these with script_spawn and native 1271C for CD runtime.
Pass &queries.ops as runtimeops.queries. An explicit reserved body array maps
original slots; copy the array, retain body ownership. Do not infer active order
from world lists. Nearest rebuilds the active prefix after swap release/reuse.
Dedicated ray scratch must not overwrite world.blocking_entity or movement/
camera workspaces. Queries allow spawn.updating during controller steps, reject
world/level/nested query reentry. Init/free outside updates, before world teardown;
stable address required. Read analysis/enemy-queries.md.
test-enemy-queries: 53,384 ASan+UBSan assertions, 2,753 controller frames,
2,603 chase frames, 1,774 moves, 2,593 walks, 4,096 player frames, 3,376 airborne,
1,536 third-person, 16 corpse releases. All 16 maps; -no-pie, detect_leaks=0;
PowerShell driver passes. No combat/audio/animation playback. Earlier controlled
CD test remains. New drivers: test-enemy-targeting, test-ray-traversal,
test-ray-intercepts, test-enemy-ray, test-enemy-queries. B18C matches exactly:
34 bytes/17 instructions in recovered/sector_reject.c. Native behavioral
recovery is not matching progress.

Combat is now native: combat_damage.c (037F4), combat_explosion.c (035A8),
combat_rewards.c (125E0). 155,517 new ROM comparisons including 4,096 combined
explosion/damage/reward chains on all 16 maps. Read analysis/combat-damage.md,
combat-explosion.md, combat-rewards.md, combat-combined.md and combat-runtime.md.
037F4 increments the selected shot-hit count BEFORE flag1 rejection. Preserve
difficulty scaling, wrapped signed halfword armor C6 THEN C4, timer/flash ordering,
source vs source_owner, credit and local-index rules. Movement.flags (+BC) is
NOT entity.body.flags (+30). Player inventory.health is canonical; entity_health
is its mirror. Single-player hits set body flag1, even survivors; clear0800 last.
035A8 uses strict unsigned squared-distance radius, no visibility for nonzero
radius; zero radius uses reject and type<=D6. Source67/player quartering changes
damage for subsequent targets, even when that player lies outside radius.
Current damage source can differ from explosion origin. 125E0 adds2000 only for
CD..D6, wraps score, creates no entities. It is outside the current catalogue;
no speculative split or exact match was added this pass.
combat_runtime.c binds native damage to script actors/player runtimes, explicit
reserved slot array and indexed players. It owns no entities. Other reserved
bodies have no health projection: requests to damage them fail. Bind initial
map enemies separately in future. It restores source/owner/localflags after
calls and synchronizes health/script mirrors on failure too. Player and spawn
locks prevent documented reentry; direct world/level mutation remains prohibited
by the callback contract, not universally guarded by every API.
ssa_combat_cd_step consumes contact/explosion events, forwards presentation.
Enemy health changes now; its next AI update releases groups; corpse expiry
later unlinks/releases. Preserve active pool order through synchronous effects.
Network events are requests only; received NULL-source hits can apply locally.
Frame/credit/difficulty/hurt_flash and score remain explicit shared state.
test-combat-runtime: 26,435 assertions, 130 controller frames, 32 chain deaths,
16 player deaths, 512 death frames, 2,560 movement/jump/H frames, 681 guards,
3 failure paths. Tests cover all16maps and two-player packet routing. ASan+UBSan
with -no-pie,detect_leaks=0; PowerShell driver passes. All43libraries build.
Drivers: test-combat-damage, test-combat-explosion, test-combat-rewards,
test-combat-combined, test-combat-runtime. Weapon firing follows below;
remaining projectile types, rendering, audio playback, OS input and a playable native
executable still remain.

Weapon animation (FC04/116BC/E460), all ten weapon controllers (116E4), switching
(12160), cooldowns (12200), auto-aim (A4F4) and hitscan (A444/9DE4) are native.
303,556 additional ROM comparisons pass: 275,520 animation, 11,652 controller,
8,192 aim and 8,192 hitscan. Read these documents under analysis/:
weapon-animation.md, weapon-controller.md, weapon-aim.md and weapon-hit.md.
The animation loader copies 233 sequences (528 words) as data. Tick after weapon
controllers: FFFF loops with done=1, FFFE holds with period=FF, FFFA..FFFD set
the completion marker. Elapsed time wraps to a byte before threshold checks.
Aim uses full player heading; A444 ignores its heading argument and uses the
body facing byte shifted left eight. Preserve the shotgun's repeated direction.
Flags-3 traces collect walls THEN block-list entities per cell, with stable ties.
Use the SAME dedicated context for aim/hitscan: shots preserve stale top/bottom
aperture and opening. Dead or zero-damage entities still stop shots; nonzero
health/damage emit impact9D4C BEFORE damage37F4. Wall impacts occur regardless of
damage. Release the world query lock before effects; preserve blocking_entity.

weapon_runtime.c binds an existing player_gameplay loadout to combat, aim/hitscan,
cooldowns, near-wall lowering, selection, firing and lower/commit/raise stages.
Single-runtime init does not grant a loadout; use ssa_weapon_runtimes_start for
the recovered1240C startup (see below). Original 0244C frame order is:
entities, movers, VM, pickup respawn,0908,F85C sprite controllers,E460 animation,
render. F85C type2 calls F150 then122AC. Run weapons AFTER movers/VM; animate
after all weapons. F150 skips visibility and firing when GAME bit1 is paused.
Otherwise object.flags bit1 follows NOT display-local BC bit8 BEFORE122AC's
input-state3 early return. Held/pressed are original A4/A6 (fire bit1), distinct
from movement BC flags. Heading comes from motion.movement.heading. Object Y
mirrors weapon_action and the respawn view. Gameplay's WEAPON_ANIMATION callback
must call ssa_weapon_runtime_respawn: permitted inside player update, forbidden
inside weapon update. It copies the view then sets animation. Near-wall12230
uses dedicated0429C contacts (64 units along facing), skips flag2000 and sets
flag100 for line kind2. It does not activate scripts.

Private combat_runtime_internal.h composition holds combat, all player and spawn
locks through callbacks and restores locks on errors. Impact precedes damage.
Callbacks may observe but cannot mutate world/entities/ownership. Unsupported
projectile/effect creation must return -1, never silently succeed. Earlier
effects persist on failure. Hit stats use the current weapon BEFORE selection;
credit and hit-stat pointer clear on exit; source stays this player, as in ROM.
Multiplayer requires weaponruntime.credited_kills bound to that shooter's
persistent byte; the shared damage counter pointer restores on exit.
Inventory.health and actor.activation.health remain canonical; mirrors also
synchronize on errors. Remaining projectile types, rendering, audio, OS input and outer
enemy controllers/game loop remain. There is no playable native executable.
All 68 portable native CMake libraries build. FC04 matches exactly (56 bytes) in
recovered/weapon_animation_set.c, verified through actual decomp.wtf. Drivers:
test-weapon-animation, test-weapon-controller, test-weapon-aim, test-weapon-hit,
test-weapon-runtime. See analysis/weapon-runtime.md for integration evidence.
Current integration: 100,466 ASan+UBSan assertions, 7,888 single-player weapon
frames, all five hitscan weapons on all 16 maps, 16 direct shotgun kills and
64 separate CD proximity deaths, 80 corpse releases, 2,048 jump/H frames,
16 respawn-helper checks and genuine two-player credit/automatic respawn.
Sound/impact callbacks observe effects; enemy corpse animation completion is
test-controlled. Projectile6/7/10 requests fail explicitly in this fixture.
PowerShell test-weapon-runtime passes; original combat integration still passes.
Cooldown12200 matching stopped after16 manual attempts at98.7%, NOT exact;
claim released with notes. FC04 export66 functions rebuilds the full ROM exactly.

Projectiles now have native construction 10830/109F0, motion 10A20, controllers
65/66/67/69/6B, wall reflection 4718/468C and attached E2 effect 113E4. Independent
ROM comparisons total 136,192: 69,632 creation, 16,384 motion, 25,600 controllers,
16,384 bounce and 8,192 attached effects. Read analysis/projectile-create.md,
projectile-motion.md, projectile-controller.md, projectile-bounce.md,
projectile-effect.md and projectile-runtime.md. New drivers: test-projectile-create,
test-projectile-motion, test-projectile-controller, test-projectile-bounce,
test-projectile-effect and test-projectile-runtime. All 68 portable native libraries build.
Aimed constructor 109F0 matches 48 bytes through actual decomp.wtf. E2 candidate
recovered/projectile_effect_follow.c remains 97.9%, NOT exact after three manual
attempts; claim released with notes (mask constant/load scheduling differs).

projectile_runtime uses the SAME script_spawn 100-slot pool and actor bodies,
with separate stable projectile/effect projections. Weapon runtime's optional
binding consumes CREATE/COMMIT for weapons 6/7/10 natively. CREATE's fourth
argument is Z offset, not speed; speed comes from type. Native creation inserts
BEFORE assigning source/owner; final sync maps owner to entity.player_index.
Keep owner weapon counters alive; free projectile runtime BEFORE weapons,
combat, spawn and world. Do not externally release/reuse bound projectiles or
converted targets while E2 follows. Failure may leave an allocated unlinked slot;
explicit runtime release/free cleans it. Destruction defers until the controller
returns. Projectile release sends no group defeat notification.

Guided flight uses full player heading and wrapped timer*speed; timeout is signed
timer>480. Type-67 wall failure restores XYZ/velocities before three wall rays;
the strict floor comparison in468C matters. Type69 sets target flag400 and
actor.parameter34=30. Optional E2 starts at Z+height+16 then follows at +48;
target flag20 destroys E2. Type69 CONVERTS enemies; the earlier freeze/thaw
interpretation was incorrect. Flag400 chooses other enemy targets and +34/+36
are periodic health drain, now canonical actor fields synchronized by CD runtime.
Do not implement an invented freeze gate. Read projectile-conversion.md. Type68 spread,
6A splitting, animation image decoding and complete game/platform integration remain.
The new runtime uses real world/combat under the private combat scope; particle/
audio callbacks remain presentation boundaries. No playable native executable
exists yet. See native-projectile-runtime-validation.json for current counts/hashes.
Final PowerShell projectile integration passes 37,132 ASan/UBSan assertions on
all 16 maps: 3,270 projectile frames, 3,202 moves, ten verified wall reflections,
two penetrations (including preserved incoming credit/counter), conversion/E2 follow
and death release, full 99-slot pool/reuse, 222 guards and two failure scenarios.
Two multiplayer rocket kills use distinct shooter counters; four network requests
are observed, not delivered. Jump/H remain covered by 2,048 player frames.
The original weapon suite still passes 100,466 assertions and combat passes
26,435. Exporter's C-only external-symbol fix lives in
patches/decomp-wtf-c-externals.patch, applied by scripts/bootstrap.sh after the
Thumb-symbol patch. Its focused test uses real ARM ELF relocations and checks
patch reproducibility, data symbols and no added interworking veneers.
Export and independent rebuild now pass with all 67 matched functions in C and
the unchanged ROM SHA-1. See analysis/rebuild-validation.json and
analysis/exporter-c-externals.md. Live status: 481 compiler attempts; 67/526
functions, 1,816/87,648 catalogued bytes (2.07%).

Native entity_dispatch now recovers04E04 and083E4:72,420 ROM comparisons,
415,927 callback reentry checks. Live active_count is read each iteration;
swap-release skips the moved slot until next frame, appended slots may run now.
Auxiliary output stores the NEXT physical index, including explicit active_tail
at capacity100; current returns to slot0. Do not simplify these original quirks.
Read analysis/entity-dispatch.md and entity-runtime.md. entity_runtime connects
player level gameplay, CD native queries/combat, projectiles and owned E1 timers.
It temporarily rebinds CD walk_count and restores it on unbind/free/destruction.
Run before movers/VM; weapon controllers and animation belong to later stages.
Known unsupported original handlers return errors; original table no-ops remain.
Observer callbacks cannot mutate/reenter; individual controllers own their locks.
Free entity_runtime before any dependencies. Validation:85,982 assertions,
2,062 entity phases, 2,048 jump/H frames across16maps. No complete game loop.

weapon_loadout recovers1240C (12,979 whole-ROM comparisons). It initializes only
players with BCbit4; setting0 retains ammo2..10, settings1..36 use signed table
values/caps, god gives all weapons/ammo999. Only cooldown[player_index] clears.
ssa_weapon_runtimes_start binds this to existing gameplay/combat using embedded
weapon objects, not the original sprite pool. Partial allocation failure retains
earlier players and bindings; free bound runtimes before retry. Inactive players
remain untouched. See analysis/weapon-loadout.md and weapon-start.md. Validation:
84,077 assertions,608starts,5,057weaponframes,192initialweaponshots on16maps.
New drivers:test-entity-dispatch,test-entity-runtime,test-weapon-loadout,
test-weapon-start,test-projectile-conversion. Conversion test passes686assertions:
49CDframes,48moves,2retargets,2drains,45E2frames andcallback-error field sync.
Canonical actor.parameter34/36 mirror before callbacks and on all step exits.
Final regressions:CD59,319,queries53,384,combat26,435,weapons100,466,
projectiles37,132. No new exact match this pass; live matching stays67/526,
1,816/87,648bytes(2.07%),481attempts. Existing67-C full-ROM export stays exact.

Latest continuation: frame0244C and spriteF85C (including101C4/101D0) pass
10,500 and70,344 whole-ROM comparisons. Read analysis/frame.md and
analysis/sprite-dispatch.md. GAME+0 increments even while paused; processed
input copies after polling. Elapsed time wraps32, then unsigned-clamps57E04.
Pause bit1 is checked ONCE after05898/021E4. Reached order: entities04E04,
movers1C13C, VM1C6DC, pickup respawn1B854, actor animation00908. Sprite
controllersF85C and animationE460 always run. Display-local BCbit8 gates both
renders; successful final latch B02=16. Failure retains prefix effects/no latch.

Sprite dispatch requires84 distinct stable object pointers and u32 active
indices. Logical index always advances; physical cursor advances ONLY when
post-handler current.type survives, after queue append. submitted[] stores
LOW BYTE of object.active_index(+0A), not slot. Reset count, preserve stale
queue bytes beyond the prefix. Current returns to objects0/state3 on exit.
101D0 is native; submit is an optional observer after append. Other typed
controllers require actual implementation or error. No sprite allocator claim.

Actor00908/05394 uses entity+58..5F, separate from spriteE460. Original142
sequences load from08449D30/08449728; 218,240 ROM comparisons pass. MarkerFFFF
loops with doneFF; FFFE holds with doneFE/periodFF; FFFA..FFFD are events.
Flag800 skips every field. Owned actors project automatically; reserved bodies
require stable bound states and forwarded setters. SsaScriptActor now has
animation_display_frame. Integration passes75,088 ASan/UBSan assertions on
all16maps:32 natural CD death-animation completions(tick66),32 corpse releases,
16 reuses and2048 jump/H frames, without manual completion writes. Read
analysis/actor-animation.md. 05394 matches60 bytes through actual decomp.wtf,
source recovered/actor_animation_set.c. Frame0244C reached94.1% after8 attempts,
NOT exact; claim released. No exact claim for00908/F85C.

Pickup1B854/05330 passes73,734 ROM comparisons. Only multiplayer levels12..15
operate. Elapsed wraps16 and uses strict>300; indices advance BEFORE occupancy
or allocation. Any active body's whole XY blocks, irrespective of Z/type/flags.
Native pool integration passes6,314 assertions on all4maps/all71 D9 markers,
all16 schedule entries, full pool/reuse/hidden E1 timer. Particle failure retains
the inserted actor. Fallback Z explicitly represents the original address8 read.
Adapters borrow combat/plan/data; spawned actors remain owned by script_spawn
on adapter free. Read analysis/pickup-respawn.md.

game_runtime composes frame, entities, separate mover/VM stages, pickup, actor
animation, sprite controllers and sprite animation. Do not run the new
level_runtime_movers_step/script_spawn_scripts_step AND their combined step in
the same frame. It auto-binds newly owned state-zero CD before the first entity
phase. Each bound weapon object must appear once among84 stable sprite pointers.
F150 runs natively in sprite order; other handlers need explicit callbacks.
POLL/DISTRIBUTE/CONTROL/RENDER remain external. No playable executable, renderer,
platform backend or complete level startup is implemented. Free game runtime
before its dependencies. All64 native CMake libraries build.

Shared globals: game_runtime.life_game holds message/input/continue/exit state.
entity_runtime.shared_life_game imports/exports each player and mirrors before
and after presentation callbacks, including failures. VMopcode9 writes03004304,
the SAME input+14 field; its imported/exported state gates ALL F150s this frame.
player_gameplay.display_flags is an optional live display-local BC pointer read
at the hazard stage. Entity runtime binds/restores it, so local respawn updates
later remote hazards in the same phase. game_runtime.cd_game is the canonical
shared loop/player/blocker. Existing external loop_source is inherited; CD
updates temporarily bind it, mirror old views before presentation/after stepping,
and restore pointers. Prior shared bindings restore on free. GAME counter,
ticks and flags drive combat/hazards/weapons, separately from04E04's0461C counter.

Combined integration passes997,410 assertions:8220 single/extra frame calls,
132 genuine two-player calls,16 automatic CD death/completion/release/VM chains,
16 startup weapon10 projectiles proven first updated NEXT frame,2048 jump/H
frames,21 failure prefixes,362,922 reentry checks and51 dependency guards.
See analysis/game-runtime.md and native-game-runtime-validation.json.
Drivers: test-frame, test-sprite-dispatch, test-actor-animation,
test-actor-animation-runtime, test-pickup-respawn, test-pickup-respawn-runtime,
test-game-runtime. That frame continuation reached68/526,1,876/87,648 bytes
(2.14%),490 compiler attempts; its68-C export rebuilt the ROM byte for byte.

The new camera extension is `src/view.c` / `include/ssa/view.h`: H event routing,
first-person default, third-person chase pose, static obstruction retraction,
jump following and close-wall eye-view fallback. Read `analysis/view.md` and
run `test-view` (10,442 assertions, 2,048 real-map camera frames). It uses a
dedicated collision workspace; never reuse a gameplay query's pending contacts.
The Win32 backend now routes keyboard/focus/raw mouse to the PC adapter. No
renderer or animated player body is implemented. The toggle is a native extension.

Latest PC continuation: pc_input.c accumulates WASD (opposing keys cancel),
Space, H, E and relative horizontal mouse counts. Mouse-left uses distinct
held/pressed weapon bits and retains a short click as a one-frame held pulse.
Space taps likewise persist until sampled; H pairs reduce to toggle parity.
E uses movement edge10000, separate from weapon fire. No vertical mouse look.
Sensitivity>0 in65536 units/turn; yaw accumulation wraps32 using unsigned math,
then native heading wraps16. Normal held-key repeats retain state; isolated
action repeats cannot synthesize a fresh press. Focus/disable clears pending
actions and blocks held action keys until release. Held movement may resume.

pc_input_runtime borrows a bound GameRuntime and selected GAME+68 recipient,
independent of display-local index. Events run outside frames or external POLL.
POLL samples once; DISTRIBUTE uses native05898 local branch then applies only
selected PC keys/yaw/jump/fire/view. Inactive BCbit4 players retain ALL inputs.
GameRuntime.step_serial is NEW native bookkeeping, NOT an original counter;
both functions reject duplicate/stale samples and permit a new poll after a
failed frame before GAMEcounter++. Focus loss/disable after sampling cancels
cached actions immediately, including regain before distribution. Mode3/pause
consume edges. Read analysis/pc-input.md. Validation:314,726 ASan/UBSan assertions,
2504 single-player frames/all16maps,65 two-player bypass frames,17 shots,
33 gate checks,35,071 guards,96,504 reentry checks. Tests cover all three reviewed
bugs: inactive gating, stale poll serial, cached focus/disable actions.

Original input_dispatch.c recovers the local/bypass05898 path05C26..05C5A only:
player_count<=1 OR GAMEflags100 OR exit_flag!=0. Scans ALL FOUR player slots;
writes packed PROCESSED03003F84 only when BCbit4 and slot==GAME+68. Other
records stay unchanged. No edge synthesis/display-local/input-mode reads here.
183,988 complete ROM comparisons and19,788 separately labeled network-entry
checks pass. Genuine network path returnsNETWORK_REQUIRED(-2) before mutation;
no transport, correction, packet delivery or fake remote input. PC adapter
retains that error in last_error when outer frame returns-1. Multiplayer tests
explicitly use original flag100 bypass; do not present those as network gameplay.

platform/win32/input_win32 attaches to caller HWND/current thread, foreground
raw mouse usage1/2, flags0; WM_KEYDOWN/UP and focus messages for keyboard.
Legacy mouse messages are NOT fed again. WM_INPUT reads GetRawInputData then
DefWindowProc for cleanup. No hooks/subclassing/capture/clipping/warping; prior
process mouse registration restores, a newer owner is preserved. Pump outside
updates or external POLL. No event pumping during simulation/presentation phases.
Windows test uses hidden message-only windows and explicit raw payloads:
3215 assertions,23 routed messages,1035 payloads; no physical-device or successful
HRAWINPUT/gameframe test. Read analysis/win32-input.md for Microsoft contracts.
Build/tests use actual MSVC x64 C11, /W4 /WX for backend/test. Driver
test-win32-input runs PowerShell directly, not WSL; its process policy is temporary.

All68 portable modules +ssa_input_win32 build as69 Windows x64 static libraries
in build/native-windows/Release. scripts/build-windows.ps1 discovers existing
VS2022BuildTools/CMake and installs nothing; -TestInput also runs backend tests.
Source hashes are in analysis/windows-native-build-validation.json. Linux
libraries and sanitizers still pass. No playable Windows executable exists yet.

sprite_assets decodes311 AGB! resources at0844B710 and palettes from0879EFFC,
08454A5C/08454C5C. There are1261 OAM pieces/6730 8bpp tiles/430720 pixel bytes.
All copied storage is owned, atomic malformed/allocation failure, free before
reuse; AGB* is explicitly unsupported (none in this table). Static indexed
raster, RGB555->RGBA convenience; no runtime projection/flip/scale/blend/cache.
622 originalDF80/E320/E358 chains,311 FCE4 calls/1261 descriptors,32 palette
setups,2183 malformed cases and2488 ASan/UBSan assertions pass. Real previews
in build/native-tests/sprite-000.png etc are ROM-derived/private build artifacts.
E320 matches56 bytes through actualdecomp.wtf, recovered/sprite_transfer.c.
Read analysis/sprite-assets.md. World texture archive leads: resource table
0844AEF4 indices16..21,101/121 named8bpp textures/three mip sizes; not yet ported.

CORRECTION: DBDC is sprite visibility/cache preparation (DBF4/DDA8/DEF8/DF80),
NOT the world renderer. FCE4 builds hardware OAM. New stage names
SSA_FRAME_PREPARE_SPRITES/BUILD_OAM are accurate; old RENDER_WORLD/RENDER_SPRITES
remain compatibility aliases at identical addresses. The actual world renderer,
audio, remaining AI/effects and full level startup still need recovery/porting.
New drivers:test-input-dispatch,test-pc-input,test-sprite-assets,test-win32-input.
Current exact matching69/526,1932/87648bytes(2.20%),491 compiler attempts.
Independent69-C export/rebuild remains byte-identical; report verified_at
1792014220.615682, unchanged SHA1. All latest PC/Win/build source hashes audited.

Latest world-asset/raster continuation supersedes those current counts:
70/526exactfunctions,1986/87648bytes(2.27%),497compilerattempts. B0C4 matched
54bytes/27instructions on4thmanual compile_and_diff; candidate
recovered/map_texture_bind.c. AFF4 reached99.2% after2manuals, NOTexact, released.
No active claims remain. 70-C export rebuilt byte-identically; independent
rebuild-validation.json should be checked for latest timestamp rather than
using the previous69-C timestamp above. OriginalROM SHA1 unchanged.
Latest independent rebuild verified_at1792015236.5598607:70C,byte-identical.

New portablemodules: texture_assets,map_texture_bindings,world_raster,raster_fill,
mode_control. All73portablelibraries +Win32input build as74MSVCx64libraries;
Linux CMake build also passes. Driver names:test-texture-assets,
test-map-texture-bindings,test-world-raster,test-raster-fill,test-mode-control.
Shareddriver/CMake/README/porting are wired; testfiles named correspondingly,
mapbinding/modecontrol/raster use *_rom.py. No playable executable exists.

texture_assets owns666images/2567040bytes acrosssixWOD0archives/2families/3mips.
Pointertable0844AEF4 indices16..21. Firstcase-insensitive8byte name match via
originalC57F4 table (ASCIIuppercaseonly;NULstops),AF18 missingmipfallsbackbase.
Columnmajor storage is pixels[x*height+y], proven55,440BE60C9DA..CA4A blocks.
84wholeAFF4,28wholeAF18,65539namecomparisons,2717malformed,2671guards,
4677ASan/UBSanassertions. Loadatomic ownsallstorage;rowmajorhelper preserves
index0 ordinarypixel. Loader cap1024images/dimension/16MiB; archive_find supports
fullu16 count for signedindexproof. Read analysis/texture-assets.md. Actual
Egyptiandoor/arch previews build/native-tests/texture-family*-000.png private.

map_texture_bindings owns mapdescriptor copies; borrowsunchangedmap/assets.
OriginalB0C4 invokesB0FC: matchednamecopieswidth/height, then signed16index;
missingreturn-1 becomesindex0 but incomingdimensionsremain. Hostlookup rejects
negativeindices. Live side+4/+6/+8 upper/middle/lower textures;FFFFabsent.
Nevercache sideids: switches mutatemiddle+6. 1059wholeB0C4calls (realB0FC/C57F4,
NOinstructionhooks),10910records,59778sidelookups,5124miplookups,160mutations,
324guards;59951ASan/UBSanassertions incl8actualswitchtexturechanges. Selected
families have185missingnames/854descriptors butNOwallreferences tothose.
Read analysis/map-texture-bindings.md. Sector+A/B areNOTtextureindices:
BA04->A888->AB10 WORLD+708->A98C nonzero=flatpalettecolor,zero=sky.

Actual world chain:02B88 displaylocalview+07C94clear ->B6B0 BSP ->017CCwalls
->BE60 clipping/projection/mip/UV ->ARM ROM002F4 relocated03000270. AB10/AA4C
closeplane spans->A98C. D2D0/01828 lateroperations unresolved. Worldframebuffer
120x120indexedbytes (07C94 E10words,BE60 clip119,stride120); automapdifferent.
world_raster implementsONLYcompleteARMcolumn: fraction/step wrap32,shift25..30
or32(defaulttexel0),count1..120. Signedstep<=signedfinalfraction overwriteslast
with framebufferpredecessor;count1 needsdest-120 live. Boundsatomic,aliasorder
preserved. 67678wholeARMcomparisons (actualselfmodifiedLDRB,Unicorncacheflush
afteroriginalSTR),7992useall666nativeloadedimages,1050aliascases,44guards.
NoROMexecution innativemodule. Read analysis/world-raster.md.

raster_fill implements ARM00338/RAM002B4 columnfillstride120,
00348/005AC rowfill,00358/005BC forwardskycopy. All6189wholeARMcomparisons,
819overlaps,all256colors,22guards. Zero counts rejected (originalwouldwrap).
Skycopy allowsaliaswithoriginalforwardpropagation,NOTmemmove. Read
analysis/raster-fill.md. Skydata0846285C/0846DC5C;A98C rowoffsettable0844AAEC,
AB10 headingoffset=(displaylocalplayer+B8)*5>>9. Coordinate/spangeneration
NOTported byfill primitives.

mode_control implementswhole021E4 standaloneborrowedscalars: flagsGAME+4,
inputstate03004304,menu_timerGAME+76,playercount+10,selection+72,player+A4
packedinputs. ScanslivecountwithoutBCactive/displaylocalfilter. UIcommand8EB0
(NOTsound) andspritecreateE914 mandatoryevents. Callbacksallowlivemutation;
bindingsstable,guardrejectsreentry,prefixeffectsretained. 13092wholeROMcalls,
60101reentrychecks,41failureprefixes,11guards. GameRuntime CONTROL stillexternal;
frontendmustbindcanonicalflags/life_game.input_state andimplementUIevents.
NoEscape/menuOSrenderingpolicy invented. Read analysis/mode-control.md.

Next actualnative scene work: B574/B6B0/B220/B428/B18C/B7B0 visibility inputs,
BE60 fullwallprojection/UV/mip/clipping,AB10/AA4C/A98C span generation/sky,
D2D0/01828 laterworldoperations,actorprojection/bodyimages,thencomposition/
presentation. Keepjump/Hpose/inputfunctional; renderingthirdpersonSam body,
audio,remainingAI/effects,fullinitiallevelactorhealth/setup remain.

Latest continuation supersedes that renderer TODO: world visibility, full solid/
portal walls and flatcolor/sky plane composition ARE NOW NATIVE. 72/526exact,
2052/87648bytes(2.34%),502compilerattempts. Newmatches B22042bytes/21insns
(firstmanual) andA87024bytes/12insns(secondmanual). Root02B88twoattempts72.1%
NOTexact, released; recovered/render_view.c andrender-view-match-attempts.json.
BE60claimnotesonly,released. Noactiveclaims/helpers/tool sessions remain.
72-Cexport+independentrebuild byte-identical; verified_at1792016849.264096.

Newmodules render_view,plane_renderer,world_visibility,wall_renderer,world_scene.
All78portablelibraries compileLinux;79Windowsx64libraries plus REAL native
geometryviewer executable buildMSVC C11Release. Viewer/Win32backend /W4 /WX.
platform/win32/viewer.c sourcehash864b628c...013772; authoritativefullhash in
analysis/native-viewer-validation.json. Currentrenderer/build/viewer hashes
audited. OriginalROM SHA1unchanged. NoROMembedded/copied/published inexe.

RUN:run-native-viewer.cmd [--level 0..15] launches
build/native-windows/Release/ssa_native_viewer.exe withorig/SSA.gba.
ViewerCLIrequiresROMpath, --level N optional, --smoke hidden, --snapshot BMP
optionalexplicitpath. Executableverifies8MiBAENP SHA1viaBCrypt. Console+visible
windowinteractive:clickcapture,WASDstrafe/move,relativehorizontalmouseyaw,
Spacejump,Heye/chasecamera,Escrelease. Noverticallook. Capturereleasesonfocus/
resize/systemmenu/cancel/close; GetCaptureverified,ClipCursorclientrect,
WM_SETCURSOR hidewithoutShowCursorcounter. ForegroundRawInput only, WM_INPUT
GetRawInputData+DefWindowProc; newvieweradapter isseparatefromunchanged
GameRuntime Win32backend, NEVERa fakeGameRuntimeprojection. Original120x120
geometrydisplayedsquareletterboxed;hardware scalingunrecovered,NOTguessed.
Actualsingleplayerspawn/motion/vertical/camera/jump/view APIs directly;
NOdummyDEATH/HAZARDS/fullgame stagecallbacks. Onlyplayerbodyregistered.
Explicit60Hz tick1 hostpolicy/catchup8. Renderererrorsstopwithdiagnostic.

ViewerisDIAGNOSTICGEOMETRY ONLY,notcompletedgame: noactor/enemyrendering,
combat/weapons/pickups/scripts/movers/HUD/audio/menuplay/Hbody. Hchanges
actualcamera; Sam'sbodydoesnotrender. Fullusergoalremainstheactualgame.
DoNOTreplacefullstartup/AI/simulation withtestfixtures or viewer assumptions.
Windows --smoke passes3840frames/16hiddenwindows/all16maps,1278airborne,
1600actualchaseframes,100selectedchaseframespermap;GDIStretchDIBits actual
hiddenHWND. Portableinputdirectfixtures,noSendInput/globalinjection/capture
inhiddenmode. FourCLIrejects pass:missingROM/level16exit2,missingfile/wrong
SHA1exit1. PhysicaldeviceandvisiblewindowinteractionNOTtested. All16BMPs
build/native-viewer-smoke/ visuallyinspected00/06realcourtyard/sky. Read
analysis/native-viewer.md. Newtest-native-viewer dispatchesPowerShellbeforeWSL;
scripts/test-native-viewer.ps1 buildsbydefault,-SkipBuildvalidatesrecordedhash.

render_view.h/c whole02B88 scansALL4BCactive4/automap8 slots, zoomA8held1/2,
unsigned32wrapthenmax65536/min4096. Clear/automap/world/overlaymandatoryevents.
Cameraoriginalsector+18 (NOT+E),negativeyaw94E8writes sine+10/cos+14.
Poseadapter copieseye/chasecoords/heading andfindsCAMERAsectorinactualmap.
84032wholeROMcomparisons(real94E8),206005reentry,5failures,6guards.
274785ASan/UBSanassertions/2048PCframes/all16maps,1751airborne/1464chase.
WORLDcallbackthatfixtureobservesONLYinput; actualpixelsseparatelytested.
Readanalysis/render-view.md;driver test-render-view.

plane_renderer ownsA844/A870/A888/A900pool80level2/64others,stable252byte
SsaPlane {height,min_x,max_x,color,top122,bottom122}; xstoredindexx+1.
AB10/AA4C/A98C fullspans,actualARMfill/copyoracleruns. 23955wholeROMcalls,
128actualsectorcasesall16maps,230583sanitizerassertions,24guards/1failureprefix.
Skywindows0846285C/0846DC5C each768*120bytes overlap; actualfirst60rows
each640pixels+128duplicatedprefix. Noheaderboundprovesindependent120rowimages;
copyentirereadablewindowasoriginaldoes. Headingoffset=(u16heading*5)>>9;
rowtable0844AAEC=row*768. PlaneheightdistanceusestheSIGNEDcameraZHIGHhalf
(original+0A),sectorappearance+A/Bcolor/sky,NOTtextures. Readplane-renderer.md.

world_visibility wholeB574/B6B0->B7B0/B818/BA04 withA888native buildsordered
100slotwallqueue,4096inclusiveclipranges,sector/vertexbits,planes. 218912ROM
calls:2048wholeB6B0,175904B818,8192B428,32768AC8/C10;21723walls/22028ranges,
1024liveheightframes. 206121ASan/UBSanassertions/4096frames/485guards incl
101stwall andplaneexhaustion. Legacyhigh_watertracksPREVIOUS wall_countpeak;
CURRENTslotsboundedbywall_count;head/nextonly (consumedunlinkedslotspossible).
Originalsolidclipcompactionretainsoldexclusive-endspareword; firstupper
sentinelterminatescoveragelist. Nativeboundsreplaceoriginalinvalidindexaccess.
Readworld-visibility.md;driver test-world-visibility.

wall_renderer completeBE60solidANDportals,actualperspective/mips/pegging/
planesplit/clipbytecopies. Reciprocal65536uint32 at08034934,near/fardither
0844AEDC/0844AEEC,closedcliparrays0844A9FC/0844AA74. Publicssa_wall_render
general0/-1,solidwrapperreturns-2forportalbeforeeffects. 14322wholeROMcases:
7207solid/7115portal,all10994realmapsegments,2048modifiedsector/flags cases,
239251originalARMcalls,9154splits,243048clipbytes,128guards. Source/destprefix
caseactuallevel7portal213 requiresframebuffer_offset120 in14520allocation;
noinventedwrap/blackfallback. Outinitialized,preservesunusedclipheights.
Pointerclipscarryscreenorigininsteadpointerbeforeallocation; originalcopy
lengthright-left EXCLUDESright. BE60DOESNOTrewritequeuedwall+20/+24 plane
handlesafterA900split; onlyworkingglobalschange,retainresultprojection.
Readwall-renderer.md;driver test-wall-renderer.

world_scene ownsvisibilityscratch,80planes,120clips,100results,24000openings,
14520framebuffer withprefix120. Borrowmapbinding+math/vis/wall/sky tables.
clearonlyactual14400pixels;prefixpreserved. DrawB6B0queues->017CChead/next,
00A84/E3Csecondarycoverage before flaggedwalls ->fullBE60 ->AB10. Rangescopy
avoidsdifferentstructtypealiasUB. Retainqueuedplanehandles,splitresultsseparate.
Failpreservesprefixeffects/clearsguard; innerupdatingguardsblockclear/free/draw.
Secondarycoverage7893wholeA84/40268wholeE3C compareallsparebytes,10guards.
WholeoriginalB6B0->017CC->BE60->AB10/AA4C/A98C pixels128frames/all16maps:
1843200pixels,1280walls/532portals,754planes,19259ARMcolumns; fullbuffers/
clips/pools/openingbytes/cursor/globalsmatch. ONLYBIOS/DMA+laterD2D0/01828
actorpassescontrolled. ThuswholeWORLDGEOMETRYproof,notactor/fullgameproof.
NativePC/gamefixtures render2048realframes/all16maps withjump/H/mouseyaw:
481049ASan/UBSanassertions,225guards,29482217nonzeropixels. ActualPNGs
build/native-tests/world-level-XX.png private,00/06visuallyinspected.
Readworld-scene.md;drivers test-scene-coverage,test-world-scene,test-world-scene-rom.

Latest actor continuation supersedes previous actor/body/viewer TODOs and counts:
73/526 exact functions,2084/87648 cataloguebytes(2.38%),508compilerattempts.
Original09238 wrapper matches32bytes/16instructions onfirstmanualcompile;
F98 bounded5attempts80.3% NOTexact, D490claimnotesonly; bothreleased.
73-Cexport+independentrebuild byte-identical; verified_at1792018042.744509.
No native library counts or screenshots imply the complete game is finished.

New six portable modules:map_actors,actor_assets,actor_raster,actor_queue,
actor_projection,actor_scene. All84portablelibraries buildLinux;85Windowsx64
libraries plus nativeviewer. Windowsviewer/MSVC C11Release /W4 /WX passes
3840hiddenframes/all16maps,1278airborne,1600actualvisibleSamframes,
18326actorcommands,2762120changedactorpixels,2160300Sammaskpixels. All385
mapobjectsloaded. FourCLI/ROMrejectspass. Physicaldevices/visiblewindow untested.
RUNrun-native-viewer.cmd [--level N]. CLI --snapshot-chase BMP requires --smoke
andwritesactualframe100. Privatechase00/06BMPsvisuallyinspected:Samoriginal
body/overlay andplacedobjects againstrealcourtyard/pyramid/walls/sky.

Viewer nowallocates originalmapobjects BEFOREplayer,worldcapacity100. MapActors
owns01984 plan/pool/00908animationstates; retains initialzerohealth/dimensions/
flags,NOinventedAI. Reads canonicalanimationstate,notinitialSpawnObjectmetadata.
Worldscene freshgeometrydraw ->ActorScene ->actualnative actorpixels ->GDI.
H ONLYclear80inrender-source snapshot whenpose.third_person; canonicalbodyflags
stay0x9A andfirstperson/fallbackhidden. Original65/66period4animation transitions
useplayer_frame predicates, D338flag10 adds5*(owner+1)overlay. Jumpmovesactual
bodyandactorprojection. No fakeGameRuntime orno-opgameplaystagecallbacks.
Viewer stillhasNOAI/combat/pickup execution/scripts/movers/HUD/audio/menuplay.
TeardownActorScene BEFOREgeometry,MapActorsfree BEFOREworld detachesONLYits
objects,keepsplayeralive; originalROM copiedassetsonly andneverembeddedinexe.

ActorAssets:142descriptorresources at08449D2C,5407imagereferences/2607unique,
7053920copiedreferencebytes/152440RLEcolumns,138nonemptyresources. Header32
bytes,origin+18/+1A,size+1C/+1E,u16coloffsets; highbitpackettransparent skip,
lowbitliteralcount,index0transparent. Exact09258+ARM0638 12bit sampler handles
mirrorbit31,also discardsbit30pointershift. 13787wholeROMdraws/1whole0890,
2617malformedloads/11atomicrasterguards/37860ASanassertions. Readactor-assets.md.
ActorProjection:D490billboard/D884fixed/D2D0ascendingvisiblesectors,D338double
playerpass,realmaplinkedorder. 17664isolatedROMcalls/256collections,9110records,
4608guards. AllocprojectionbeforeF98actor,originalprefixeffects retained.
ActorQueue:F98/1238+B24C/01828 mixedwall/actorlinks,clip/depth/reversedraw.
67840ROMcalls(33792allocate/32768insert/1024order/256actualpixelpasses),
8192realactor draws;36564ASanassertions/432guards/1024frames. Closedclipbuffers
need121bytes originaladjacenttail; dynamicclipsborrowremaining24000capacity,
notcopiedlength/cursor. Original1238right120/121scratchaliases andlast4clipwords
overlapnextallocation; explicit32*120*2+4backing preserves. Readactor-queue.md.

ActorScene rootcomposition testedagainstWHOLEoriginalB6B0 withNOactorhooks:
256views/all16maps,128native+original00908animatedframes,3686400pixels,
317actorcommands/2584walls(1088portals)/1600planes. Allprojection/actor/clip/
order/prefixpixels/wall/plane/openingstates match. ONLYBIOSdivide/CpuSet/DMA
modeled; originalmachinecode is UnicornTESTONLY, nativeexecontainsnone.
Original720byteopeningarea canoverflow(level6probe741bytes overwrotecursor);
equalityoracle usesboundedoriginalviews,nativeexplicit24000backing remains.
Wholegeometryoldproofexcludesactors; NEWactor-sceneproofincludesallactorpasses.
Readactor-projection.md/native-actor-scene-validation.json. MapActorsproof:
16whole01984/385entities/83markers/16grids,2048whole00908/49280records/119guards,
5404ASanassertions retain16separatelyspawnedplayersafterfree. Productionhashes
inreports; never reportstalehashesafterchanges. Newdrivercommands:test-map-actors,
test-actor-assets,test-actor-projection,test-actor-queue,test-actor-scene-rom,
test-actor-scene. ComponentROMs andfullsceneproof alreadyrun; don'trepeatunless
productionchangesinvalidateevidence.
FinalActorScene init/free guardfix checksALLinnergeometry locks; wholeROMproof
rerun matchesall3686400pixels,Windows3840framesstillpass. Standaloneintegration
test-actor-scene passes160522ASan/UBSanassertions/688guards/3840movementframes,
1278airborne/1600visibleSam,3872scenedraws/18661actorcommands/2888125changed
pixels. 116missingsourcefailures/100preservedqueueprefixes/64ownershipchecks;
canonicalflagsremainunchanged,cameraborrowsclear. Currentactor_scene.c SHA
5fc25cb0...9a17b2c. Finalproduction/header/test/buildhash audit evidence:
analysis/native-actor-source-audit.json. Allhelperstestsfinished,noactiveclaims.

Latest presentation continuation supersedes the weapon/HUD/OAM/scaling TODO:
74/526exact,2152/87648bytes(2.46%),517compilerattempts. NewFB5C markinghelper
68bytes/34instructions exactSECONDmanualcompile; recovered/sprite_mark.c.
FC3C2attempts83.9%,074F0 5attempts83.2% NOTexact; allclaimsreleased. RootDBF4
claimnotesonlyreleased. 74-Cexport byte-identical; independentrebuild report
analysis/rebuild-validation.json authoritative. Do notreportcatalogueasfullcode.

Sixnewmodules sprite_pool,hud_sprites,sprite_oam,sprite_cache,display,hud_scene.
All90portable/91Windowsx64libraries plusREALnativeviewer. HUDnowvisible with
originalstartingknife,healthbar/icons/glyphs and Hhidesfirstpersonweapon. Itis
DIAGNOSTICIDLE-only: nofire/combat/AI/pickupexecution/scripts/movers/audio/menuplay.
Score/time remaininitialized. Healthfromspawnedentity,armorfrominventory. No
fakeGameRuntime orno-opF150/122AC substitute. HUDscene.step_idle direct12160 is
explicitpartialstage, inputstate3 skipsraise. Nativecanonicalframeintegration
stillunfinished. Original120x120world no longerdisplayedasquare: recovered
normalmode1 affineBG sampling(x/2,floor3y/4) ->240x160,3:2letterboxedWin32DIB.
Capture/WASD/mouseyaw/Space/H/Esc/focuscleanupunchanged. Physicalinputuntested.

SpritePool borrows84stableobjectbindings inSpriteDispatch; actualFA78/FAE8reset,
F9B4allocate/FA08swaprelease/FB5Cmark/FBA0range. Hud101F4 actual74objects1player,
59+2*players multiplayer; F84Corder1240CweaponsFIRST thenHUD. glyphwidth128bytes
0844BC0C. Normaltypes4/5/6/7/8/9/C/E/F/10/11/12 andinactiveD recovernative;
activeDtextreturns0unsupporteduntouched. Numeric3/8/5scratchpersistent andonly
designatedsubtyperebuilds; boundoutsizenumbersinsteadoverwritingmemory. Ammo1/2
HUDwrites999 mustcopybackcanonicalammo. 77424wholeROMcalls(18576pool/480create/
40960scalar/16384numeric/1024transient),679guards/256ASanUBSancycles. Readsprite-pool.md.

SpriteOam fullFCE4 includingE648/FC3C/FC58. Usesprevious_frame NOTcurrentframe,
cache{resident,pending,tile64},16originalbuckettable0844BBEC,128raw8byteentries,
32affinematrices,localindex/counterbit2/mosaic. Linksnormalizedhostlogicaltokens
0=end,1..17nodes,80000000|(slot+1)objects; NEVERGBAnorhostpointers. 6036wholeROM
calls/45603entries/10054affineobjects,13183ASanassertions/4536frames. Readsprite-oam.md.

SpriteCache fullDBDC(DBF4/DDA8/DEF8/DF80),E0A8/E2ECallocator,E358normaluploads,
DAF0reset. 311actualcacheentries;50requestring/50blocks/50images/10transfers.
Extra2backingrecords preserveoriginalstaleE1CC/E2B0shiftreads. Free{160,512}
tile64 range intentionallybeyondphysicaltile511; uploads mirror32K intoOBJ.
OAMrawtile overflowleftunchanged (canchangepriority,unsupporteddisplaycases
explicit). Weaponreservedtile0; eightrequests/10000budget perDF80. Original
DEF8expiry skipsnextshiftedimage; E1CCleavesfinaladjacentfreepair. DoNOTreplace
withgenericmerge. DAF0keepsphysicalpixels/requestbytes/blocktails,resetcursors,
zerosfirst50usedimages/entries andallactivepreviousframes0. Initfreshzerosowned.
Safetyextension:FAE8previousFFFF ->index3FFF oldretention treatednoimage, not
originalout-of-boundsprobe beyond2000entrycache. Testseedsunrelatedoriginal
memory0explicitly,doesnotclaimarbitraryundefinedtailbehavior. OtherinvalidIDs
fail. 30596wholecalls:9952DBF4/2048DBDC/11147alloc/5337free/2048E358/64DAF0,
7769920uploadbytes,20624ASanassertions/2359frames/11guards. Readsprite-cache.md.

Display native240x160RGB555 mode1 usesDISPCNT1441 BG2CNT6085/6089 PA80/PD C0.
06E30/06F38 tilemap andARM264 transposition prove120x120 ->225column-ordered
8x8tiles at06004000/8000. HostRGB888replicates5bits; noemulatorruntime.
448wholeROMregister/map/transfercalls,635independentframes/311images/128affine,
418281ASanassertions/40guards/1308frames. Explicitunsupportedothermodes/layers,
blending/windows/mosaic/4bpp,decreasingOAMpriority andbudgetoverflow; decreasing
priority canarisefrom ORIGINALunmaskedtileoverflow,notjustmalformedinput.
AtomicfailurebeforeRGBwrites; readanalysis/display.mdwithprimaryhardwarelinks.

HudScene owns84objects+pool/cache/OAM/anim/state andborrowsinventory/body/assets/
math. Keepstableaddress. Initoriginal1240C+101F4. Stepidlesweapon12160BEFOREHUD
projection ofcurrentweapon/flags (sameframecommit),nativecontrollers/E460 then
cache/OAM/flush; drawdisplayseparate. Unsupportedactivenotice fails; nogameplay
stagefakecallbacks. WindowsnewHUDsmoke3840frames/all16maps,1600visibleSam,
3856HUDdraws(initialdraws included)/2112raisedfirstpersonweapondraws,
21807376changedpresentationpixels;4CLIrejects. Privatelevel00/chase00BMPs
visuallyinspected showrealknife/HUD/Sam. Launcherunchanged. Finalproofs in
native-hud-scene-integration-validation.json andnative-viewer-validation.json.
Drivers:test-sprite-cache,test-sprite-pool,test-sprite-oam,test-display,test-hud-scene.
Finalsameframeprojectionfix: HUDstate.current_weapon isPLAYER+78REQUESTED,
state.weapon_action isPLAYER+28COMMITTED. Bind bothAFTER12160; startingknife1/
weapon8 hidescrosshair nativeF794. Rootreadyguardsalso validatepairedweapon
pointerbelongsinlineobjects (NULL/foreignpairs mustfailbeforewrites).
Wholeoriginalhud_scene proofnow16F84Cinit+2048fullF85C/E460/DBDC/FCE4/E358frames,
172032objects/2097152OAMbytes/67108864OBJbytes plus64independentRGBreferences
allmatch. Inputstate3usesgenuineF150skipaim/fire branch; ONLYBIOS/DMA modeled,
unowned3FFFcacheprobezero-seededexplicit; submissionqueueoutsideadapterscope.
StandaloneASan187759assertions/464guards/3840realmapframes+32directedpending/
commitframes/32RGBrefs;1600Hhiddenweapon/visibleSam/48healthchanges. Final
sourcehud_scene.c a2a910c5...1eb03ef8;viewer5ed9dc88...c4d3958c,exe
b3fb59cb...68c46c. FinalWindows3840framesunchangedexceptpresentationpixels
21807376andnospuriousknifecrosshair. Privatelevel00/chase06BMPsvisuallyinspected.
Nativepresentationcurrenthashaudit306checksPASS:
analysis/native-presentation-source-audit.json supersedesoldactor-source-audit
forfrontend/buildbinary hashes. Independent74-Crebuild verified_at1792048310.092951.
Additionaldriver test-hud-scene-rom; readanalysis/hud-presentation.md.

Nextwork:fullremainingenemyhandlers/initialactivation andhealth,gameplayeffects,
activeHUDtextnotices/spritecontrollers/paletteblend,thenactualfullGameRuntime/
startup/mode/weaponfire/UI/audiointegration inWindowsfrontend. Keepviewerhonest
untilactualgamecomplete; doNOTshippreactivationobjects asworkingAI.


Current playable checkpoint (2026-10-15), supersedes earlier frontend TODOs:
play-native.cmd launches ssa_native_game.exe, a native first-level prototype.
Production NativeGame owns full frame/entities/combat/enemies/projectiles/
pickups/scripts/movers/weapons/HUD/renderer, retaining borrowed ROM untilfree.
Diagnosticviewer unchanged/separate. Frontend exposeslevel0only; latercampaign,
bosses, specialweapons, crosslevelcarry,originalmenus/paletteeffects/audio unfinished.
Muted soundbackend; hostpickup/death/exittext. Noemulator/runtimeROMexecution.
Normaldifficulty1,continue_flag3,inputstate1,originalsetting0loadout,actualpertype
speeds. Mapobjects FIRST thenplayer; adoptplan transfersbodiesintoownedspawnslots
whileworldreplace preserveslistorder. reserved=initialprefixhighwater, NOTminimum
livecount. Allruntimeschooseownedbeforeexternalprefix; release/reuseprefixlegal.
Externalplayerneverreleased. ActoranimationexternalstatesNULLallowedforowned.
Pickupitem=actor.animation(+58), NOTstate(+3E). Canonicalhealth=inventory/player
oractivation/enemy. Hbodyflagchangesrenderingsnapshotonly. Genericenemybind
inEntityRuntime handlesCE..D6; existingCDauto-binding stays. Particles96 and
65..6B/E2/E3 runtimeflight;type64portalrelocationexplicitunsupported.
Firstlevelnormalinputcompletion PASSES GCCASan/UBSan/leaks1 andMSVC x64,
identical5258frames/28000score/8pickups/19spawned/health100/13pistol23shotgunshots.
Botusesseparateaimscratch/privateplanningmap; NOhealth/ammo/body/scriptwrites,
forcedkills/forcedanimationdone. Originalgroups3/4unlockgate4;exitpickupqueues
script3opcode9. Ownership4817checks/all16; sharedgame997410checks/8220frames.
Newenemycontrol200948ASanassertions/18432frames; ranged11994ROM/180884ASan;
pickup10015ASan/287actualoriginalpickups; particles37120ROM+48321ASan.
Windows99libs plusgame/viewer/completioncheck. Readanalysis/native-game.md,
native-playability-validation.json andwindows-native-game-validation.json.
Newcommands inbothdrivers:test-native-playability,test-native-game,
test-map-gameplay-ownership,test-enemy-control,test-enemy-control-runtime,
test-enemy-ranged,test-pickup-runtime,test-particle-create,test-particle-runtime,
test-projectile-target,test-projectile-extended. Windowsfrontendsmoke script
scripts/test-native-game.ps1, actualphysicaldeviceinteraction stillunverified.
Final source/binary audit: analysis/native-game-source-audit.json,516checksPASS.
NativeGame integration600frames/390airborne/360thirdperson/1pickup/health68,
then120freshrestartframes/health100, ASan/UBSan/leaks1PASS. Windowsfrontend
600frames plusfull5258framecompletionPASS. Actualdecompstatus74/526,
2152/87648bytes=2.46%,520attempts. New14C58(90.9%,padding) and13438(89.7%)
attemptsNOTexact,claimsreleased; prior74-Cbyteidenticalexportremainscurrent.

Dynamic PC resolution checkpoint: platform/win32/game.c now defaults to
SsaPcRenderer (src/pc_renderer.c), following client-size changes automatically.
--resolution WIDTHxHEIGHT selects arbitrary fixed dimensions; auto restores
window sizing; --original-renderer retains the original 240x160 reference.
No preset resolution cap: checked INT_MAX/size_t spans and allocation limits.
Color/depth storage is dynamically owned, atomic resize, about8bytes/pixel.
This is a PC extension, not an exact replacement for original GBA raster rules.
True per-output-column rays traverse live map sectors/portals/door heights;
highest original texture/actor mip, original flat/sky data, per-pixel depth.
HUD/weapon original pixel art uses a transparent OAM layer in centered3:2 area.
Never describe this as merely upscaling the old world, or as infinite memory.
Read analysis/pc-renderer.md and new validation reports. Tests cover7sizes
through3840x2160 and8193x17, jumping/H, readonlygameplay, allocation guards.
PC renderer sampled87frames across unchanged5258framelevelcompletion.
Windows verifies liveauto1264x861->1008x729,custom4K,8193wide,originalfallback,
snapshotdimensions and4badstrings. CMake100libs. Newdrivercommands
test-pc-renderer,test-pc-renderer-playability; Windows test-render-resolution.ps1.

Chase-camera framing checkpoint: ssa_view_init now defaults to256units behind
and64above eye (previously128/24). Existing4unit swept collision and close-wall
firstperson fallback preserved. Updated test_view passes10442assertions,
2048jump/H poses across16maps, ASan/UBSan (non-PIE, leaks0). Windows rebuilt;
600frame frontend smoke and unchanged5258framelevelcompletion PASS. Snapshot
build/native-game-smoke/gameplay.bmp visually inspected. Relevant current
reports: native-view-validation.json and windows-native-game-validation.json.
Prior broad source audit/PC reports predate this camera-only tuning.

PC free-look checkpoint: captured mouseY now produces saturating pitch deltas;
player_runtime pitch clamps +/-14564 (80deg), normal mouse orientation. First
vertical input sets free_aim; controls/focus gates clear pending deltas. Default
zero pitch retains original autoaim until that input. Original renderer option
suppressesY and retains horizontal reference. Movement remains horizontal.
src/pc_renderer_pitch.inc uses world geometry, near clipping, perspective UV/
depth, directed BSP subsector polygons, live heights and upright actor billboards.
Do not replace pitch with horizon shifting. Zero pitch keeps column renderer;
pitched geometry path reports rays=0. Both retain dynamic resolutions/HUD art.
weapon_hit_query_free adds full yaw/eye/slope and floor/ceiling-before-intercept
checks; original query remains separately ROM tested. weapon_controller_step_free
suppresses adjacent-yaw retries without changing original controller ABI. Native
weapon adapter computes direct slope, muzzle point and PC aim reticle point;
chase reticle projects actual player aim (rendering does not query/mutate world).
New test-free-look in both drivers:214assertions +28000 independent depth
comparisons/40views (+/-80deg), ASan/UBSan/leaks1. Elevated target hit/misses,
floor rays, actual rocket muzzle/velocity checked (inventory fixture grant).
PC input304406assertions/2504frames/16mapsPASS; original hitscan8192 and
controller11652ROM comparisonsPASS. Windows rebuilt without compiler warnings;
600frame smoke and unchanged5258framecompletionPASS. Windows test-free-look.ps1
covers up, chase down,4K andoriginalfallback. See analysis/free-look.md and
free-look-validation.json/windows-free-look-validation.json. Prior broad audits
and original resolution/playability reports can predate these PC feature changes.

Original crosshair/performance checkpoint (supersedes custom PC reticle above):
Removed custom projected glyph and free-aim type18 suppression. Original HUD
art/position/target animation and knife/weapon8/death/automap rules restored.
Vertical mouse aim remains direct. pc_aim presentation fields removed.
NativeGame tick now advances simulation only; prepare_view builds camera/sources;
step retains tick+originaldraw for old callers. Windows uses tick then one view/
PC draw per batch, avoids unused original raster on every catch-up tick. Full
normal-input completion through split path still5258frames/28000score/100health;
87normal+87pitched1280x720frames along route, ASan/UBSan/leaks1PASS.
PC pitched renderer caches static floor topology via geometry fingerprint and
camera-local sky UV by pitch/resolution. Live sector heights/colors remain read
each draw. Sky yaw applied atsampling. Triangle raster uses scanline spans and
incremental reciprocal attributes; sparse HUD runs preserve exact art. x64 SSE2
sqrt has portable Newton fallback. Sky UV adds4bytes/renderpixel to8bytecolor/
depth; Windows adds client-size32bitDIB. Free releases geometry/UV too.
Windows present draws frame/bars/text into memoryDIB, GdiFlush, then ONE finished
StretchDIBits towindow. No visible FillRect black clear; original reference and
arbitrary-resolution modes preserved. Resize/reload/shutdown resource handling.
New test-render-performance.ps1:601bufferedpresents/1803pixelchecks/600frames,
liveauto1264x861->1008x729, jump/H/pitch. Physical display flicker not measured.
Local180frame1280x720Windows aiming workload6.730s->1.233s (5.46x throughput),
horizontal2.824s->2.447s. Actual game simulation stays60Hz, no universalFPSclaim.
GCC fixed-pitch CPU views34.9..70.1ms->3.85..7.88ms; continuous vertical sweep
14.0..16.2ms afteronly. Frozen baseline binaries are local build artifacts.
New free-look validation18510assertions +30100 independentdepth comparisons/
43views,18280 originalHUDpixels, liveheight/cachetests. Existing renderer11M+
checks/7sizesPASS. Windows8resolution/modes,4rejects,4free-lookmodes,600gameplay
and5258completionPASS. See analysis/render-performance.md and current reports.

Startup-crash correction: normal run() had rendered before the first native
tick, so zeroed initial OAM records failed display validation. Hidden smoke
already ticked before drawing and missed it. Normal startup AND reload now
disable input and execute one native bootstrap tick before first render.
Do not remove this ordering without explicitly initializing sprite/cache/OAM.
--startup-test exercises the NON-smoke path: actual raw-input registration,
SW_SHOWNOACTIVATE/UpdateWindow,120+ticks, one internal restart, autoexit. No
physical input injection. scripts/test-native-startup.ps1 runs PC/reference
modes through actual play-native.cmd from another cwd; also verifies error2
logging. test-native-game.ps1 now includes this regression (5258completion and
600smoke also PASS). Current proof windows-native-startup-validation.json.
Launcher stderr goes build/logs/last-launch.log and errors pause visibly;
SSA_LAUNCH_NO_PAUSE=1 suppresses pause for automation. Original ROM unchanged.
Only Windows frontend/launcher changed; earlier performance report binaries
predate this startup fix. Actual physical input/display quality remain separate.

Chase aim-alignment checkpoint: player_runtime now calls ssa_view_pose_pitch.
Rear offset rotates with aim pitch; world-up lift stays64, defaultdistance256.
Existing4unit clearance,one-unit sweep,retraction and close-wall fallback remain.
ssa_view_pose remains zero-pitch compatible. NativeGame owns dedicated camera_trace
(init/free withworld). ssa_weapon_hit_query_camera returns EXACT contact without
wall4unit/entity10% particle backoff; original/free weapon queries unchanged.
prepare_view traces actual weapon heading/height/slope/range, sets pc_pose target,
and ssa_view_angles computes pc_camera.heading/render_pitch convergence. No-hit
uses range endpoint. PC renderer/sky cache use pc_camera/render_pitch, NOT weapon
pitch or render_view.camera. Original reference orientation stays separate. Do
not move the original crosshair or alter player aim to compensate camera offset.
New test-chase-aim inbothdrivers:1213ASan/UBSan/leaks1assertions,88real thirdperson
views across4headings/11pitches +/-80,jump. Maxcontact projectionerror0.211342px
at4K. Player/firing trace unchanged,PCdrawreadonly. test-view10962assertions,
2048realmapframes+128pitchedposes/16maps PASS. Free-look18510/30100depth/HUD
PASS; original hitscan8192ROM PASS. Split-path5258completion,87normal+87pitched
draws PASS. Windows rebuilt; normalstartup/restartbothmodes,600gameplay,
5258completion and4free-look/4Kmodes PASS. Read analysis/chase-aim.md.
Earlier render benchmarks and broad audits predate convergence; no new FPS claim.

Chainsaw switch-angle fix: prepare_view now traces a COMMON2048-unit sight ray
for all weapons, independent of actual attack range. Do not use equipped weapon
reach for camera convergence: weapon1's96-unit reach tilted the elevated camera
downward on selection. Heading/height/slope and original crosshair rules remain;
attack range/mechanics unchanged. Extended test-chase-aim reproduced the old
code failure, now9223ASan/UBSan/leaks1assertions/232alignedviews including144normal
switch-animationframes. All10weapons have identical presentation at each fixed
player/worldpose; explicit96/1024/2048attack-range checks. Max4Kerror0.228582px.
Windows rebuilt; actual launcher startup/restart PC/reference,600gameplayframes
and5258normal-inputcompletion PASS. See analysis/chase-aim.md/currentreports.

Unlimited-range PC checkpoint (supersedes finite camera/attack ranges above):
NativeGame enables weapon.unlimited_ranged and projectiles.unlimited_player_range.
ROM range table, original controller/hit-query/motion entrypoints stay unchanged.
weapon_runtime extends clear ranged hitscan rays (including shotgun side pellets)
to first obstruction/world boundary; original nearby impact arithmetic remains.
Meleeweapon1 stays96units. Freeaim extendedquery; originalautoaim retains nearby
vertical selection and acquires distant level-ray targets. Near projectile muzzle
fractions stay relative to original controller range. Don't replace with giant
fixed-point range: original traversal caps at64cells and coordinates can wrap.
ssa_weapon_hit_query_unlimited walks512-unit steps, preserving source identity,
planes/portals/entities and checking coordinate/world bounds. Query has no damage.
Camera uses SAMEunlimitedexact sight ray forallweapons, keeping chainsaw alignment.
ssa_projectile_motion_unlimited: onlyplayerowned shots skip480tickexpiry and age
saturates480 to avoid timerwrap. Enemyexpiry/effecttimers/bounces/impacts retained.
New test-unlimited-range inbothdrivers:1124ASan/UBSan/leaks1checks,29000unitray
beyond64cells,wallocclusion/floor/ceiling/worldedge/coordinatebounds,1000projectile
ticksafterexpiry,8actualruntime2304unitattacks weapons2..5 inautoaim/freeaim;
chainsawdoesnothurtfartarget. Originalhitscan8192 andmotion16384ROMcasesPASS.
Chase9223checks/232views max4Kerror0.212615px; free-look18510/30100depthPASS.
Longer shotgun reach changes combat/wave timing. Normal-input bot now fights
visible threats even below40HP and skips optional supplies after28000score;
completion requires original exit/groups19spawns/score28000/liveplayer and
>=2pickups, rather than demanding historical8pickups. No body/
health/ammo/script grants inbot. GCCsanitizers/MSVC both4653frames,28000score,
7pickups,19spawned,23health,13pistol/22shotgun. Windows script pins new results;
600smoke andnormalstartup/restart PC/referencePASS. Older audits/benchmarks and
Windows4Ksnapshots predate this change. See analysis/unlimited-range.md.
Split tick/PC draw route alsoPASS:77normal+77pitched1280x720frames, same4653
completion. Renderer sampling check follows actual route ticks/60, rather than
historical minimum80frames, because longer-range shots shorten the route.

Third-person muzzle-flash visibility: prepare_view hides owned type96 effects
with flag2000 (particleIDs8..12, all5weaponflashfamilies) in render snapshots only.
Clear2000 AND set80: original fixed-screen projection ignores invisibility, so
setting80 alone would leave flashes visible in --original-renderer. Canonical
entity flags/particle allocation/lifetimes/attacks stay unchanged. Firstperson
snapshots restore originalflags, including activeeffects onHswitch/fallback.
Impacts and projectiles unaffected. test-chase-aim now9328ASan/UBSan/leaks1checks;
all5families third/first/third, referenceprojectioncounts andPCpixelcomparisons,
entityflags unchanged plusimpactcontrol. Same232alignmentviews/max4K0.212615px.
Windows rebuilt; startup/restartbothmodes,600gameplay and4653completionPASS.
Running game exe was preserved by renaming to timestamped.running-*.exe in
build/native-windows/Release (Windows permits loaded-image rename), not killed;
fresh launcher exe containsfix. User needs relaunch toloadnewbuild.

Native audio milestone (supersedes muted-backend notes): src/audio.c and
include/ssa/audio.h implement ROM-data MP2K sequencing/host mixing, NO ARM CPU
runtime.136songrows at083E99D0,5empty/131populated (105effects/jingles+26music).
Originalsongstart08C3EAC andmusicselect08FBC recovered tableaddresses. Campaign
music table0844A9DC has14entries, NOT16 (followingwords272/768 areotherdata).
Level0music0x73; diagnostic14/15requestnosong. MPlayrecords083E99A0+player*12+10
gateincomingpriority; SSAflagsall0, so don't unconditionallyrejectlowerpriority.
Tracks handle runningstatus, waits, notes/ties/EOT, tempo, patterns(depth3), GOTO,
repeat, instruments/keysplit/rhythm, pitch/pan/LFO. Preserve sequence-note identity
separatefromrhythm pitch forEOT. PCM signed8 source resampledlinearly, pulse/wave/
noise synth,32hostvoices/stereo16 output. Not bit-exact GBA hardware audio:
PSG envelopes/channel allocation/modulation approximate, no hardwareecho/reverb/
sweep, no fulloriginalmenu/musicdispatcher orpositionalaudio. Footsteps still
explicitlymuted in NativeGame tick. Latermusicbeyondtestedexcerpts needsvalidation.
SsaNativeGame.audio_ops optionalplay/stopobserver, bindonlyidle/loaded; callbacks
don'tmutate/reentergame. Native callbacks nowplayandSTOP weapons/enemyloops etc.
Logical soundreturnvalues remainindependent ofdevice/preemption tokeepgameplay.
Windowsplatform/audio_win32 owns48000Hz stereo16,3x800frame buffers (~50ms),
workerthread4mswait, criticalsectionserializesmixer/events/restart/mute. Close
signals/joinsworker thenwaveOutReset/unprepare/close beforeROMfree. Restartflushes
andresetsmusic whilemuteprefsretained. Normalplayautoopensdevice; failurelogs
andcontinuessilently. M togglesmute, --mute, --no-music. Legacyhiddenrender/startup
tests stay silent unless --audio-test (requires--startup-test). Hardwaretest
isoutputmuted; deliberate160msrenderstall muststillrecyclebuffers, Mhandlerdouble
toggle underlock and1restart checked. No physical-listening claim.
test-audio inbothdrivers:289913ASan/UBSan/leaks1checks,131nonzerosongs,
35640000catalogframes24kHz (3sec/effect,45sec/music)+level0throughfirstGOTOloop;
sample/pitch/pan/mute/blockinvariance/rhythmEOT/optionalpriority/malformeddata tests.
MSVCsamecatalogPASS. test-audio-playability:actualnormalinput4653completion,
3722400mixedframes48kHz,2157notes,7443362nonzerosamples,20clampedstereosamples,
noerrors; same28000score/7pickups/19spawned/23health/13pistol22shotgun. Windows
audio-output/restart/mute/worker-stall testsPASS. 101libraries Windowsbuild;
normalstartup/restartPC/reference +600gameplay and4653unboundsinkcompletionPASS.
New scripts/test-native-audio.ps1, scripts/test-audio-playability.ps1 and CMake
nativeaudio/nativeaudioplayabilitycheck targets. See analysis/audio.md/reports.
decomp.wtf live catalogue unchanged74/526,2152/87648bytes2.46%,520attempts;
no export/ROMchanges or claimthatnewaudiohostcodeis matchingoriginaldriver.

Footstep/PSG audio follow-up: NativeGame now enables original camera footstep
events13/14, filtered at player_sound after actual motion. Require movement keys,
liveplayer, exactgroundedZ and >=1/16unit horizontal displacement. Snapshot X/Y
at tick entry. Recovered physics/camera code stays unchanged; no walking sounds
when idle, airborne, disabled or blocked bywalls. Bothfirst/thirdperson work.
ROM routes13/14 toplayer0 (music!), so host audio play/stop overrides ONLYthose
two routes toSSA_AUDIO_FOOTSTEP_PLAYER=4. SSA_AUDIO_ROM_PLAYERS stays4 for ROM
table validation and prioritymetadata; SSA_AUDIO_PLAYERS=5 for host mixer loops.
Music trackstate/tempo/clock preserved across bothfeet play/stop, including
--no-music effectsonly. Other effects retain originalrouting/prioritypolicy.
PSG envelopes now15level normalized host ADSR with instrument 3bitattack/decay/
release rates at64Hz,4bitsustain, zerorates skipphases. release_voice unifies
gate/FINE/EOT release; zeroPSGrelease endsimmediately. Fractional sampleclock
independentofrenderblocks. Stillnotbitexactoriginal volume-goalquantization,
hardwareallocation, sweep,echo/reverb orDAC; no physical-listening claim.
test-audio now297034ASan/UBSan/leaks1checks; same131nonzerosongs/35640000catalog
frames. New24kHzPSGADSRunits/zero-release fixture and44.1kHzsplitbuffer/30thtick
at sample20672. ActualW/S/A/D,H,jump,idle/disabled scenarios:23steps,
156airborneframes,872blocked/idleframes, musicpreserved. Windowschecker samePASS.
Audioenablednormalinputroute still4653ticks/28000score/7pickups/19spawned/
23health/13pistol22shotgun, now254steps/2411notes/3722400mixedframes/
7443218nonzerosamples/0clipping/0errors; musicactive0x73on EVERYtick.
WindowsMSVCsamecompletion/steps/notes; device testnowrequiresbothfootsteproutes
preserveplayer0, plusrestart, withmusicenabled/disabled andmutedoutput.
NormalWindowsstartup/restart PC/reference,600gameplay andunbound4653completion
PASS. Fresh101libraryWindowsbuild inusualRelease path; relaunchplay-native.cmd.
Live decomp.wtfstatus unchanged74/526matches,2152/87648bytes,520attempts;
ROM SHA1still4ff48d8c18b70a5184758f1917343ae85a70858e. See analysis/audio.md.
One repeatWindowsaudio aggregateassertion failed beforecaseoutputwasretained;
causeunconfirmed. Isolatedlaunches and5consecutivefulltestsafterwardPASS.
scripts/test-native-audio.ps1 nowcaptureslauncherexitbeforeJSONdecode andsaves
build/native-audio/device-music.txt anddevice-effects.txt forfaildiagnostics.

PC spatial audio follow-up: optionalplay_at/listen/move added aftercontext in
SsaNativeAudioOps; allinitializers updated. Legacy sinks still receivecentered
play. NativeGame copies enemy/CD/activation/impact-particle positions at emission;
localplayer/weapon/footstep/pickup/mover sounds staycentered. Original08D5C takes
IDonly; this spatialmodel is aPCaddition, NOT a recoveredmatchingGBAbehavior.
Listener usesoriginalplayer.camera.eye_z andactualmotionheading rightvector
(cos,-sin), NOT pc_camera/chasepose orpitch. Bindrefresheslistener; worldevents
refreshbeforeplay; tickrefreshesaftergame_step. H/pitch don'tdisplaceaudioears.
One-shotpositions copied, noentitypointerborrowedbyaudioworker, slotscanbereused
withoutmovingoldimpacts. Canonical game.cd_game.loop_source supplies0x3Aposition
aftereachtick. move onlymatchescurrentpositionedsong anddoesnotrestartnotes.
audio.h exposesSsaAudioPoint/Listener, ssa_audio_play_at/listen/move. Music>=0x6E
rejectedbyplay_at; ordinaryplay resetsposition/gain. Trackpan preserved; world
pan reducesoppositechannelto25%atpureleft/right. Gain1through256units then
256/3Ddistance withoutcutoff.24.8deltas+uint64isqrtboundfullint32coordswithout
libm. Smoothworldgainchanges per samplewith10mstimeconstant. resetclearsspatial
state/listener. Win32copiesallvaluesunderexistingcriticalsection; neverreads
gameplayonworker. NoHRTF/Doppler/occlusion/roomreverb orindependentpolyphonic
copiesofonesong. Original4playerpreemptionexceptfootsteps retained; original
08D5C per-frameSFXgate notyetmodeled. Fullmenu/musicdispatcher stillunfinished.
test-audio297274ASan/UBSan/leaks1checks,131nonzeroentries/35640000catalogframes.
IndependentconstantPCMlevel/distance/direction/vertical/rotation/smoothing/
fullcoordinatebounds/move-withoutrestart/centerreset/musicisolation/blocksplit
fixtures; compositionactualimpactparticle,controller/activation/localweapon,
H/pitch/yaw inputchecksPASS. Existingfootstep/PSGchecksremainPASS.
AudioenabledrouteGCC+MSVCstill4653ticks/28000score/7pickups/19spawned/23health/
13pistol22shotgun.254steps/2411notes/175spatialevents/4829listenersnapshots/
321CD-looppositionupdates,3722400mixedframes/7443190nonzerosamples,
0clipping/0errors,levelmusic0x73activeeverytick. Windowsaudio_spatial=2tests
playright/moveleftAFTERrestart, bothmusic/effectsonly, outputmuted; no listening
claim.101libraryMSVCbuild,PC/referencestartup/restart,600gameplay/unboundnormal
4653completionPASS. Freshplay-native.cmdexe. Source/reporthashauditcurrent.
Live decomp.wtfunchanged74/526functions,2152/87648bytes(2.46%),520attempts;
userROM SHA1unchanged4ff48d8c18b70a5184758f1917343ae85a70858e. Seeaudio.md.

Single-player white variant: NativeGame.prepare_view sets ONLY local player's
SsaActorSource.owner=3 when combat.player_count==1. Original overlay formula
animation+5*(owner+1) selectsresources121..125 (white shirt), insteadof106..110
(blue). Verified originalROMpreviewfamilies106blue/111green/116orange/121white.
Canonical player_index remains0; animation/controllers/collision/assetdecode and
originalprojection code unchanged. Shared snapshot appliesPC level/pitched and
reference renderer. Originalstanding/walking/hurt/death mappings retained.
101libraryWindowsbuildPASS; chaseaim9328ASan/UBSan/leaks1checks/232views/max4K
error0.212615PASS. WindowsPC/reference normalstartup/restart,600gameplaysmoke,
4653normalinputcompletionPASS. Visuallyinspected newgameplay.bmp: whitevariant
visibleonSam'schasebody. Freshplay-native.cmdexe; relaunchneeded. Currentbuild,
chase/game/startup reports matchthischange. Earlieraudio sourcehashreports predate
thiscosmeticNativeGame snapshotchange; audioimplementationitself unchanged.

Music transitions follow-up: audio.c/.h now expose ssa_audio_music_select (pure
08EB0music-requestpolicy) andssa_audio_music (hostapply). Settings[0300572C]+2C
isMUSICENABLE, NOTplayercount; requestsonlywhenbyte==1, exceptcommand21stop.
Modes1/3:84onlevels0..5,12,13else85;2:14entrycampaigntable;7:70;8:6F;
9:78on0..5,12,13else7F;10:81;11:71level0/6Elevel6/82else;19:83;
20:78level5else7F;21:FFFFstop. HexsongIDs. OthercommandUI/SFX/pause effects
notclaimedimplemented. Nativeguardmaps14/15command2nooutoftablelookup.
New tests/test_music_selection.py iscalledbytest_audio.py:1400isolatedROM08EB0
comparisons(14levels*25commands*4settingsbytes),2diagnostic+6invalidguards.
UnicornARMtest-only; no runtimeCPU. analysis/music-selection-validation.json.
NativeAudioOps gainsoptional7th music(ctx,command,level) callback. NativeGame
music_mode=2initially, separatemusic_serial; latchedcallbacksforfirstdeathnotice
(10) andactualexit(9), MODE/bossmusic forwarded, latebindresyncterminalcue.
PCmanualrestartdeathscreen usesoriginalgameovercueatnotice, NOTclaimedoriginal
continue/menu timing. Backendopens/restartslevelmusic asbefore. music9/10clear
SFXplayers1..4even--no-music so frozencontrollerscannotleaveoldloops; deathvoice
17emittedafterwardstillplays. music21stopsONLYmusicplayer0, nototherSFX.
Originalqueuedfades/intro-autoreturn/pause/menuSFXdispatcher stillunfinished.
299233ASan/UBSan/leaks1Cchecks,131songs/35640000catalogframes; realcombatdeath,
320death+30terminaltickswithoutretrigger, gamefree/reinitrestart, latebind,
effects-only/mute/terminal-loopcleanupPASS. WindowsMSVCsameCcheckerPASS.
GCC+MSVCnormalrouteunchanged4653/28000/7pickups/19spawned/23HP/13pistol22shotgun;
now2413notes,254steps/175spatial/4829listener/321loopupdates,3722400mixedframes,
7443190nonzerosamples/0clipping/0errors. FinaltickONEcommand9to0x78;
music_preservedfieldnowmeanslevelsonguntilfinish+correctterminalcueafter.
Windowsaudio_music=3:cue9BEFOREactualrestart, stickychecklevelsong0x73restored
AFTERrestart, thencue10andlevel2 callbacks; --no-musiccase stayssilentmusic.
Normalstartup/restartPC/reference,600smoke/unbound4653completionPASS.

Launcherconcurrencyfix: sharedlast-launch.log heldopenbyliveusergameblockednew
launches. play-native.cmd nowatomicallymkdir-reservesbuild/logs/launch-*/,
stderr.logperlaunch, <=32collisionretries. CMDsimultaneousRANDOMsequencesCAN
collide, so randomfilenamealoneisNOTsufficient. best-effortcopytolast-launch.log
onexit, preservingnativeexitcodeevenifsharedfilelocked. SSA_LAUNCH_LOGoverride
forautomation; startup/audio testssetandrestoreprivatepaths. Startupchecksnow
also2simultaneousdefaultlaunches+distinctsessionlogs. CaptureStart-Process.Handle
beforeWaitForExit toretainExitCode. No usergameskilled. Firstloadedexe preserved
asRelease/ssa_native_game.running-20261015-172854-074.exe beforelinkingfreshname.
Don't rebuildwhilelauncherchecksareactive: own validationprocessescanlockexe.
101libWindowsbuild/latestexe current. Earlierchaseaimreportpredatesmusicheaders;
audio/music/build/game/startup audits refreshed. Live decomp.wtfunchanged74/526,
2152/87648bytes2.46%,520attempts. ROMSHA1unchanged4ff48d8c18b70a5184758f1917343ae85a70858e.

Audio pseudo-echo follow-up: CD08/CD09 now store track echo volume/length and
copy them into each new voice. Later commands do not retarget existing tails.
Voice stage4 holds the oscillator at the echo level; it is not a delay line.
PCM ADSR now uses original8-bit arithmetic, rather than floating decay, and
enters echo at the release threshold or zero sustain. PCM echo level=volume;
PSG uses ceil(15*volume/256), retaining the host's normalized PSG goal model.
Both tail lengths count60Hz sound frames, independent of tempo/64Hz PSG ADSR.
release_voice ignores already releasing/echo voices. Stop/reset clears tails;
mute still advances them. Global reverb/hardware sweep remain unfinished.

ROM evidence: PCM envelope080C32EE..3354; xcmd080C51F8 table083E8FAC;
CD08 handler080C52D8 writes track+1E, CD09 at080C52E4 writes+1F.
tests/test_audio_echo.py plus test-only audio_echo_oracle.c compare18,307 PCM
steps/lifetimes and512 command-byte cases directly against isolated ROM code.
Called by test_audio.py; report analysis/audio-echo-validation.json. Never
compile audio_echo_oracle.c into the game (it includes audio.c for private
test access). ARM execution remains test-only. analysis/inspect_audio_echo.py
is read-only reconnaissance; all song-header reverb bytes are0 (does NOT prove
the global driver reverb setting is0).

315479 GCC ASan/UBSan/leaks1 and MSVC C checks pass: all131 populated songs,
35640000 catalog frames, plus explicit sampled/PSG echo amplitudes, expiry,
per-note settings, nonzero PSG release, chunk invariance, mute/stop/reset.
GCC/MSVC audio completion still4653ticks,28000score,7pickups,19spawned,23HP,
13pistol/22shotgun. Audio2413notes/254steps/175spatial/4829listener/321moves,
3722400frames,7443179nonzero samples,0clipping/0errors. Windows device checks
pass muted, with/without music and restart. Matching decomp.wtf unchanged
74/526,2152/87648bytes(2.46%),520attempts; user ROM unchanged.

Music queue/fade/intro follow-up: audio.c/.h now have SsaAudioMusicState and
ssa_audio_fade(interval0..255, capped15). Original08D30 queues latest song;
08C04 commits at a sound frame, waiting for an explicit fade to reach0.
Requests do NOT initiate fades automatically. Fade volume startsFFDC=65500;
subtract base>>4=4093, so17 decrements reach0 (16 leave12), next frame commits.
Gain uses ceil(volume/256)/256 and applies to all five host players. Repeated
fade calls leave it running. Pending matching stop cancels; reset clears all
policy state/restores gain, preserving mute/music settings. Empty-player music
starts immediately as a PC convenience. Original game-transition fade request
call sites are NOT all bound yet; Windows audio-test explicitly starts a fade.

Original09120 auto-returns commands1/20 to2 on inactive music, ONLY enabled==1.
Commands9/10 remain terminal. Windows open/restart now requests intro1 (84 on
levels0..5/12/13,85 elsewhere); backend auto-returns to campaign song. No gameplay
delay. ssa_audio_level_music still means direct command2 for diagnostics/tests.
NativeGame's mode latch default2 is independent of backend intro playback.
Real audio full-route test starts intro too, validates84 then73 then final78.
Windows device test queues each music callback once and waits for commit (never
requeue every frame), verifies intro before/after real restart and restored gain,
explicit fade progression, and terminal callbacks. audio_intro=2/audio_fade=2.

tests/test_audio_policy.py and private audio_policy_oracle.c compare8360 sound
frames,166 queue requests,1552 fade starts,2688 automatic-return cases against
original08C04/08D30/08E74/09120:12766 ROM cases. Called by test_audio.py;
analysis/audio-policy-validation.json. Oracle includes audio.c; NEVER link it
into game. Reverb discovery: init mode0092D300 atC3E48 after zeroing; only other
direct mode-set caller C46AE is header-gated, all136 header reverb bytes0. Keep
dry output; no invented global reverb. analysis/inspect_audio_policy.py is
read-only call-site/settings evidence. No reverb renderer added this turn.

335597 GCC ASan/UBSan/leaks1 + MSVC checks,131 songs/35640000 catalogue frames,
4 real intro/post-boss automatic returns PASS. Constant-PCM fade amplitude,
silence, overwrite/cancel queue, chunk invariance, mute/reset PASS. Full route
GCC/MSVC still4653ticks/28000/7pickups/19spawned/23HP/13pistol22shotgun;
now2279notes,7443373nonzero samples,3722400frames,0clipping/errors. Both intro
and level music observed;254steps/175spatial/4829listener/321moves unchanged.
Windows muted device both music settings, normal PC/reference start/restart,
concurrent launch logs,600smoke and unbound4653completion PASS.101-library
build has no compiler warnings; latest play-native.cmd target rebuilt. Ten
current report audits/542 source hashes PASS. Live decomp.wtf74/526,
2152/87648bytes2.46%,520attempts; user ROM SHA1 unchanged.

Flat background request: PC sky is now a screen-facing2D backdrop, not a
per-pixel rotated ray/sphere/cone. Yaw pans horizontally via existing linear
sky_column; vertical framing stays FIXED across pitch. World/actor perspective,
aim and depth are unchanged. Both level and pitched paths share sky_row.
Only the actual60 backdrop rows are fit across screen height: family starts
46285C/46DC5C differ768*60. The original120-row readable sampling window
overlaps neighboring image/data; sampling all120 at high pitch exposed bright
garbage. Original GBA/reference path remains unchanged. Do not restore120-row
sampling to the PC background or clamp pitched per-ray poles (vertical bands).

pc_renderer_pitch.inc replaces sky_uv[pixels] with sky_axes[width+height].
Rows rebuild only on dimensions; columns update for yaw, with no per-pixel
directions. Cache costs4*(width+height), not4*width*height. geometry_free owns it.
tests/test_free_look.c now identifies visible background via solid0/255 sky
renders, then verifies straight horizontal/vertical stripe patterns and fixed
row sampling across7 pitches/varied headings.1493513 ASan/UBSan/leak assertions,
30100 independent world depth cases,18280 HUD pixels PASS. PC renderer sizes
11169482 assertions/7sizes/24chase-jump frames PASS; different_from_upscale now
268576. Chaseaim9328checks/232views/4Kerror0.212615 PASS. Windows up/down chase,
4K and original fallback visually inspected; no bottom neighboring-art leakage.
101library build, startup/restart/concurrentlogchecks,600smoke and4653normal
completion PASS.7 current rendering/build/game/startup audits/530 source hashes
PASS. Older audio validation executable/full-source hashes predate this renderer
change; audio code itself was not edited. User ROM SHA1 unchanged.

Two-level campaign checkpoint: production play-native.cmd now exposes levels
0..1 only. Enter after level0 completion advances to level1 with actual earned
health/armor/weapons/ammo/acquisition queue/selected usable weapon/score and H
camera mode. R restores current level-entry values, including after death.
Enter after level1 exit replays it; remaining campaign is still unsupported.
Direct --level1 uses original map1 startup loadout. This is PC shell policy,
not a claim to have recovered the original campaign menu/transition dispatcher.

SsaNativeCarry contains VALUES ONLY. Capture requires idle/alive valid game;
apply requires initialized game BEFORE first tick. Reject invalid inventory,
selected weapon and late application. Never copy or relocate SsaNativeGame:
it has self-pointers. reload allocates replacement at final heap address, inits
and applies carry before releasing old game, then resets renderer/audio and
performs mandatory bootstrap tick. Keep renderer flat-sky policy above intact.

tests/test_second_level.c includes unchanged first-level normal-input bot and
captures its real earned carry, then completes level1 via movement, aim, fire,
E and owned-weapon selection. No grants, body writes or script shortcuts.
Route planner changes only separate map copy. Level1 needs reachable supplies,
offscreen-enemy hunting, actual wall switch approach/use, and reselecting owned
shotgun after ammo pickup to avoid leaving empty-weapon fallback on pistol.
Actual wall switch latch/exit_toggle are asserted. Some optional enemies can
remain when original exit becomes available; do not require arbitrary kills.
First unchanged:4653ticks/28000score/7pickups/19spawned/23HP/13pistol22shotgun.
Second:11314ticks/102000cumulative score/22pickups/54spawned/75HP.126 PC views
(63 level +63 pitched) plus reference draws pass;10 carry guard cases pass.
GCC ASan/UBSan/leaks1 and MSVC results identical. test-second-level is registered
in decomp.ps1/scripts/decomp.py. analysis/second-level-validation.json binds
all portable sources/headers and both bot sources to this result.

scripts/test-native-campaign.ps1 -SkipBuild validates MSVC completion plus actual
launcher Enter advancement/R restoring entry inventory and score, both renderers,
real muted audio restart with music on/off. Explicit --transition-test requires
--startup-test --mute and level0; ONLY that test fixture sets terminal/dead state.
Do not confuse frontend fixtures with independent normal-input completion proof.
analysis/windows-native-campaign-validation.json records both checks. Also reran
test-native-game.ps1 (normal startup/restart/concurrent logs/600smoke/4653route)
and test-native-audio.ps1 (131-song catalogue/device worker/intro/fades/restart).
All pass; physical devices/listening untested.101-library Windows build has no
warnings and current play-native.cmd target is rebuilt. Direct level1 1280x720
snapshot visually inspected.6 current reports/332 source hashes and recorded
game/checker hashes audited successfully. Live decomp.wtf stays74/526,
2152/87648bytes2.46%,520attempts. ROM SHA1 unchanged. No new byte matches claimed.

analysis/probe_native_levels.c/.py is read-only native reconnaissance: all14
campaign maps initialize/run600 basic ticks, NOT full playable proof. BossD7/D8
and later campaign must still be recovered/validated before being exposed.

Original map-view/music correction supersedes earlier intro-in-gameplay notes.
User explicitly requires all original features/behavior identical, with requested
PC additions. Previous open/restart command1 during live gameplay was WRONG.
Original01ACBC requests3 on pre-level map;020A0 requests2 at gameplay init.
Commands1/3 select same84/85 but ONLY1 auto-returns;3 stays map/silent when cue
finishes. Never start1 in normal Windows startup/restart, nor auto-start gameplay
at map song completion. Audio policy's1/20 behavior remains valid in other contexts.

src/map_view.c + include/ssa/map_view.h recover01A9A4/01AAA0/01AC80. Original
Egypt/Rome art0AB39C/0AFE4C decodes38400bytes,600tiles (original48000byte copy
overruns its actual decoded artwork; do not copy uninitialized trailing bytes).
Tilemap0B4EEC is30x20 copied to32stride; palette0A8168. Font0B697C/0B7A60 has
126glyphs each, NOT63;16px/8px four-bit fonts,16128/4032bytes. Glyph/metrics/
language tables and OAM priority preserved. Route table44D1C4 ten points/stage,
one new point/32ticks, prior routes remain; prompt blinks bit5, title/password
and palette banks original. Module supports12campaign maps/3languages/3tiers;
frontend stillonlylevels0..1, English/normal. Menus/password entry are NOT ported.

Fade15614/15698 executes before input each60Hz screen tick. Starts0F00,-0040;
fire starts from0,+0040 once, plays command13's song2 and audio fade5 if music
enabled. Done61 subsequent ticks. Native map view owns no game state. Windows
run freezes gameplay counter0 until done, then resets/flushed device map audio,
starts command2 at restored gain, performs mandatory first gameplay bootstrap,
renders and captures mouse only if its own normal window is foreground. Click,
Space/Enter confirm map. R/restart and level advance show it again. Raw movement,
weapon selection/H do not reach gameplay during map. Missing device/no-music
cannot block screen timing. PC sky/cameras/gameplay otherwise unchanged.

tests/test_map_view_rom.py:864 original glyph-submission cases (12x3x3x8),
4147200 original VRAM background pixels,691200 actual OAM/VRAM composed pixels,
8976 original fade/input frames PASS. ROM CPU execution ONLY isolated test oracle.
Hook only selected routine entries, not every instruction (much faster). Analysis
inspect_map_view.py saves read-only evidence build/native-tests/map-view-disassembly.txt.
Native map0/1/6 previews underbuild/native-tests; map1 visually inspected.
tests/test_map_view.c/.py:7694 ASan/UBSan/leaks1 and MSVC checks. Actual map songs
both families finish then wait another180ticks each in normal/muted/no-music,
1080 terminal wait ticks total. No automatic level music or map completion.
Bounds/malformed LZ/repeated press/full fade/reset gain/mute preserved PASS.
decomp.ps1 commands test-map-view and test-map-view-rom registered.

Windows startup tests now automatically press mapped Space after8 map ticks and
wait full61tickfade:70 frozen ticks per map. Startup/R tests2maps/140ticks;
campaign Enter thenR tests3maps/210ticks. Map command3 checked9 times per map,
level command2 on each finish_map. Device tests renamed audio_intro to
audio_level_starts (2); no fake intro during gameplay. scripts/test-native-campaign
also runs MSVC map checker and compares GCC result. test_audio_playability.c now
starts command2 at first gameplay tick, NEVER1. GCC/MSVC4653route unchanged;
2413notes/7443179nonzero/3722400mixedframes/0clip/errors,254steps/175spatial/
4829listeners/321moves. Original menu/continue/boss/full-campaign/audio fidelity
gaps remain; do not claim this screen makes full original feature parity done.

Final map-view build:102 libraries, no Windows compiler diagnostics; latest
play-native.cmd executable rebuilt. Normal startup/restart/concurrent launcher,
600smoke, unchanged4653first completion, full two-level campaign, MSVC map/music
checker and muted Windows device music/effects checks PASS.9 current reports/
360 source hashes/9 recorded binary hashes audited. Physical capture/listening
still not manually verified. decomp.wtf74/526,2152/87648bytes2.46%,520attempts;
no new exact matches claimed. Original ROM SHA1 unchanged.

Original startup menu checkpoint: src/menu.c + include/ssa/menu.h now recover
1862C loop and callbacks16A34 main,17BBC language,17CD0 title,17DB8 difficulty,
17F7C single-player,180D4 music options. Supported pages0,1,4,9,10,11 ONLY.
Main multiplayer/scores and single-player password/options credits destinations
emit explicit unsupported notifications and keep current page usable. These
guards are PC prototype boundaries, NOT original parity. Boot/publisher screens,
first-stage narrative/cutscene, pause/continue/scores/password/credits/multiplayer,
save/settings persistence remain unfinished. Read analysis/menu.md.

SsaMenu is zero-initialized, stable inline storage, borrowing ROM. It embeds the
shared map-view font/palette canvas. map_view.c now exports shared text/compose
helpers without changing map behavior or SsaMapView layout. Menu uses32K tiles
0A8368 and12K logo0B5434; original64x64 BG0 tilemap, stepped horizontal scroll,
three persistent64x64 eight-bit logo objects, original126-glyph fonts/metrics.
Do not replace it with a host-designed menu. draw_page/draw_counter retain the
old callback's final submissions when a key changes pages; counter increments
only if the original outer loop continues. Title START gate iscounter>90, prompt
blinks bit5. Language confirmation resetscounter0. Main text slides16px/tick
from+-240. Native input is edge navigation; boundaries usecommand17, moves12,
confirm13 (some original paths twice), back16. New-game fade exits61 ticks later.

Normal Windows startup runs language/title/main/Single Player/New Game/difficulty,
then original map. Explicit --level0..1 bypasses menus for diagnostics. Smoke
remains a separate diagnostic bypass. Menu input arrows/WASD; Enter/Space/click
map confirm+START; Esc back. During menus/map gameplaycounter stays0. Menu-selected
language carries to map and difficulty sets existing combat.damage rules. Initial
continue counts5/3/2 also selected; finite continue progression not yet implemented.
R checkpoint and two-level carry policies are still PC shell behavior, not claimed
original campaign-policy recovery. Both choices survive advancement and restart.

Windows audio_open now starts SILENT (previous unconditional3 removed). Original
language confirm requests menu8; navigation/confirm/boundary/back sounds bind
12/13/17/16->songs1/2/6/5. Options set music_enabled and command8/21; M is separate
mute. New game requests fade5. begin_map ALWAYS resets voices/fade and requests3;
finish_map flushes map buffers and requests2. Never start1 with gameplay. reload
rebinds new game audio ops beforebegin_map; no duplicate music reset. No device
must still allow full UI flow. Both standard audio and campaign tests cover it.

tests/test_menu_rom.py reuses isolated map ROM oracle (ARM TEST ONLY), then
compares1179 menu frames including state, command order, glyphs and4147200 full
OAM/VRAM pixels. All3languages/3difficulties plusoptions toggles/back/bounds/fades
PASS. Shared map proof864/4147200background/691200composed/8976fade also passes
after shared canvas extraction. tests/test_menu.c/.py passes13256 ASan/UBSan/leaks
and MSVC checks,3402frames/18newgames, all3x3x2language/difficulty/music modes,
output sentinels, malformed logo, guards, unsupported boundaries. Menu preview
build/native-tests/menu-0.png visually inspected. No replacement ROM art assets.

scripts/test-native-menu.ps1 tests all9language/difficulty combinations through
actual launcher with both renderers and real muted audio. --menu-test0..8 is
explicit test-only (requires--startup-test --mute, rejects directlevel/audio-test/
transition-test). It visits music options/toggles twice, thenSP/NewGame/difficulty,
map/gameplay/R. Eachcase213frozenmenu+140frozenmapticks,2levelmusic starts;
actual native combat difficulty and maplanguage asserted. Standard startup route
skipsoptions and takes195frozenmenu ticks. The startup/audio/campaign scripts now
also assert original-menu traversal. Do not remove these checks to speed startup.
Input/listening are synthetic window messages/muted, not physical-device proof.
test-menu and test-menu-rom registered in decomp.ps1/scripts/decomp.py.

103-library Windows build/latestplay-native.cmd target rebuilt without diagnostics.
Core first-level route with audio unchanged4653ticks/28000score/7pickups/19spawned/
23HP/13pistol22shotgun;2413notes/7443179nonzero/3722400mixedframes/0clip/errors.
The original menu plus map integration adds native code, not exact byte matches.
Live decomp.wtf74/526,2152/87648bytes2.46%,520attempts. Original ROM unchanged.

Final menu checkpoint validation: all9 Windows menu selections, standard startup/
restart/concurrent logs,600 gameplay smoke, first-level completion, both full
native campaign levels, map/music transitions and actual muted audio device
catalogue/worker/restart checks PASS.12 current reports/391 source hashes/11
recorded binary hashes audited. No user game processes killed or machine audio
settings changed. Physical input and listening still unverified.
# Audio channel/gain correction (supersedes normalized PSG/polyphony notes)

`src/audio.c` now follows the supplied ROM's SoundMode0092D300: three shared
PCM voices, four exclusive PSG channels, and one separate PC footstep PCM slot.
SSA_AUDIO_VOICES is8; do not restore a32-voice free-for-all or overlap several
oscillators on the same PSG channel. PCM chords across the three physical voices
are intentional. Priority allocation C3BBE..C3C32 uses released voices first,
then priority and ORIGINAL track-address ties from PLAYER_TABLE, never host
pointer addresses. BA sets track priority; player+track priority saturates255.
The released flag survives entry into echo; a tail remains replaceable.

Volume/pan C47CC, note velocity/rhythm pan C3AC4, PCM master gain C3356 and
PSG goal C4978 use original integer truncations. Driver byte2 is RIGHT and
byte3 LEFT. Master13 scales PCM envelope by14/16. Removed arbitrary player0
0.28 vs effects0.7 gains; do not fix balance by normalizing individual samples.
PSG envelopes peak at the actual0..15 goal, echo=ceil(goal*echo_volume/256).
Wave DAC volume uses original083E8F68 table. SOUNDCNT_L77/HA90E determine
relative PSG/FIFO output gains; the host still interpolates PCM and uses centered
oscillators, with approximate PSG timing/modulation/sweep/DAC behavior.

`tests/test_audio_channels.py` compares12,000 original allocation cases,
6,000 original note/goal/PCM-gain cases, and executes original SoundMode field
setup. Includes held/released echo ownership, ties, cross-player priorities,
pan/velocity/rhythm-pan boundaries. `tests/audio_channels_oracle.c` includes
audio.c for test-only access; NEVER link it into the game. `test_audio.py`
invokes it alongside the existing music-policy/selection/envelope proofs.
Native C fixtures check same-channel replacement, BA command priority, PCM
limits, all five instrument output scales at full/reduced volume, waveform
stereo, echo cutoff, mute, block splits and catalogue channel ceilings.
Latest core checks573176, all131 populated songs audible/35640000 frames,
max3PCM/4PSG. Audio-enabled normal completion remains4653ticks/28000score,
3722400 mixed frames/2385 admitted notes/7443001 nonzero stereo samples,
254footsteps/175spatial/4829listeners/321moves/once-only final command9,
zero clipping/decoder errors. Read analysis/audio.md and source-bound reports.
No physical listening comparison or full hardware waveform parity is claimed.

Audio checkpoint Windows validation: rebuilt all103 native libraries/frontend;
MSVC matches573176 catalogue checks and4653tick audio-enabled completion.
Actual muted WinMM worker/buffers/mute/restart/music-on/off checks pass. All9
language/difficulty menu routes pass with original map/level music boundaries,
both renderers and restart. Both normal-input campaign completions and Enter
advancement/restart/map-cue checks pass.11 current audio/build/menu/campaign
reports audited:579 source hashes and8 recorded binary hashes, unchanged ROM.
decomp.wtf live status remains74/526 exact,2152/87648bytes (2.46% incomplete
catalogue),520attempts. No matching C count is added for the native audio fix.

Wave-instrument follow-up: fixed a remaining octave and gain error in audio.c.
C4880 produces the same period register for pulse and wave; the32-step wave
cycle runs at HALF the pulse frequency. Do not give type3 the pulse's261.6Hz
at key60. psg_register now uses ROM083E8E90/083E8F14 pitch tables, fractional
pitch and key clamping, plus C4D18 even-register rounding for bit8 instruments
at this ROM's SOUNDBIAS40. Pulse periods also use these original tables.
wave_dac applies INTEGER nibble conversion BEFORE centering:25% maps15->3,
50% maps15->7,75% maps15->11,100% keeps15. The prior smooth centered scaling
made quieter waves too loud. No instrument-specific normalization was added.
First-level bass instrument1/key40/volume105 is41.21761Hz; velocity44 produces
goal4/DAC60/p2p1536 (old1920), velocity108 goal10/DAC80/p2p5632 (old5760).

tests/test_audio_wave.py (part of test_audio.py) exercises all6 music wave
instruments1/5/7/18/24/32 and3 waveforms:960 original CgbSound note-on cases,
3840 original pitch-register cases for both pulses/wave,256 level/nibble DAC
checks. Original full CgbSound writes DAC/goal fields; the sample arithmetic
and hardware period semantics are independently checked against mGBA's PSG
implementation. tests/audio_wave_oracle.c includes audio.c for test-only
access, NEVER link into game. See analysis/audio-wave-validation.json.
GCC sanitizer core573209 checks/all131 songs/35640000frames passes; native
audio-enabled completion still4653ticks/2385notes/3722400mixed frames,
7442495 nonzero stereo samples,0clip/errors. Shared MSVC fixtures now cover
all four nonzero wave DAC levels and the correct wave half-cycle boundary.

During rebuild the user's running process29336 held the old game executable.
Renamed only that build artifact to
build/native-windows/Release/ssa_native_game.previous-cb15e3a7c93447a998b75e7344c798d7.exe
and rebuilt the normal ssa_native_game.exe successfully (103libs, no warnings).
The old process remained alive/responding; no user game was killed. The next
play-native.cmd launch uses the new executable. Do not delete the old backup
while it is in use. Listening comparison remains unverified.

Final wave checkpoint: MSVC573209 catalogue checks/all131 songs and actual
muted WinMM worker/buffer/mute/music-on/off/menu/map/restart tests PASS.
Windows audio-enabled first-level completion also passes4653ticks/2385notes,
with0 clipping/decoder errors.10 current audio/build reports audited against
559 source hashes and3 recorded binary hashes; ROM SHA1 unchanged. The older
full menu/campaign reports were not rerun for this audio-only correction.

Sample-volume feedback follow-up: user says ALL sampled channels remain quiet.
audio.c now applies PCM_BUS_GAIN=2.0 (+6.02dB) after sample()/recovered note gain
at the host summing bus. This is a requested PC balance adjustment, not a newly
discovered original-driver gain factor. C3394 ARM packed mixer confirms the
prior per-note multiplication scale; the original channel/volume/envelope
proofs remain unchanged before the host gain. Do not undo the boost based on
the older original-balance prose. PSG levels and wave pitch/DAC stay unchanged.
Applies to music/percussion, effects and PC footsteps equally; preserves pan,
spatial attenuation, master fades and pseudo-echo. No per-instrument peak
normalization, limiter or compression was introduced.

Core now573562 sanitizer/MSVC checks.20 slot/player reference cases prove2x
PCM output6912/6976 vs3456/3488, and three PCM voices sum20736/20928. Catalogue
131songs/35640000frames has peak28530 and0clipped samples, max3PCM/4PSG. The
catalogue and GCC/MSVC normal completion now REJECT nonzero clipping. Completion
still4653ticks/2385notes/3722400mixedframes/7442789nonzerosamples/0clip/errors,
same gameplay results. Actual muted WinMM worker/buffering/mute/music-on/off/
menu/map/restart checks PASS with the new103-library Windows rebuild.
See audio-validation.json, audio-playability-validation.json and Windows
counterparts. No user game process was stopped and ROM data remains unchanged.

Gameplay-effects volume follow-up: audio.c now applies EFFECTS_BUS_GAIN=1.5
(+3.52dB) to song IDs7..6D, covering world/player/enemy effects and footsteps,
both sampled and PSG. This stacks with the existing2x sample gain. Music6E..85
and menu cues1..6 retain their previous gain.350 shared GCC/MSVC reference
cases cover category boundaries, all5 players and5 oscillator types; positional
fixtures verify unchanged distance/pan ratios with the higher effect level.

Direct1.5x effects clipped562 samples in normal first-level combat;1.25x still
clipped31. Final1.5x has effects-only peak protection: sum music/menu and FX
separately after master fade, then attenuate only FX with a common stereo gain
to fit+/-32760. Immediate attack,100ms release time constant, unity snap within
1e-6. Music/menu are never attenuated by this control. effects_headroom_gain
resets1 in ssa_audio_reset; effects_limited_frames is cumulative like clipped.
The private headroom fixture verifies exact stereo outputs32459/32760 for the
overlapping reference mix, unchanged6912/6976 music through recovery, mute/
reset and buffer-size invariance. No sample/instrument normalization added.

GCC ASan/UBSan/leaks:575098 core checks,131songs/35640000frames,peak28530,
0hardclipping/decoder errors. Normal audio-enabled route4653ticks/2385notes/
3722400mixedframes/7442868nonzerosamples,689805effects-limited/recoveryframes,
0hardclipping/errors, same gameplay completion. The previous25%/50% failures
were gain/headroom diagnostics; do not hide clipping by weakening test checks.
The103-library Windows frontend has been rebuilt without changing the ROM.

Final effects checkpoint: MSVC catalogue575098checks, effects_scope_cases350,
gain1.5 and0hardclipping/decoder errors; actual muted WinMM worker/menu/map/
restart/music-on/off tests PASS. Windows audio completion matches GCC exactly,
including689805effects-limited/recoveryframes and0clipping.10 current reports/
559sourcehashes/3binaryhashes audited. No user processes stopped. Older broad
menu/campaign reports were not rerun for this audio-only change.

Latest user request supersedes the original shared-channel allocation policy:
allow music + multiple simultaneous effects; prevent note overlap only within
each music sequence track. Native audio now reserves player0 and voices0..15
for monophonic music tracks. Songs below6E (including UI/footsteps) get32
independent effect instances and64 separate voices. The ROM route is merely a
free-slot hint. Repeated same-ID effects and same-type PSG oscillators coexist.
Release/echo voices keep their instance occupied after sequence completion.
At capacity recycle only effects (oldest instance; voices prefer released,
then oldest), never music. Counters effect_recycles/voice_recycles expose this.
Stop(song) clears all matching instances; move(song,point) moves the newest
positional one. Current callback API has no individual identical-ID emitter
stop handle; current CD loop uses one tracked source. Music slot serials are0
so state/oscillator comparisons against music-only playback remain meaningful.
Keep PCM2x / gameplayFX1.5x and FX-only stereo headroom protection unchanged.

The former hardware allocator moved to tests/audio_channels_oracle.c as an
explicit unused original reference (12000 ROM comparisons). Production note
volume still passes6000 ROM comparisons. audio_concurrency.inc tests five
oscillator kinds, duplicate positioned effects, actual stem summation, unchanged
music state/phase, same-track music replacement, SFX polyphony, saturation,
stop isolation and ended-sequence tail ownership (six scenarios). Core passes
639290 GCC ASan/UBSan/leak and MSVC checks, all131 populated songs/35640000
frames, peak27377,0clipping/errors. Gain reference coverage now2310 cases.
Normal audio-enabled route:4653ticks,2425notes,3722400frames,7443992nonzero
stereo samples,3369142FX-limited/recoveryframes,0clip/errors. Peaks19 effect
instances/15 effect voices/5 music voices; no instance or voice recycling.
Windows agrees, and actual muted WinMM worker/buffering/mute/music-on/off/
menu/map/restart checks pass. Frontend and103libraries rebuilt; no game process
was stopped. Updated README/audio.md describe this requested PC extension.
Original ROM SHA1 unchanged. Older full menu/campaign reports were not rerun.

2026-10-16 source/door audio fix: source-aware SsaNativeAudioOps.source and
ssa_audio_source now carry copied uint64 emitter IDs to the mixer, never live
entity pointers. PLAY overlaps one-shots; LOOP ensures one sequence per emitter
and sound; MOVE updates all attached positional copies; STOP affects only that
source/song (SSA_AUDIO_SONGS sentinel means all its sounds). RETIRE stops owned
loops and detaches finite one-shots at their last position, preserving death
tails. Local weapon stops target player source1, not other emitters' sounds.
Win32 initial/reload bindings both use the new source callback under its mutex.

Native presentation ownership is in src/native_audio.inc. Door movement song67
was left looping after68 because independent effects no longer replaced it.
Each mover now has its own source2+index, a boundary-endpoint centroid and live
floor/ceiling midpoint. Observe mover flag8, deduplicate67, explicitly stop it
before68, reconcile after each tick/end screen and retire on unbind/free.
Original sector_movers.c and enemy algorithms are unchanged. Enemy sounds
follow body centers, with lifetime IDs preventing old sounds following reused
slots. Pickup sounds now use the item's actual world position too. Particle/
impact one-shots stay at their emission point. Listener stays at Sam's eye.

Both original single-owner enemy loops are superseded only at presentation:
every qualifying CD can run3A, every CE can run4E. Preserve their original
phase/dying/distance checks and hysteresis (start1023/399, continue1024/400).
Canonical ROM game.loop_source fields and original controllers remain intact;
legacy sinks still receive their old callback path. Continuous world sequences
3D/51/5F also receive loop ownership if emitted, so retirement stops them.
All retain stereo pan,3D inverse-distance gain, smoothing, PCM2x/FX1.5x and
FX-only peak protection. No HRTF/occlusion/reverb added.

tests/audio_world.inc adds four source/mover/CD/CE scenarios: same-ID source
isolation, deduplication without note/clock restart, movement of multiple notes,
retirement/tails, paired doors with independent stops and reverse commands,
paired CD/CE loops and CE399/400/401 hysteresis. Shared GCC ASan/UBSan/leaks and
MSVC suite:639783 checks,131songs/35640000frames, peak27377,0clipping/errors.
Normal audio route:4653ticks,2439notes,3722400frames,7444001nonzero stereo
samples,3411883FX-limited/recoveryframes,205spatialstarts,4790listeners,
20775sourceupdates. Peaks19effect instances/14FXvoices/5musicvoices, with no
instance/voice recycling. Actual muted WinMM worker/menu/map/restart/music-on/
off checks PASS. Windows audio completion agrees. Both full levels also pass
again on sanitizers and MSVC (4653/11314ticks), plus Windows Enter advancement,
restarts, map music and both renderers. Built103libraries/frontend. ROM SHA1
unchanged; no user game stopped. New include files are included in report hashes.

Door-loop follow-up: the previous fix was incomplete. User reports all opened
doors keep sounding until level end. Actual cause: level_runtime.c routes
player-triggered line sounds through PlayerCall.player_sound_id -> native
player_sound, bypassing mover_sound. It started extra tied67 sequences on local
source1, while the mover-owned copy stopped correctly. The prior paired-mover
test never exercised that start path. Native player_sound now routes IDs67/68
to mover_sound before local/footstep handling. No changes to ROM mover timing,
height endpoints or synth envelopes were needed. The one-frame flag8 at an
exact endpoint is original strict-limit timing, not the persistent leak.

New player_door_fixture in audio_world.inc exercises the real map line with
the player sound bridge, then three open/close cycles, rendered finish cues,
exact zero PCM after completion, and repeated uses of an already open door.
It fails on the old code at source ownership. Full audio-enabled normal-input
route additionally rejects any67 not owned by a valid mover and any67 left on
a stopped mover. Before this fix it accumulated14866 stray-instance frames;
after:394 active mover frames,0stray/0stopped loops BEFORE terminal cleanup.
Both GCC ASan/UBSan/leaks and Windows/MSVC agree. This is the regression that
proves the user-reported persistent sound, rather than only a stop API call.

Current core650703checks,5world/source scenarios +6concurrency scenarios,
131songs/35640000frames/peak27377/0clipping/errors. First-level audio still
4653ticks/3722400mixedframes;2434notes/7442950nonzero stereo samples,
1294034FX-limited/recoveryframes; peaks16effect instances/11FXvoices/5music
voices, no recycling.205spatialstarts/4795listeners/20775sourceupdates.
Updated Windows frontend/103libraries, audio device/mute/menu/map/restart tests
pass. Both native levels also pass again on sanitizers. Keep this player-sound
dispatch fix and the new rendered-silence/normal-route checks in future changes.

Final follow-up checkpoint: Windows two-level completion, original map music,
Enter advancement, level-entry restarts and both renderers pass again with the
corrected player-door callback.13 current audio/map/campaign/build reports
audited against801source hash records and6binary hashes. Original ROM SHA1
unchanged. No user game process was stopped. Physical listening not claimed.

World-sound distance follow-up: doors and pickups already had world positions,
but spatialize kept full volume inside256 world units. Replace that plateau
with continuous256/(256+distance) gain for all positioned effects, shared with
enemies. Half volume at256units, fifth at1024; retain vertical distance, stereo
pan, smoothing and source ownership. Music/UI/local player effects unaffected.
Actual native door67/68 and pickup35 callbacks are now rendered at nine
distances from0to4096, checking decreasing peaks, vertical separation, stereo
direction and pickup position snapshots. Six world/source scenarios now pass.

GCC ASan/UBSan/leaks and MSVC audio core:650941checks,131songs,
35640000frames,peak27377,0clipping/errors. Normal first-level audio route:
4653ticks,2434notes,3722400mixedframes,7442848nonzero stereo samples,
1036048FX-limited/recoveryframes;394moving-door frames,0unowned/0stopped
door loops before terminal cleanup. Peaks16instances/11FXvoices/5musicvoices,
no recycling. Windows103libraries/frontend rebuilt; actual muted WinMM audio,
music/menu/map/restart tests and Windows audio completion pass. Only audio/build
reports refreshed this turn; broad campaign/map/menu suites were not rerun.
Ten current reports audited:568source hash records and3binary hashes match.
Original ROM SHA1 unchanged. Running user game PID48888 was preserved by moving
its mapped image to ssa_native_game.previous-c5725ca90d774711a7c663e0aa79ad98.exe;
do not remove that image while in use. Restart game for new mix. No user process
stopped; physical listening not claimed.

Pickup audio correction: the user now explicitly wants item pickup cues local,
with no distance attenuation or positional panning, like player sounds. Native
pickup_event therefore calls sound(g,id), not world_sound. Retain the source1
local PLAY path (NULL position), legacy play fallback and original track pan.
Door/enemy/impact spatialization and the door67/68 ownership fix are unchanged.
The world-distance fixture still renders real door movement/finish callbacks
over nine distances; pickup35 now renders identical PCM to local playback over
item/listener positions and yaw, on source-aware and legacy sinks.
GCC ASan/UBSan/leaks and MSVC:651003checks,6world/source scenarios,
131audible songs/35640000catalogframes/0clipping/errors. Full first-level audio
route passes both compilers:4653ticks/2434notes/3722400mixedframes,
7442834nonzero stereo samples/1036048FX-limited frames;198spatialstarts,
4788listeners/20775sourceupdates.394active-door frames,0stray/stopped loops;
no instance/voice recycling or clipping/errors. Windows103library rebuild and
actual muted WinMM worker/menu/map/restart/music-on/off tests pass. Broad
campaign suites not rerun. Ten audio/build reports audited against568source
records and3binary hashes; original ROM SHA1 unchanged. Running user game
PID62984 preserved at ssa_native_game.previous-5ffc23bcafd74b71b58b7c3b3289c400.exe
before rebuild; no user process stopped. Restart game to hear updated pickups.

Idle-mouse music slowdown reproduced and fixed in Windows output, not synth
tempo. Old worker polled WaitForSingleObject(stop_event,4); main-loop pumps
provided extra refills on UI/input wakes. New real-device wall-clock checker
without any window/input/main-thread refill measures old music-only idle at
0.833530 speed, frequent main assistance1.000087, idle plus16 looping effects
0.978493. The160ms prior worker-progress check passed despite slowed delivery.
Use waveOutOpen CALLBACK_EVENT with a manual-reset buffer_event and worker
WaitForMultipleObjects(stop,buffer,INFINITE). Reset completion event BEFORE
scanning headers under the existing mutex, retaining completions during scans.
Keep event handle alive through device reset/close; join worker before teardown.
No global timer resolution/priority changes or music/sample-rate changes made.

New tests/test_audio_win32_timing.c and Windows CMake target exercise idle
music, assisted music and idle music with16 looping effects, comparing3seconds
of real device sample-position advancement against QPC elapsed time. Accept
speed0.98..1.04 to allow endpoint/device reporting granularity. Muted actual
device output, no physical listening claim. Post-fix runs show approximately
1.0000 in all cases; scripts/test-audio-timing.ps1 writes source/binary-hashed
analysis/windows-audio-timing-validation.json. New checker executable remains
separate from gameplay. Windows103library/frontend rebuild and existing MSVC
651003check/131song suite plus actual WinMM worker/mute/music/menu/map/restart
checks all pass. Portable synth unchanged; GCC/campaign not rerun this turn.
Original ROM unchanged; no user game process was running or stopped for build.

Automatic fire follow-up: native60Hz calls consumed one round/stat every update
for machinegun4/minigun5 (60/s). Original116E4 intentionally does this per CALL,
while scaling single-player damage by GAME+07 ticks. Original2134 calls244C once
per completed render;21B4 accumulates VBlank ticks at GAME+06 and2134 copies them
to GAME+07 after rendering. There is no recovered fixed ROM rounds/s value.
Do not claim20Hz is exact ROM timing. The native frontend now explicitly opts
into automatic_period3 (20rounds/s), a PC correction to frame-dependent ammo/
attack cadence. Original controller_step/step_free remain canonical and pass
all11652whole-ROM fire/switch/cooldown comparisons,18047reentry checks.

New controller_step_clocked gates only weapon4/5 hit/ammo/stats/flash, keeping
aim/start/release every simulation tick. Each admitted attack uses accumulated
active-tick damage, preserving sustained single-player DPS. First attack is
still the tick after start; release/empty/weapon changes/respawn clear cadence,
pause freezes it, no post-pause bursts. Partial active intervals on release are
discarded. Default automatic_period0 keeps original runtime behavior. Native
movement, chainsaw, discrete weapons and their animations/cooldowns unchanged.

New tests/test_weapon_cadence.c:14cases/6674checks on GCC ASan/UBSan/leaks and
MSVC. Actual native input/gameplay verifies40shots/40rounds over120ticks for
both guns with0/30/60/144/240Hz view preparation; three-tick spacing, prompt
release/repress, pause/input suppression and empty ammo. Four controller cases
check total tick-scaled damage and uninterrupted aim between shots. Reports
weapon-cadence-validation.json and windows-weapon-cadence-validation.json;
Windows command scripts/test-weapon-cadence.ps1. Legacy weapon-runtime suite
still100466checks across16maps,7888SPweapon frames and305MPweapon frames.
Its old Python link list omitted projectile_extended/target/particle_create;
added these existing dependencies so the required test runs again.

Windows103libraries/frontend rebuilt. Audio-enabled first-level completion on
GCC sanitizers and MSVC remains4653ticks/28000score/7pickups/23HP/2434notes,
3722400mixedframes,0clipping/errors/recycles;394moving-door frames with0stray/
stopped loops. Broad campaign and full audio catalog not rerun this turn.
Native audio timing backend unchanged; ROM unchanged; no user process stopped.

Original fire-rate correction (supersedes estimated20Hz above): user rejected
the estimate and requested original FPS-dependent timing. Added a TEST-ONLY
mGBA0.10.5 core checkout, pinned26b7884bc25a5933960f3cdcd98bac1ae14d42e2,
in tools/mgba-timing, with lock metadata. tests/probe_original_frame_rate.c
boots unchanged owned ROM through original menus, selects gun4/5 via RAM
inventory/current weapon/object state, then holds original A. No ROM code
patched. In first-level start scene BOTH runs produce85rounds/85shot-counter
increments in600hardware refreshes;84steady intervals each exactly7refreshes.
Hardware clock is16777216Hz/280896cycles per refresh, so cadence8.532500081/s.
scripts/test-original-fire-timing.ps1 rebuilds/runs the oracle and writes
analysis/original-fire-timing-validation.json including full traces and hashes.
It uses built-in BIOS. This is a measured scene, not physical-hardware proof or
a recovered universal low-FPS constant. Original2134 has one VBlank wait plus
render work, not an explicit7-refresh cap. Other scenes may slow further (or
have different workload); reproducing campaign-wide CPU slowdown is unfinished.
Never link this emulator core into the native port; its generated standalone
CMake project is under build/mgba-timing-src, separate from root CMake.

Native automatic_period7 + automatic_gba_clock1 replace20Hz. Rational phase
maps60Hz simulation to actual GBA timing (seven PC ticks would be0.46% fast).
The first attack now waits one original-frame interval; each round applies7
original ticks of damage. Release/empty/weapon-change/respawn clear phase;
pause freezes it. Muzzle flashes restored to original frame&3 gate, using a
virtual original frame only inside the weapon call, restoring shared frame
even on error. The prior every-attack flash override was also too fast.

GCC ASan/UBSan/leaks and MSVC both pass40016checks/14cadence scenarios.
Each gun85shots/85ammo over600native ticks across0/30/60/144/240Hz view prep;
exact rational spacing (7or8PCticks), first-shot wait, release/repress, pause/
input suppression, empty ammo, damage budgets and21native flashes verified.
Shared weapon frame restores to actual simulation counter after every call.
Canonical controller11652ROM comparisons/18047reentry checks and legacy
weapon-runtime100466checks pass again. Windows103libraries/frontend rebuilt.
GCC/MSVC first-level audio completion still4653ticks/28000score/7pickups/23HP,
2434notes/3722400mixedframes,0clipping/errors/recycles,394moving-door frames
and0stray/stopped door loops. Full campaign/catalog not rerun this turn.
ROM unchanged; no user process was running/stopped for build.

PC quick-click fix (2026-10-16): input already retained down/up between polls,
but automatic startup could cancel before the measured first-shot delay ended.
SsaWeaponRuntime now has opt-in buffer_fire and fire_pending_weapon; native_game
enables it. One pending requested-weapon attack synthesizes held fire through
startup/raising/reload until the actual shot counter increments, then releases
on the next update if the physical button is up. No first-shot speedup. Original
controllers unchanged; default library buffering off. Real release acknowledges
GAME flag400 protection even while retaining the pending shot. Pending cancels
on focus loss/controls disable/pause/death/respawn/input-state3/empty ammo/weapon
change. pc_input_runtime policy clears it on focus/enable gates. It is one
outstanding attack, not an arbitrary click queue. Keep sustained cadence intact.

GCC ASan/UBSan/leaks and MSVC cadence suite now42132checks:14 original cadence/
damage scenarios,8 quick-click cases (weapons1..7/10),8 cancellation cases.
Each click released before poll, during reload or during raising produces one
actual attack, with90 released ticks checking no extra shots. Ammo1/2 is999
because original HUD restores infinite ammo; preserve that behavior. Existing
85rounds/600ticks across0/30/60/144/240Hz view schedules still passes.
PC-input regression304406assertions and legacy weapon-runtime100466assertions
pass. Windows103libraries/frontend rebuilt. Audio-enabled first-level normal
input completion now4977ticks/28000score/7pickups/69HP/13pistol/23shotgun shots
on both compilers: the extra buffered shotgun shot changes bot combat/route.
3981600mixedframes/2623notes,0clipped samples/errors/recycles;394moving-door
frames with0stray/stopped loops. Full campaign/catalog not rerun for this fix.
Current7 relevant reports audit697source hashes and2checker hashes, all match.
Owned ROM SHA1 unchanged. No user process stopped. Reports: weapon-cadence,
windows-weapon-cadence, audio-playability, windows-audio-playability,
windows-native-build, native-weapon-runtime and native-pc-input validations.

Campaign continuation (2026-10-16): playable frontend now admits levels0..2
(first THREE levels), using SSA_NATIVE_CAMPAIGN_LEVELS=3 in native_game.h.
Enter after either first two real exits carries earned value-only inventory/
score/H-mode into the next stable-address game. R restores that map's entry
state; Enter after third exit replays it. Direct --level0..2 accepted. Map,
fade and audio reset composition retained. No new enemy/controller substitute:
the original map2 types CD/CE/CF/D0/D1/D3 were already recovered. Level3+ remain
guarded. The complete original campaign/menus/continues/special weapons8/9/
bosses/presentation parity is still unfinished; do not claim full parity.

scripts/inspect_campaign.py is a read-only, owned-ROM-SHA1-guarded inspection
of all12campaign maps' initial definitions, script spawn lists and actual E1
animation3F exit markers. It is content evidence, not whole-game discovery or
matching coverage. Initial3F exit for map2 is3648,-4224. Maps3/4 have ordinary
supported enemies; map5 scripts include unsupported bossD7 and map11 bossD8.

Refactored second-level test bot into reusable campaign_level_run; its final
inventory is captured before teardown for the third map. It finds actual exit
pickup coordinates instead of hardcoded map1 coordinates, and verifies all
switches. Updated bot's NORMAL inputs to prefer earned guns5/4 over shotgun3/
pistol2 and jump during combat. The previous bot died on map1 with changed
quick-click timing; no production health/damage/AI was changed to bypass it.
No gameplay grants, teleports, script edits or forced defeats. Added
tests/test_third_level.c/.py, CMake MSVC checker and actual local driver command
.\decomp.ps1 test-third-level. test-second-level remains supported separately;
its old report is historical, current three-map proof is third-level-validation.

GCC ASan/UBSan/leaks=1 and MSVC x64 agree over all3maps:
map0:4977ticks/28000score/7pickups/19spawned/69HP/13pistol/23shotgun shots;
map1:8772ticks/108000cumulative score/14pickups/49spawned/52HP/98PCviews,
1306airborne frames/150machinegun rounds/1activatedswitch;
map2:9385ticks/228000cumulative score/30pickups/69spawned/91HP/106PCviews,
1822airborne/4945third-person frames/262machinegun rounds/2activatedswitches.
Both last-map switches latch and real pickup/VM exit_toggle fires. View checks
include reference rendering plus level/pitched PC views. Each replacement has
stable ownership, and carry guards still cover10invalid/late values.

Windows103libraries/frontend rebuilt. scripts/test-native-campaign.ps1 now
compares third-level checker to GCC, then actual launcher/window-message tests
both advancement boundaries and R at levels1/2, PC/reference renderers, real
muted device and music-on/off. Eight frontend stages,4restarts,5map screens,
350frozen map ticks,45map-music checks and5gameplay-music starts per case.
No gameplay during map view; audio errors0. Test-only end-screen/death fixtures
are separate from real bot gameplay proof. Physical devices/listening untested.
scripts/test-native-game.ps1 hardcoded pre-click result was stale; updated its
expected4977ticks/69HP/23shotgun shots to independently verified current run.
Normal startup/launcher/hidden smoke/restart suite passes; Windows cadence still
42132checks,14timing/damage+8quick-click+8cancel cases, unchanged85rounds/600ticks.
No broad audio catalogue or later campaign completion rerun this turn.

Current6report audit:356source hashes/5binary hashes all match; owned ROM SHA1
unchanged. Reports:third-level, windows-native-campaign, windows-native-build,
windows-native-game, windows-native-startup and windows-weapon-cadence validations.
No user process stopped. decomp.wtf live status74/526 exact,2152/87648catalogued
bytes(2.46%),520attempts; no new matching attempt or exact function this turn.

Five-map campaign continuation (2026-10-16, supersedes three-map limit):
SSA_NATIVE_CAMPAIGN_LEVELS=5 now admits maps0..4 in the playable frontend.
Enter advances after four real exits, carrying earned inventory/score/H-mode;
R restores current map entry; Enter after fifth exit replays it. --level0..4
works; usage derives its range from the constant. Map5 (the SIXTH campaign
level) still requires unsupported D7 boss14AB8/1485C. Do not admit it merely by
increasing the constant. Map11 requires D8 boss14648/143B0. The bot expects an
initial E1 animation3F exit pickup; boss maps lack it, requiring their actual
boss/exit behavior, not an invented exit. Full feature parity remains unfinished.

New tests/test_campaign.c/.py and MSVC ssa_native_campaign_check complete every
admitted map consecutively through normal PC input. New actual driver command
.\decomp.ps1 test-campaign. The explicit checker argument2..4 can bound a probe;
default follows the compiled frontend limit. Python requires exactly that many
successful real exits and writes analysis/campaign-validation.json. Old second/
third drivers remain; their old reports are historical, not current source proofs.

Substantial TEST-BOT corrections were needed, not gameplay-rule changes:
visit reachable script-bearing pickups even when inventory benefits aren't
needed; choose reachable switch approach points and finish their sub-cell path;
reachability must recognize already being in the destination cell. Later-map
planning uses4unit clearance, actual radius for approaching a clear graph node,
8unit walk checks, cached edges invalidated by sector heights/body dimensions,
and stable waypoints. Raised-floor tangent routes otherwise caused oscillation.
Collision already uses a swept bounds box; do NOT describe it as destination-only.
The earlier commentary theory about skipping thin walls was not the final cause.
Graph memory is freed between maps and in first-level teardown. All changes
are in tests, using separate map/collision scratch, never writing game bodies,
inventory, health, damage or scripts in the actual completion route.

Later-map bot uses normal vertical mouse input to visible center/upper body,
retreats from ledges, engages every visible target within its sight probe, uses
earned rockets on groups and chooses clear keyboard escape directions from
nearby enemies. Its prior1300unit engagement gate hid a visible D4. Diagnostic
finite/unlimited ray and actual weapon-runtime checks found no firing bug;
test probe is in .cache/probe_pitched_trace.c, not a shipped target or new ROM
proof. Actual enemy AI/combat/physics remain unchanged by this continuation.

Preserve original script OPEN semantics: map3's opening armor pickup at384,6592
(item2F, trigger101) starts its script and opens mover0/sector84. OPEN retains
the lock bit4. Validation observes the door gaining clearance, not an invented
unlock. The fourth map's real exit needs only2of3 switches; unused third switch
is optional for this completion path. The fifth map has no wall switches.
Do not force optional switches or alter original scripts to make a bot pass.

Final GCC ASan/UBSan/leaks=1 and MSVC match on all five maps:
map0:4977frames,28000score,7pickups,19spawned,69HP,13pistol/23shotgun shots;
map1:9178frames,110000cumulative score,16pickups,49spawned,65HP,102PCviews,
1332airborne,150machinegun rounds,1switch;
map2:10557frames,236000score,24pickups,73spawned,95HP,118PCviews,
2278airborne,5397third-person,287machinegun rounds,2switches;
map3:11095frames,432000score,22pickups,104spawned,100HP,124PCviews,
1173airborne,5335third-person,3541pitched frames,210machinegun rounds,2switches;
map4:11836frames,636000score,24pickups,119spawned,56HP,132PCviews,
1190airborne,5880third-person,3272pitched frames,20machinegun rounds,0switches.
All exits are original pickup/VM exits. Ten original carry guards still pass.

Win32 transition_check test-only fixtures now scale to the admitted map count;
they remain separate from real-input completion proof. Actual launcher checks
all4advancements and R on maps1..4 in PC/reference renderers and music-on/off
with a real muted device:16stages,8reloads,9map screens,630frozen map ticks,
81map-music checks,9gameplay-music starts per case,0audio errors. No gameplay
during map presentation. Physical input/listening are not claimed. Windows
103libraries/frontend rebuild and normal startup/launcher/hidden smoke pass.
Windows fire suite still42132checks (14cadence/damage+8click+8cancellation).
First-level audio GCC/MSVC remains4977ticks/3981600mixed frames/2623notes,
0clipping/errors/recycling;394moving-door frames,0stopped/unowned loops.
The broad audio catalogue/remaining campaign were not rerun.

Six current-report audit:562source hashes and5binary hashes all match; original
owned ROM SHA1 unchanged. Audited campaign, windows-native-campaign,
windows-native-build, audio-playability, windows-audio-playability and
windows-weapon-cadence reports. Normal startup/game reports also refreshed.
User game PID33516 was preserved for rebuild at
build/native-windows/Release/ssa_native_game.previous-1893b347e221418b860c09eacf131bc3.exe;
no user process was stopped. Do not remove a preserved image while in use.
Live decomp.wtf status remains74/526 exact functions,2152/87648catalogued bytes
(2.46%),520attempts. No new matching attempt/exact function this turn.

Map-to-level fade restoration: src/map_view.c now finishes the outgoing map
at solid black rather than the BLDY15 residual brightness. Shared composition
and the recovered61-tick fade/input state machine remain unchanged. Win32 keeps
map_active through the terminal render/present and breaks simulation catch-up
at that boundary. Reset the presentation clock AFTER the completed black blit;
the next level bootstrap waits a fresh60Hz interval. Do not finish_map in the
same step that first sets map_view.done, or catch-up can erase terminal black.
Map music stays on the map; level audio/initialization still begin at handoff.

Validation: test-map-view passes8084 ASan/UBSan/leak checks, including midpoint
dimming and every endpoint pixel in both artwork families/music modes. Original
test-map-view-rom still passes864 glyph frames,4147200 background pixels,
691200 composed pixels and8976 fade/input frames. Windows103libraries rebuild.
Startup/restart and concurrent launcher tests pass both renderers. Campaign
suite again completes all5maps with unchanged normal-input results; each actual
launcher renderer presents9 terminal black map windows and531 intermediate fade
frames,630 frozen map ticks,81 map-music checks,9 level-music starts,0 audio errors.
The startup test scans every composed terminal-window pixel before permitting
handoff. See map-view.md and current map-view/windows-native-campaign/startup
validation reports. Owned ROM remains unchanged; no matching work this fix.

Boss continuation (2026-10-16): enemy_boss.c recovers BOTH complete D7/D8
dispatchers14AB8/14648, including all8states, actual five-shot cycles, reinforcement
script requests, hurt/death and corpse release. Read analysis/enemy-boss.md.
The generic enemy runtime/entity dispatcher now handles CE..D8, except CD retains
its dedicated adapter. Native HUD binds spawn.game.boss to canonical actor health.
104Windows libraries/frontend build. SSA_NATIVE_CAMPAIGN_LEVELS remains5:
boss-map normal-input progression/exits are NOT established. The older instruction
that D7/D8 controllers are unimplemented is superseded; do not admit map5 merely
because the controller now works. Its original encounter is script10; map11's
is script6. They spawn their bosses later, not in the initial idle map objects.

Outer boss timer ages before every handler; activation/corpse also age internally.
Preserve that double decrement. Saved facing uses activation.saved_facing(+48),
not generic navigation.heading; boss summon_cycles(+52) lives in runtime.bosses.
Animation index(+5C) is actor.animation_frame, not displayed image frame. D7 launches
after index8,D8 after6; both create6A,96units forward/64units high. Constructor,
sound, vertical velocity assignment occur in that order. D8 sets projectile state1;
D7 keeps0. Private projectile create/commit services sync this to owned actors.
Both bosses turn toward player BODY but damage reactions face player RECORD XY.
Phase table group bytes are already zero-based alive indices; don't subtract1.
Boss queues execute in normal entity-before-VM order. Group notification/timer720
precede death sound/music20; the NEXT corpse update clears spawn.game.boss.
Native allocation exhaustion returns an error instead of original NULL dereference;
prefix effects/allocations persist. No fabricated exit/reward was added.

New actual driver commands: test-enemy-boss and test-enemy-boss-runtime. Original
oracle passes4948 whole-dispatcher comparisons,5708reentry guards,22effect failures,
276projectile creations/477script requests. No emulation in production. Integration
passes97763 ASan/UBSan/leak assertions on16real layouts:30688 attack-cycle frames,
160shots,32script queues,32hurt-callback failures,32natural-animation death/releases,
832observer/reentry events. Cross-map actors/script banks are explicit testfixtures;
projectiles are inspected then released, not a full boss fight. A separate full
native_game fixture reloads unchanged ROM data, queues original encounter10/6 on
map5/11, runs2400actual frame phases/2304bossHUD frames/20reference draws, idle input.
This verifies dispatcher/VM/HUD composition, NOT natural boss-map completion.
Windows boss checker matches every GCC count. Run scripts/test-native-boss.ps1;
reports enemy-boss-rom-validation.json,enemy-boss-runtime-validation.json and
windows-native-boss-validation.json. Five-map normal-input GCC+MSVC campaign stays
identical, including earned carry, jumping/H-view/vertical aim. Actual Windows
campaign/restart/map/music/black-fade transitions pass both renderers,9maps each.

Actual decomp.wtf claim/compile/release for14804 wait helper used2manual candidates:
88.0%,then89.5%, NOT exact. Best in recovered/enemy_boss_wait.c; claim released
with load-order/regalloc notes. Live matching remains74/526,2152/87648catalogued
bytes(2.46%),522attempts. No new exact match/export change. ROM file unchanged.

Visible map-to-level transition correction (2026-10-16): user reported the old
single black frame/full-brightness level pop was still not noticeable. Win32 now
presents each outgoing map fade tick separately, holds terminal black15 displayed
ticks (minimum250ms), then reveals the frozen level image over60 displayed ticks
(minimum1second). Recovered map_view.c state machine/art/timing is unchanged.
These added PC presentation phases supersede the older one-tick handoff notes.
Clocks restart AFTER each completed GDI window blit; no catch-up batch may consume
outgoing/hold/reveal phases. Actual default-timer measurements on this machine
are about0.49s black/1.85s reveal. Do not describe nominal tick counts as exact
wall-clock durations. Level music starts at handoff, after the map's hold. One
bootstrap builds valid level render state, then gameplay stays at counter1 and
input/capture wait until full brightness. Fade operates on completed offscreen
window pixels (including HUD/text); it never darkens source render buffers or
reruns gameplay/rendering during the reveal. M and Esc remain usable.

Windows startup/restart/concurrent launcher checks pass. Campaign again completes
all5maps and all9 map transitions per renderer with unchanged simulation results;
630 original map ticks,135 black-hold ticks,540 reveal ticks,9 level-music starts,
531 visible intermediate reveal presentations and0audio errors per renderer.
Timing minima are measured for EVERY map; both renderer cases pass >=250000us
black and>=1000000us reveal. With SSA_TRANSITION_CAPTURE_DIR set, --startup-test
--transition-test writes first-map black/mid/full completed-window BMPs. The
campaign script compares EVERY RGB sample against black/half/full brightness,
requires a nonempty level image, records six artifact hashes. Actual PC stages
were visually inspected. Paths: build/native-campaign/{pc,reference}-{black,mid,full}.bmp.
Current evidence: windows-native-startup/campaign/build-validation.json.
No portable gameplay or original ROM behavior was changed by this correction.

User started game PID46244 during compilation; it was preserved without stopping
it at build/native-windows/Release/ssa_native_game.previous-bd8f2dd73efa4eb9917720c798a2dcac.exe.
The standard executable path was rebuilt. That running session stays on the old
image; user must relaunch play-native.cmd for the new transition. Do not delete
the preserved image while in use. No matching attempts this fix.


Continuous weapon audio / rocket correction (2026-10-16): native_audio.inc now
tracks the local weapon loop on source1 and explicitly stops minigun19 before
wind-down1A (chainsaw12 before13). End-frame sync gates chainsaw/machinegun/minigun
loops on held fire, current/requested weapon, ammo, focus/input, pause, health and
music mode; unbind stops the tracked loop too. Music and unrelated source voices
survive cleanup. Canonical weapon_controller.c and original cadence are unchanged.

NativeGame opts into projectile_runtime.ballistic_player_rockets. Player type65
COMMIT clears attachment4000 immediately; initial XYZ/velocity and owner exclusion
100/player_index remain intact. Future player turning/moving cannot steer the
rocket. Default recovery runtime stays original; enemy rockets/types unchanged.
See analysis/weapon-fixes.md. Actual driver test-weapon-fixes and Windows checker
pass126198assertions:46real-mixer loop cases,6launches/36flight frames in both
views with yaw/pitch/walking/jump,960clipped-column cases. GCC ASan/UBSan/leaks=1;
MSVC agrees. test-world-raster67678ROM and test-wall-renderer14322ROM still pass.
Default projectile-runtime regression passed37132assertions this turn before the
separate raster fix; its broad historical source hash set is consequently older.

Normal-input5map campaign passes GCC ASan/UBSan/leaks and MSVC with the unchanged
bot. New ballistic flight changes map3 to11917frames/448000score/22pickups/100HP
and map4 to12194frames/660000score/23pickups/75HP. First3maps unchanged. The new
map4route exposed reference portal upper end==bottom==120: preserve original
sample count and signed final-pixel correction, discard only offscreen trailing
row through ssa_world_raster_column_clip_bottom. No scene layout changes and no
relaxed ordinary raster guard. Every visible byte matches padded original raster;
output suffix stays untouched. Keep this guard fix when changing renderer code.
Windows cadence42132checks and audio-enabled first-map4977ticks/3981600mixed
frames/2623notes pass. No new matching attempts or campaign admissions this fix.

Final Windows campaign/launcher regression passes5maps,9map views per renderer,
all Enter/R boundaries, original music handoff and black/reveal timing/pixel
checks. Current focused/campaign reports match all recorded sources and the
standard executable. Relaunch play-native.cmd for the new weapon fixes.

Weapon visibility during map-to-level reveal (2026-10-16): Win32 finish_map now
places the weapon at its normal ready Y=0x5000 before the single bootstrap tick,
with gameplay.weapon_action and weapon_view.y mirrors synchronized. That tick
builds native animation/cache/OAM with the gun already on screen. The frozen
level image includes the weapon from the first visible reveal frame; there is
no delayed post-fade raise. Third-person weapon hiding still follows the normal
bootstrap visibility gate. Map fade/black hold/reveal durations, music handoff
and gameplay freeze are unchanged. Recovery controllers/native simulation code
are unchanged. Startup diagnostics validate ready Y/screenY80, visibility flags
and resident image on each first-person map reveal; campaign script requires
reveal_weapon_checks==9 for each renderer. PC midpoint capture was inspected and
contains the weapon at half brightness.

User session PID42896 was preserved without stopping at
build/native-windows/Release/ssa_native_game.previous-b10ac86e4f1c4bb7b913a35332bcfadb.exe.
The standard executable has been rebuilt; relaunch for the visibility change.

Final Windows campaign check passes:5native completions,9weapon-ready reveals
and all map/music/restart/fade/pixel checks per renderer,0audio errors. Current
windows-native-campaign-validation.json matches source and executable hashes.

Sixth campaign level admission (2026-10-16): SSA_NATIVE_CAMPAIGN_LEVELS is now6.
Normal-input bot includes D7/D8 targets and does not require an initial exit E1;
map5 ends through its real VM progression. The bot never queues an encounter,
grants ammo, changes health/AI, defeats groups or teleports. Consecutive earned
carry completes map5 in11295frames,866000cumulative score,12pickups,140spawned,
94HP. First5 outcomes remain unchanged. Actual evidence:1037boss lifetime frames,
763matching bossHUD frames,772boss-projectile frames,752hurt frames,defeated1.
The boss dies before reinforcement-wait in this route (wait_frames0); do not
claim natural summon-phase coverage. Separate controller fixtures cover summons.

Sixth-map campaign validation mixes9036000stereo frames at48kHz, observes battle19
and post-boss20 music once each,0decoder/callback errors. Native audio ownership
now treats music2/19/20 as gameplay for weapon/enemy/mover loops and door endings.
Previously the2-only gate silenced those effects during boss/post-boss play.
Focused test-weapon-fixes now passes126730ASan/UBSan/leak and MSVC assertions:
46old loop stop cases plus4boss/post-boss loop cases with both sink contracts,
6rocket trajectories,960raster clips. Canonical controller/ROM code unchanged.

CMake second/third/campaign test targets now link ssa_audio for the real-mixer
fixture. Windows campaign script compares boss/audio evidence with GCC and uses
6level-derived expectations:11map views,770map ticks,165black hold ticks,660reveal
ticks,649intermediate reveal presents,11weapon-ready checks,20Enter/R stages,
10restarts per renderer. Remaining maps6..11 (includingD8) are still unadmitted;
original interchapter/cutscene/menu/parity work remains. See analysis/enemy-boss.md
and current campaign/weapon-fixes validation JSON. No new matching attempts:
live decomp.wtf status74/526,2152/87648catalogued bytes(2.46%),522attempts.

Final fresh GCC six-map ASan/UBSan/leak run and Windows six-map/audio/launcher
regression both pass. Each renderer validates11map/weapon reveals and0audio
errors. Current campaign, focused and build report hashes match all recorded
sources and the standard game executable. Original ROM SHA1 remains unchanged.

PC sky half-height correction (2026-10-16): sky_row in pc_renderer.c now fits the
actual60-row backdrop to ceil(height/2), clamps Y to the last displayed upper
row, and repeats that per-column bottom pixel through the lower half. This
reduces vertical stretching, retains straight screen-facing panorama axes and
fixed framing under pitch, and handles odd/tiny heights without division by0.
Both level ray columns and cached pitched sky axes use this same sampler.
Geometry/actors/HUD/depth still occlude the background normally; original renderer
and ROM routines are unchanged. Existing flat-sky stripe expectations updated.

test-free-look passes1493513ASan/UBSan/leak assertions,30100depth comparisons,
43pitched views and18280HUD pixels. test-pc-renderer passes11169482assertions,
7sizes through4K,24chase/jump frames;133287pixels differ from reference upscale.
Windows free-look/4K/chase/reference and8resolution/auto-resize/4reject checks
pass. Fresh look-up.bmp was inspected: compressed top artwork and vertical
column trails are visible. Current free-look/pc-renderer/Windows render/build
reports match recorded sources. Standard executable rebuilt; relaunch required.
No campaign/AI/audio/control code changed and no new matching attempts.

Live level reveal and 3/4 sky (2026-10-16): latest user request supersedes the
older frozen-reveal/half-height notes. PC sky rows use height-height/4 (ceil3/4),
then repeat each column's last displayed sky pixel through the bottom quarter.
Shared level/pitched sampler, panorama and geometry occlusion are unchanged.

Win32 capture/buttons/keys now work from the beginning of the LEVEL reveal;
the outgoing original map and black hold still own map music and no gameplay.
finish_map places the weapon ready, bootstraps once and drains normal sprite
cache prepare/OAM/flush passes without extra world ticks so the complete HUD is
available before the first visible level frame. Gameplay/input/AI/animation and
HUD/rendering update throughout the reveal. Normal simulation catch-up is kept;
one displayed fade step cannot skip other steps. Fade timing uses its own
elapsed time AFTER black was first displayed, independently of simulation time
spent preparing that first frame. Minimum1second reveal and black hold persist.
Esc during reveal releases capture and it is not forcibly recaptured at the end.

Startup/campaign fixture verifies initial HUD residency, entry carry BEFORE live
simulation, and real W input moving each map during its reveal. Later gameplay
can legitimately change inventory/health, so shell handoff checks use the saved
entry values. Diagnostic frames excludes reveal simulation; reveal_game_ticks
records that work explicitly. Live transition snapshots include paired unfaded
-source.bmp images per phase. Pixel checks compare the faded frame to ITS OWN
current source (black0/mid1/2/full1), never to a different simulation frame.

User session was preserved at
build/native-windows/Release/ssa_native_game.previous-64a514a818e64b1da27de9bdc2148cd7.exe.
Do not stop it or delete its image while in use. Relaunch standard play-native.cmd
for the new sky and live-reveal behavior. No native gameplay recovery or ROM
changes;6campaign maps remain admitted.

Final live-reveal regression passes6native levels and all11map transitions per
renderer. Both require11real movement checks,11initial HUD checks and11entry
carry checks; no audio errors, measured reveal>=1second, paired live-source fade
pixels all match. Portable renderer/free-look ASan/UBSan/leak and current Windows
free-look/campaign/build report hashes verified. No further tests required for
this change; standard executable is ready to relaunch.

Local player sound priority (2026-10-16): audio.h/SsaAudioPlayer.local_priority
marks effects admitted without a position (native source1 pickups/weapons/hurt/
footsteps; menus too). World play_at/source point effects remain lower priority.
At full32-instance or64-voice pools, recycle world before local; equal class
keeps oldest-instance and released/oldest-note rules. Incoming world effects or
notes drop non-fatally if only protected local resources remain. Music player0
and its16voices are never candidates. Release/echo retains local ownership.
Counters effect_drops/voice_drops expose overload without decoder errors.

Peak protection now reserves music/menu headroom, then local effect headroom,
then world headroom with independent linked-stereo world_headroom_gain. Enemy
bursts cannot reduce the local effects_headroom_gain or leave a local recovery
fade. Existing bus boosts, note arithmetic and positional mix stay intact.
No unbounded allocation; user explicitly allowed prioritization instead.

New tests/audio_priority.inc adds7stress cases; include it in report hashes.
128world-start floods preserve all protected player records/voices, released
tails, music state; new local sounds reclaim world slots; both pool-full drop
paths and recovery pass. Loud world mixes cannot change local gain, and removing
the crowd immediately matches music/local-only output. test-audio GCC ASan/UBSan/
leaks and MSVC agree:653659assertions,131audible songs,35640000catalogue frames,
0clipping/decoder errors. Original ROM oracle envelope/priority/volume/policy/
wave tests still pass. Windows audio device+restart with/without music passes.
Windows audio-playability4977ticks/3981600mixed/2623notes,0errors/clips passes.
Focused weapon-fixes126730assertions and Windows checker pass; minigun stop,
boss music loops and independent rockets remain correct. Build104libs rebuilt.

Preserved running user image:
build/native-windows/Release/ssa_native_game.previous-0a2d86038fd04dd5b64e762177ed4c93.exe.
Do not delete it while in use. Relaunch standard play-native.cmd for new audio.
Original ROM SHA1 unchanged.6maps admitted; no new matching attempts.

Requested weapon cadence changes (2026-10-16): native_game enables
SsaWeaponRuntime.double_minigun and clocked_chainsaw, retaining automatic_period7
and automatic_gba_clock1. Minigun doubles rational GBA phase advancement, not
damage per round:17.065000163 rounds/s,28SP damage per round, one ammo deduction.
Tommy gun remains8.532500081/s. Chainsaw shares that interval and applies7SP
damage per bite (original scaling gives10Easy/7Normal/4Hard), retaining96-unit
melee reach and infinite ammo. Its old damage1 rounded to0 on Hard.
Canonical step/step_free and default library runtimes retain ROM behavior;
only clocked controller attacks defer1/4/5, aim/start/release still run every tick.
double_minigun applies in GBA-clock mode; flags default0 and validate<=1.

Windows and GCC ASan/UBSan/leaks cadence:54032checks,21timing/damage cases
(chainsaw/Tommy/minigun at0/30/60/144/240Hz views),8quick-click,12cancellation.
600simulation ticks admit85chainsaw/Tommy attacks,170minigun rounds,7/8 versus
3/4tick spacing. Pause/release/empty-ammo/input cancellation remain covered.
test-weapon-runtime adds6actual native map hitscan/combat cases for chainsaw:
3difficulties x original/free aim, first8ticks no damage, tick9 real health loss.
101373assertions pass; original controller11652ROM comparisons still pass.
Focused sound-loop/rocket tests126794checks pass on GCC and MSVC (46loop cases,
6rocket launches/36flight frames).104Windows libraries/frontend rebuilt.

Old campaign bot died in map6 at8531ticks after doubled minigun changed route/
ammo use. Preserve build/native-campaign/firerate-initial-completion.* logs.
Test-only input selection now uses an earned shotgun against targets<700units;
no gameplay rules/grants changed. GCC ASan/UBSan/leaks and MSVC both complete
all6maps, with unchanged first3 outcomes. Latest map3:13002ticks/436000score/
20pickups/104spawns/100HP; map4:12649/656000/26/119/91HP; map5:11331/882000/
13/128/61HP. Boss668frames,383HUD,494projectile,376hurt,0wait,defeated1;
music19/20 once each;9064800stereo audio frames,0errors. Current reports bind
source hashes; no ROM changes or new byte-matching attempts.

Final Windows launcher/campaign passes: all6normal-input completions match GCC;
both renderers pass11map/reveal presentations,20Enter transitions,10restarts,
11HUD/movement/entry checks, fade pixel checks and music handoffs,0audio errors.
Current source and executable hashes verified. Relaunch standard play-native.cmd
for doubled minigun and clocked, damaging chainsaw; no further tests required.

Special weapons / palette continuation (2026-10-16): native weapon8/9 callbacks
are now connected. weapon8 forwards request80000009 to damage.hurt_flash and
uses the existing native zero-radius1000-damage explosion and120tick cooldown.
Keep original reject-sector selection, CD..D6-only targets and player/boss
exclusions. weapon9 forwards40000009 and routes EFFECT_ALLOCATE/ANIMATE/INSERT
through projectile_runtime's private weapon composition, inside combat locks.
New E3 takes a real shared slot, target XY and Z+height+400, original60/61/62
animation/radius, flags20, height0, zero velocity/state/timer. Its source is the
target; owner remains0 and weapon9, preserving original no player kill credit.
Do not invent a player owner. Native11424 falls, lands, then particle7 BEFORE
explosion on next update, then deferred release. Targetflag800 remains original.
Full-pool9 consumes its shot/ammo/flash/targetflag but creates no effect, as ROM.
Allocation failure/error prefixes retain earlier effects and stable ownership.
Registered target/slot/active-order/animation/unlinked guards are enforced.

New palette_effect.c / include/ssa/palette_effect.h recovers the BACKGROUND
palette phase of0750C, using ROM data tables for both map families. Preserve
request high bits and byte-counter wrap, count1 restore versus count0 backdrop
clear, flag8 override and bit20000000 table progression. The shared canonical
request is damage.hurt_flash, fixing the previously permanent hurt-sound gate.
Advance once after each native game tick, including paused ticks; never on extra
render draws. Both reference display and PC world/actor palette use its colors;
object/HUD palette remains separate. This is per-call original state behavior,
not scene-dependent GBA rendering-duration reproduction or full0750C recovery.

Driver commands test-palette-effects and test-special-weapons added. CMake105
native libraries plus ssa_native_special_weapons_check, and script
scripts/test-special-weapons.ps1. The palette oracle executes whole0750C with
only15698 blend-register setup and hardware512byte DMA controlled:7936ROM calls,
2031616palette entries on16maps,6atomic malformed guards. No byte match claimed.
Special weapons GCC ASan/UBSan/leaks and MSVC agree:10159checks,16blast+16fall
cases,1real full-pool case,4native quick-click/input cases (both weapons/views),
33fixture requests/16fall particles. Native frontend fixtures reposition an
original CF off its ledge using public remove/insert; they retain recovered
hitboxes and AI. These grant test weapons and are NOT campaign acquisition proof.
Two real native self-damage hits verify flash expiry and repeated hurt sounds.
Repeated reference/PC draws cannot advance palette requests/counter.

Projectile runtime37132assertions, focused weapon loops/rockets126794checks
still pass. Six-map GCC ASan/UBSan/leaks completion retains prior exact gameplay
results, including882000score/61HP and natural D7 defeat. No new admission:
seventh-map probe reached late waves without runtime error but bot died at11764
ticks (1076000score,6pickups,112spawns). Preserve seventh-probe.* logs under
build/native-campaign. Pickup29 is weapon7 (conversion), NOT weapon10; pickup26
is weapon10. Do not raise campaign count based on library loading or fixtures.
See analysis/special-weapons.md and current report hashes. Live matching stays
74/526,2152/87648catalogued bytes(2.46%),522attempts; ROM SHA1 unchanged.

Final current-code verification:105-library Windows build and native executable
hashes verified. All6campaign completions match GCC ASan/UBSan/leaks; both Windows
renderers pass11map/reveal sequences,20advancement steps,10restarts, live HUD/
movement/entry carry and fade pixels/music,0audio errors. Latest portable
free-look1493513checks/30100depth/43views/18280HUD pixels and Windows4K/up/down/
chase/reference smoke pass. New weapon/palette, campaign, renderer and build
source manifests verified. No further tests required for this milestone.
Next: seventh-map normal-input completion/acquisition of conversion weapon7,
original continue/pause/presentation flow, remaining original display/HUD effects.
Keep unsupported type64 portal relocation explicit; full feature parity remains
unfinished. Relaunch play-native.cmd for current build; campaign admission stays6.

Latest chainsaw request (2026-10-16) supersedes its earlier Tommy-cadence/96reach:
user explicitly confirmed192-unit PC reach. Native frontend chainsaw_range192
scales BOTH aim and damage traces (including half-range retries), keeping ROM
rule range96 unchanged and preserving finite obstruction/range limits. Chase
camera retains the common unlimited sight ray, independent of melee range.

Extended pinned original-ROM mGBA timing oracle to weapon1. Its unchanged first
level trace gives86attacks over600refreshes;85steady intervals span584refreshes
(74x7 and11x6), measured steady mean8.693214980/s. This is scene-specific average,
not an original constant cap or exact reproduction of rendering-cost jitter.
SSA_CHAINSAW_GBA_CADENCE=2 uses rational85/584 GBA clock; mode1 retains prior
Tommy cadence, mode0/default canonical per-call. Mode2 requires GBAclock. Keep
automatic_period7 for guns, double_minigun unchanged17.065000163/s. Fractional
automatic_damage_phase supplies6/7damage ticks totaling584per85bites; budgets
Normal584/Easy839/Hard340. Clear on release/switch/respawn, freeze on pause.
Original controller, damage scaling and infinite chainsaw ammo stay intact.

Current MSVC/GCC ASan/UBSan/leaks cadence54045checks:21timing/damage,8click,
12cancel cases,0/30/60/144/240Hz views.600simulation ticks:chainsaw86attacks,
Tommy85,minigun170. Six new real map range/damage fixtures (3difficulties x
original/free aim) hit160 where96misses, miss an unobstructed256 target, verify
complete85-bite damage budgets. Legacy six damage cases remain. test-weapon-runtime
now133923assertions/11578weapon frames;16-map original/default and multiplayer
proof persists. test-unlimited-range1188checks, test-weapon-fixes126794checks
and MSVC loop/rocket checker pass. Windows4K/up/down/chase/reference smoke passes.
105-library Windows build rebuilt; targeted reports/source hashes and ROM SHA1
verified. Full six-map campaign was not rerun for this isolated chainsaw change.

Preserved user game PID5020's running image at
build/native-windows/Release/ssa_native_game.previous-d94c2545fdbc46f58d64a6ae1f84bbcb.exe.
Do not terminate the session or delete its image while in use. Relaunch standard
play-native.cmd for new range/rate. No campaign admission or matching changes.

Mouse wheel now cycles weapons in the Win32 native frontend: up previous, down
next, wrapping IDs1..10 and skipping unowned/zero-ammo slots. Native helper
ssa_native_game_cycle_weapon starts at requested, allowing rapid scroll during
lower/raise; huge signed step counts are bounded and INT_MIN-safe. Existing
select API commits through original switching. High-resolution wheel remainder
accumulates and clears on release/focus loss or inactive input. Gameplay capture,
live player, focus/enabled and unpaused state gate WM_MOUSEWHEEL. Menus/map ignore
it; live level reveal accepts it with normal controls. Number keys unchanged.

Windows105-library build passes. test-weapon-wheel.ps1 uses a hidden real Win32
window and actual proc, no physical input/cursor capture:78checks include rapid
selection, partial/multiple/reversed deltas, wraps, empty/unowned skip, completed
native switch, focus reset and inactive states. Source/binary hashes and original
ROM SHA1 recorded in analysis/windows-weapon-wheel-validation.json. Frontend
up/down/chase/4K/reference-renderer smoke passes. Full campaign not rerun for this
isolated PC control addition. User game PID5020 was left running; relaunch
play-native.cmd to use the rebuilt executable. No matching/campaign changes.

Latest minigun request (2026-10-16) supersedes the earlier requested2x rate.
Frontend now explicitly sets double_minigun=0: measured original7GBArefreshes
per round,8.532500081/s, matching Tommy. Original-ROM timing oracle rerun passes
all3weapons; minigun85shots/600refreshes,84steady7refresh intervals. Preserve
original damage28single-player, quick-click guarantee, loop/wind-down behavior,
chainsaw85/584clock and192reach, and mouse-wheel controls. Library double_minigun
remains an opt-in extension; canonical controllers and ROM remain unchanged.

Windows105-library rebuild and GCC ASan/UBSan/leak cadence53141checks pass:
21timing/damage cases at0/30/60/144/240Hz views,8quick-click,12cancellation.
Minigun now85rounds/600simulation ticks with7/8tick gaps, matching Tommy;
chainsaw remains86. Windows real-mixer weapon loops/rocket regression passes
126730checks/46loop cases/6trajectories. Full campaign not rerun for this isolated
PC cadence policy change; existing campaign reports describe their recorded
earlier settings. No campaign admission or decomp matching change.

Preserved running user game PID15500's image as
build/native-windows/Release/ssa_native_game.previous-58d3ae9baba74ba286bb8c8bf13ed26e.exe.
Session remains running. Relaunch play-native.cmd for current minigun rate.

Q now requests the last actually equipped weapon, not the numerically previous
ID. Native previous_weapon updates only at a successful living-player commit,
clears on death, and carries in SsaNativeCarry (0none,1..10). Unowned/empty last
weapons do nothing. Frontend suppresses Q repeat, menus/map/pause/inactive input.

Frontend finish_fire_before_switch=1 gates all pending handoffs while flag400
firing is active. Discrete firing/reload completes normally before lowering.
Continuous weapons coast without more attacks to the current animation-cycle
boundary, then call the canonical stop/wind-down. Reset sticky loop done only
on wait entry; global animation still ticks afterward. Pause freezes waiting.
Queued selections can change destination. Selecting the firing current weapon
again cancels pending lowering without clipping. Impact effects use current,
not requested, while old weapon fires. Library option defaults off; recovered
controllers and ROM unchanged. Preserve held-fire buffering when restoring
temporary controller held changes. switch_wait resets on respawn.

Windows105-library rebuild passes. New tests/test_weapon_switch.py and
scripts/test-weapon-switch.ps1 (MSVC) agree3295checks:20cases/all10weapons in
both views,506waiting frames, pause, release, empty ammo, replace/cancel requests,
history availability/commit/carry. GCC ASan/UBSan/leaks. Hidden actual Win32
wheel/Q proc checker now158checks including repeat and inactive states, with
no physical device/cursor injection. Current cadence53141checks/21cases/8click/
12cancel and default weapon_runtime133923assertions/11578weaponframes pass.
Windows real mixer weapon-loop/rocket checker126732checks/46loops/6trajectories
passes; up/down/chase/4K/reference frontend smoke passes. No full campaign rerun
or matching changes for this PC handoff policy addition. PID15500's existing
game remains running from its preserved image; relaunch play-native.cmd for Q
and animation-gated switching. New reports: weapon-switch-validation.json and
windows-weapon-switch-validation.json; current wheel/cadence reports refreshed.

Latest port continuation: type64 relocation is now native. Read
analysis/sector-clip.md. sector_clip.c recovers D000 wall selection, CE50 clipping
and053D0 geometry. Dedicated local scratch; original trig/dot/BIOS quotient,
signed wrapped abs, strict best-distance ties,48step equality and100unit opening
threshold,3unit correction bias, repeat-tag stop and BSP ordering are retained.
053D0 adds velocity ONCE without ticks, radius32, clampsZ to sector bounds with
NO body-height subtraction. Outer10D7C already adds velocity*ticks, so preserve
combined(ticks+1) motion. Body floor/ceiling/step and block-grid links DO NOT change.

Only nonzero CE50 correction unlinks/relinks sector nodes (D214 before XYZ
commit, D1B4 after clamp). Zero correction can update geometry sector while the
registered sector-list node remains in its old list. New ssa_world_sector_move
supports this; world remove/replace locate the actual head when geometry sector
differs. Never substitute ordinary world remove/insert for this path. Stable
owners/query contracts remain. Pure geometry failures are atomic; runtime keeps
the outer controller prefix.1024iterations bound original pathological loops.

Shared projectile constructor now accepts64; runtime binds real relocation;
entity_runtime dispatches10D7C. No new original creation site or campaign
acquisition is claimed. Controlled construction fixtures do not admit new maps.
Driver commands test-sector-clip and test-sector-clip-runtime added. New CMake
library ssa_sector_clip makes106native libraries. Windows checker/script:
ssa_native_sector_clip_check / scripts/test-sector-clip.ps1.

ROM proof:4096D000+4011CE50+3941complete053D0=12048comparisons on16maps,
1840ordered unlink/relink pairs,2048live-height fixtures and48input guards.
240synthetic cases exceed1024original iterations; native rejects atomically;
they are guards, NOT terminating parity comparisons. Only BIOS division and
sector-list allocator calls are controlled. Native runtime executes no ROM code.
GCC ASan/UBSan/leaks and MSVC agree2212checks/272type64frames/16listcases,
including zero-tick displacement, actual scheduler/animation, retained old head,
replacement/relink/grid preservation and release. Source/binary reports:
sector-clip-rom-validation.json, sector-clip-runtime-validation.json,
windows-sector-clip-validation.json. Explicit source list in
test_weapon_runtime.py now includes sector_clip.c as a runtime dependency.

World suite still passes28720ROM comparisons and native jump/H/list checks;
projectile integration still37132assertions.106-library Windows build and normal
PC/reference launcher startup/restart/error-log checks pass. Running image43344
prevented the first link; preserved it at
build/native-windows/Release/ssa_native_game.previous-b400e2f4c05948f9bf00526ef7ff9c11.exe
and rebuilt without terminating it. That user process later closed normally.
No full campaign rerun or admission in this continuation; six-map evidence
retains its documented earlier weapon-policy settings. Relaunch play-native.cmd.

Actual decomp.wtf claim/compile used053D0. Candidate recovered/sector_relocate.c
reached85.3% in one manual attempt, NOT exact (scheduling/store merge/literal
placement); claim released with notes. Live status74/526exact,2152/87648catalogued
bytes(2.46%),523attempts. No export regeneration, no matching count increase.
Next: seventh-map normal-input completion/conversion acquisition; original
continue/pause/presentation flows and other original creation/display behavior.

Latest user fix: buttons no longer wait for weapon stowing. Native frontend sets
SsaPlayerGameplay.immediate_use=1. player_gameplay's hazard adapter chooses new
ssa_player_hazards_step_immediate_use, which bypasses ONLY weapon_action==DC00.
The real action is preserved, not forged; use-key/local-view gates, original
range/contact order, callbacks, switch texture/script/sound41/latch and floor
hazards remain. Original hazard entry/default gameplay flag0 retain ROM behavior.
finish_fire_before_switch still governs weapon changes and is independent of E.

tests/test_immediate_use.py:1765 ASan/UBSan/leak checks,96cases/all8realbuttons/
bothviews, ready/partial/stowed/firing, original-mode denial and no-key denial.
One E down/up pulse activates on the FIRST tick, before stowing completes;
texture and exactly one click/latch verified. Player relocation/god fixtures
are tests only, not campaign evidence. MSVC checker/script agree:
ssa_native_immediate_use_check, scripts/test-immediate-use.ps1. Reports:
immediate-use-validation.json and windows-immediate-use-validation.json.
Original test-player-hazards still24576ROMcomparisons/4601ordered effects/96guards;
Windows weapon-switch checker3295 and perspective/chase/4K/reference smoke pass.
106-library Windows rebuild succeeds. No full campaign or matching change for
this isolated requested PC interaction policy. Relaunch play-native.cmd.

Latest port continuation restores original credits179DC/page12. Options' Credits
entry now opens the actual ROM-backed scroll instead of an unavailable notice.
Read analysis/menu.md. Supported startup pages now0,1,4,9,10,11,12; scores,
password, multiplayer, pause/continue and other presentation flows remain gaps.
No new playable map admitted; six-map campaign evidence retains its earlier
weapon policies. Original feature parity is still incomplete.

SsaMenu appends credits_offset/end (ROM-relative), signed16.16 Y/speed and u16
done/skipped-line count. New ssa_menu_step_input separates pressed/held; old step
uses pressed as held for compatibility. Native frontend tracks Up/W and Down/S
individually and clears on focus loss. Up pauses, Down accelerates0.5->2px/tick;
Up takes priority. Confirm/back suppresses glyphs immediately, page4 NEXT tick;
automatic text end returns page4 same tick. Entry resets Y160 and pointer246.
Only one16px line advance per tick. Empty lines retain original16748 behavior.
Star prefix chooses selection0/highlight, otherwise1; FONT ALWAYS0. Do not
misinterpret the selection argument as font. Three languages share the current
ROM's credits string0x080CC43C. Drawing is idempotent; malformed32-byte lines
are rejected. No runtime ROM instructions execute.

test-menu-rom now3864frames/8179200independently composed pixels, including2670
credits frames/3complete scrolls/88newline advances each plus pause/both-held,
acceleration/release/manual exits/re-entry. Shared map/font proof still passes.
test-menu GCC ASan/UBSan/leaks and MSVC agree23161checks/6648frames/18newgames.
Windows test-native-menu covers all9language/difficulty combinations, both
renderers, music on/off, actual WindowMessages, alternate-key holds/releases and
focus loss; each288frozen menu ticks/72credits checks/140frozen map ticks,
2level starts and restart. Physical input/listening untested. Reports:
menu-rom-validation.json,menu-validation.json,windows-native-menu-validation.json.
106-library Windows build succeeds; latest play-native.cmd executable rebuilt.

Actual decomp.wtf claim/compile used179DC. recovered/menu_credits.c reaches96.8%
after2manual attempts, NOT exact (loads/registers/line literal/return block).
Claim released with notes. Live status74/526exact functions,2152/87648catalogued
bytes(2.46%),525compiler attempts. No export regeneration/matching increase;
imported denominator is incomplete. Original ROM SHA1 unchanged.
Final normal PC/reference startup, raw-input registration, first paint, R restart
and concurrent launcher/error-log regression passes on this rebuilt executable.

Latest continuation restores password16BE4/page8 from Single Player. Read
analysis/menu.md. Supported startup pages0,1,4,8,9,10,11,12. The frontend accepts
passwords for the existing six-map campaign only; later valid passwords show an
explicit unavailable-level notice and remain editable. No new map admitted.
Scores/multiplayer/pause/continue/boot/narrative/persistence remain unfinished.

SsaMenu appends level/loadout/level_limit(default12), password[9], cursor and
Up/Down u8 hold counters. Win32 sets level_limit=SSA_NATIVE_CAMPAIGN_LEVELS(6).
Initial editor A_______; right/left wraps8positions, right wins. Up cycles
A..Z0..9_, Down reverses and wins simultaneous repeats. Hold repeat requires
counter>30 and counter%4==0;255 wraps back32. Original table0844C1D4/index0..35
selects level=index%12,difficulty=index/12,loadout=index+1. Invalid emits17;
success13 then normal music/screen fade. Back16 returns single-player.
Table12 is seven-character MEDIUM_; original8char editor cannot produce its
NUL. Oracle supplies that memory state explicitly; do not claim UI coverage for
this original data quirk. Drawing measures spacing0 but draws spacing1. The
selected glyph is submitted first/palette1, full string second/palette2. Shared
ssa_map_view_text_width exposes existing metrics without changing map rendering.

ssa_native_game_init_loadout adds original1240C setting to existing startup;
old init delegates setting0 unchanged. Nonzero must correspond to level; reject
>36 before mutation. Native startup now imports actual ROM weapons/ammo table,
initial selection, difficulty and continues5/3/2. Frontend allocates a NEW stable
SsaNativeGame after menu fade, binds audio and captures initial carry before
map view. NEVER copy a live native game. R restores password entry inventory;
advancement retains earned inventory. Existing map music/black/reveal stay.

test-menu-rom:8697frames/15667200independently composed pixels;4830password
frames/all36table matches and fades, cursor/highlight/hold overflow, invalid/back,
three languages, plus unchanged2670credits frames. Shared map/font proof passes.
test-menu GCC ASan/UBSan/leaks and MSVC:30433checks/8988frames/18newgames plus
36password fades and later-map limit cases. Recovered1240C regression still
12979whole-ROM comparisons. New tests/test_password_start.py:959checks,18native
password starts on6maps/3difficulties,450runtime/render frames,18inventory entry
restarts; GCC ASan/UBSan/leaks and MSVC agree. New CMake checker
ssa_native_password_check; scripts/test-password-start.ps1 validates core and
6actual launcher paths on maps2/4/6, all3languages/difficulties, PC/reference,
music enabled/disabled, WindowMessages character entry, map/level music and R.
Both level reveals verify entry inventory. Physical input/listening untested.
Reports password-start-validation.json/windows-password-start-validation.json.

106-library Windows rebuild and latest play-native.cmd ready. Original nine-case
menu/credits suite and normal startup/rawinput/paint/R/concurrent-launch/error-log
regressions pass. No full campaign rerun; prior six-map completion evidence keeps
its documented older weapon policies. Original ROM SHA1 unchanged.
Actual decomp.wtf16BE4 candidate recovered/menu_password.c reached78.5% after
2manual compiles, NOT exact (441target vs413candidate instructions, caching,
branches, loads/registers/stack scheduling). Claim released with notes. Live
matching remains74/526exact,2152/87648catalogued bytes(2.46%),527attempts.
No export regeneration or matching increase; denominator remains incomplete.

Latest continuation (2026-10-17): SSA_NATIVE_CAMPAIGN_LEVELS=7. The seven-map
earned-carry campaign completes in81,985 ordinary-input frames under GCC
ASan/UBSan/leaks and MSVC, score1098000/health93. Seventh map is the second
resource family, with real weapon7 pickup/two shots; this run has no confirmed
conversion impact. Test-bot supply/spacing/corner recovery changed; gameplay
collision/combat was not weakened. Current original minigun cadence, chainsaw
cadence/reach192, firing-animation handoff and immediate E use remain. Both
Windows renderers pass all six transitions/12 restarts/13 map views, including
HUD/movement/weapon at reveal start. Map-music validation now derives the
expected song from the real level family (0x85 on map7), not hardcoded0x84.
Password startup covers21starts/seven maps/three difficulties. See current
campaign-validation.json and windows-native-campaign-validation.json. These
full-campaign reports precede the subsequent menu-only PC input changes below;
retain their recorded hashes rather than claiming a fresh whole-campaign run.

PC menu input: ssa_menu_pointer uses localized font metrics, animated label
positions and the displayed page; hover selects and click returns confirm bits.
Blank canvas/letterbox/old-page click-through is rejected. Shared ssa_ui_viewport
and ssa_ui_pointer also drive Win32 painting, so resized mouse coordinates match.
WM_MOUSEMOVE/LBUTTONDOWN route through these helpers. Password WM_CHAR supports
case-insensitive direct text; first printable/edit replaces A_______, max8 chars,
Backspace/Delete and Left/Right/Home/End editing. WASD/M become text on this page;
Enter/click submit, Space is ignored. Original arrow cycling/hold remains;
Up/Down after typing pads to8 and returns to original editor. Short MEDIUM_ can
now be typed normally. ssa_ui_text is a reusable bounded letter/digit/underscore/
space editor for future score names; NETRICSA/scores screens remain unimplemented.
SsaMenu appends password_typed; update ctypes layout when changing it.

Original command8 emissions and menu ROM comparisons are preserved. Win32
ssa_win32_audio_menu_command treats command8 as idempotent only while the menu
song0x6F is already active with command8 and no fade/pause. Press Start and Back
to Main retain sequence/track state. Music off/on still stops/restarts normally.
Menu native checker:31,293 checks/9,080 frames; all36 codes typed in lowercase,
editing/full-buffer/invalid chars, localized pointer selection and letterbox
edges. Original ROM oracle:8,697frames/15,667,200pixels remains passing.
Windows menu automation now hovers/clicks Main twice, rejects blank clicks and
compares complete music player/pending state under lock at repeated command8.
Password automation uses original arrows in PC-renderer cases and WM_CHAR text
with correction in reference-renderer cases. Reports distinguish simulated
Windows messages from physical input/listening. Live decomp.wtf status remains
74/526exact,2152/87648catalogued bytes(2.46%),527attempts; no new exact match.

Latest campaign continuation (2026-10-17): frontend admits8 maps (indices0..7),
SSA_NATIVE_CAMPAIGN_LEVELS=8. The fresh full earned-carry chain completes in
96,309 ordinary-input frames under GCC ASan/UBSan/leaks. Earlier seven outcomes
stay identical; map8 completes in14,324 frames with1,276,000score/86HP,15pickups,
102enemy spawns, both recessed switches and original VM exit. It adds1,957
airborne/7,200third-person/6,577pitched frames,160PC level/pitched draws plus
reference rendering. Two carried conversion-gun shots produce410converted
controller frames in this completed map. Actual mixer covers all11,459,200
stereo frames, nonzero output and zero decoder errors. Map6 mixer/boss evidence
is unchanged. Read campaign-validation.json/native-game.md for current hashes.

Only test-bot switch planning changed: switch_goal tries both normals at48
then80units. Original64-unit interaction sweep includes32-unit player radius,
so an80-unit approach can activate a low/narrow recess from the adjacent room.
Do not change collision, switch/interaction range, health or VM state to make
this route pass. The original48-unit candidate was blocked by the recess height;
opposite48alone was insufficient. Failed/aborted eighth-*-probe logs remain in
build/native-campaign; they are not completion evidence. Actual input remains
ordinary movement/E/fire/mouse/Space/H, with no grants or state shortcuts.

Password startup now covers24starts/eight maps/three difficulties:1,277checks,
600runtime/render frames,24entry restarts. Windows password selectors0/10/20
cover maps2/5/8 and languages0/1/2 on both renderers. The script adjusts the
middle selector to keep language1 represented as map count changes. Existing
typed/arrow entry, music continuity, mouse menus and startup regressions pass on
the rebuilt106-library Windows frontend. Full original parity, maps9..12,
interchapter narrative, original pause/continue/score/NETRICSA remain unfinished.
No new recovered/exact function this pass; live matching remains74/526exact,
2152/87648catalogued bytes(2.46%),527attempts; no export regeneration.

Final eight-map Windows campaign validation passes and matches every GCC
completion field. Both renderers pass28transition stages/14restarts/15map views,
1,050frozen map ticks,135map-music checks/15level-music checks,225black-hold ticks,
900reveal ticks and15weapon/HUD/movement/entry checks. Live paired black/mid/full
reveal images pass; audio errors0. Nine language/difficulty menu cases and six
password-launch paths also pass, retaining music continuity and mouse/text input.
Normal startup/raw-input/paint/R/concurrent launches/error logging passes on the
same executable. Current reports bind source/binary hashes; original ROM SHA1
remains4ff48d8c18b70a5184758f1917343ae85a70858e. See
windows-native-campaign-validation.json/windows-password-start-validation.json.

Latest music balance: src/audio.c MUSIC_BUS_GAIN=0.5 (-6.02dB), applied to every
music song>=0x6E before global fade/headroom summation. PCM_BUS_GAIN=2 and
EFFECTS_BUS_GAIN=1.5 still apply; menu cues1..6 and gameplay/player/world effects
keep their prior amplitude. No per-note volumes, instrument balance, timing,
music command/restart policy or ownership changed. Existing all-type gain and
headroom reference cases now assert half-amplitude music and unchanged effects.
test-audio GCC ASan/UBSan/leaks and MSVC pass653659checks/131audible songs/
35640000catalog frames,2310gain cases,7priority cases,0clipping/decoder errors.
Windows actual device/mute/map/level/restart checks pass with music on/off.
Reports audio-validation.json/windows-native-audio-validation.json and rebuilt
106-library executable are current. The eight-map campaign completion report
retains its earlier recorded hashes; no full campaign rerun for this mix-only
change. Original ROM is unchanged; no decomp.wtf matching change.

Level-start policy clarified explicitly by the user: use original starter
weapons/ammo ONLY when there is no previous level's inventory (password/direct
launch/future level selection). Normal advancement MUST retain carried weapons,
ammo, pending acquisitions, selected/previous weapon and all other carry fields.
Do not reset or supplement an existing carry with the destination's starter row.

ssa_native_game_init_campaign(level,difficulty,progress) now centralizes this:
NULL progress uses the matching ROM row on later maps; normal New Game on map0
keeps original setting0. Supplied valid progress initializes normally then applies
its COMPLETE inventory/history/score/H state. Reject invalid/dead progress before
allocation, rather than silently treating it as absent. The old base init remains
setting0 for recovery fixtures/all16-map loading. Explicit --level N uses
init_loadout(difficulty*12+N+1), including --level0 (password-style); passwords
continue to use their exact setting. R restores the captured entry and advancement
uses init_campaign with progress. No live SsaNativeGame is copied or moved.

Password/fresh/carry suite:1975checks,24password starts,24normal fresh starts,
24existing-progress starts,24password entry restarts and24fresh entry restarts,
672runtime/render frames across8maps/3difficulties under ASan/UBSan/leaks. Fresh
ammo/ownership/selection is checked against raw ROM words; a distinct outgoing
all-weapon/depleted-ammo inventory proves exact carry/history/pending preservation.
Windows password script adds six explicit --level launches on maps1/4/8 in both
renderers to its six password cases. Direct startup validates raw ROM starter
ammo/ownership/selection after R (level_starter_checks=1), and both reveal entry
snapshots. Existing --level now starts properly equipped rather than chainsaw-only.
The full native eight-map sanitizer run was repeated with the current50% music
mix and preserves all96,309frames/1,276,000score/86HP and every old completion
counter. See current campaign-validation.json/password-start-validation.json.

Final clarified loadout checks pass on the rebuilt executable: Windows has12
password/direct-launch cases, all six fresh cases with level_starter_checks=1
and2 reveal-entry checks. All eight normal-input MSVC completions match the
fresh sanitizer report. Both PC/reference pass28transition stages/14restarts/
15map views, with unchanged carried inventories at every boundary and entry R.
Current windows-password-start-validation.json and
windows-native-campaign-validation.json bind this source and binary. Earlier
reset-on-every-level implementation was superseded before delivery; never
reintroduce it. User's existing game process was not terminated; relaunch loads
the corrected executable.

Gameplay HUD bar colors now use native0750C/0787C palette bands through
ssa_display_compose_gameplay in both actual game renderers. OBJ139..143 starts
with0844A804 (boss), changes at130 to0844A80E iff armor!=0, then at142 to
0844A818/0844A822 for signed health<=100/>100. Canonical inventory health/stat_c4
are used. Raw static compositor/asset diagnostic behavior stays unchanged.
Read analysis/hud-colors.md. test-hud-colors passes32cases/64actual original
VCOUNT interrupts/1,228,800pixels against ROM-driven palette snapshots. Display
635RGB frames/418281sanitizer assertions and PC renderer11169482checks/7sizes/
24jump-H frames pass. All106 MSVC libraries/game rebuild; both180frame hidden
Windows smoke launches pass. Full campaign was not rerun for this palette change.

Startup menu grid: SsaMenu appends grid[64], decoded from original080AAEEC.
Draw repeats this transparent8bpp BG1 tile over scrolling BG0 before text/logo,
on every supported page except Language. Original cold menu setup1141 left BG1
disabled; subsequent1341 entries enabled it. User explicitly requests it on
startup too. tests/test_menu_rom.py now samples the real original BG1 VRAM/
tilemap in its independent pixel reference and intentionally enables it after
Language. The ctypes Menu layout includes the appended64bytes. No menu input,
scroll,fade or music timing changed. First-visit transparency and malformed-grid
checks bring sanitizer menu tests to69700checks/9080frames/18newgames; original
state/glyph comparisons8697frames and requested corrected composition15667200
pixels pass. See analysis/menu.md and current menu validation reports.
Rebuilt106MSVC libraries/game; all nine current Windows language/difficulty
menu-start/credits/options/map/level/R cases pass in both renderers. Current
windows-native-menu-validation.json binds the grid-enabled source and executable.

Latest user change supersedes startup Language: ssa_menu_init starts Title in
English. Its first tick emits command8; the gate remains counter>90. Options
has Music (selection0), Credits (1), Language (2). Language is an inline localized
"Language: English"/"Sprache: Deutsch"/"Langue: Français" label, using original
237..239 names/font bytes. Right/Confirm/click advances modulo3, Left decrements;
no page change or music reset. Shared option_text drives draw and hit testing.
Recovered page10 remains solely for diagnostic ROM fixtures, never startup or
Options navigation. Fresh launch English; no settings persistence is claimed.
Native menu ASan/UBSan/leaks70367checks/9242frames/18games. Recovery fixture8694
frames/15552000pixels: explicitly enters legacy Language, independently submits
the extra label through original16748, retains native PC-only cycle tests.
Windows automation now chooses language through Options for menu/password starts.
Normal startup count187 replaces195; full menu test remains288 (72credits checks).
Update startup/audio/campaign scripts consistently; do not restore old selection
screen to satisfy fixtures. The user's running executable was preserved by rename
inside Release before rebuild, without terminating their process.
Final rebuilt Windows checks pass all9 Options-language/new-game cases,6
password cases (all3languages, bothrenderers) and6 direct-level cases. Standard
startup PC/reference and2 concurrent launcher cases also pass with187 menu
ticks. Fresh windows-native-menu/password-start/startup reports bind this binary.
All106 native libraries build; full campaign completions were not rerun for UI.

Gunrilla self-hit fix: projectile_runtime's move adapter excludes ONLY a typeD6
registered source for an unowned type67 shot while current XY bounds overlap.
Original muzzle80 < shooter90+ball16, and firststep16 can hit the shooter.
ssa_world_try_move_ignoring adds a QUERY-LOCAL pointer exclusion without changing
world/entity struct layouts, flags or source list membership. Walls, other
entities, specials and floor/ceiling commits still run. No retained immunity:
once separated, normal movement resumes and a return shot can hit its shooter.
Original entity_contact and standalone projectile_motion remain unchanged.
Read analysis/projectile-runtime.md. Driver test-gunrilla passes418sanitizer/leak
assertions/39frames:8 reproduced self-blocked headings now escape,8return hits,
1wall/1player/1other-enemy impact. Uses realD6 activation, native constructors,
combat and map geometry; test setup picks a flat clear courtyard and positions
bodies explicitly, not a new spawn/AI path. Original world28720ROM comparisons
and native projectile37132assertions/3270frames across16maps remain passing.
MSVC focused checker matches all counters; rebuilt106libraries/game and both
180frame hidden renderer smoke cases pass. Reports gunrilla-validation.json and
windows-gunrilla-validation.json bind current sources. Full campaign not rerun;
user's existing game process was preserved during rebuild.

Prion PC launch fix: native_game enables new ballistic_player_prion (default0
in recovered runtimes). Player type69 commit clears4000 just like the rocket
option, preserving100/owner/source/speed and conversion behavior. Weapon CREATE
removes its old40-unit side offset, uses controller.source's actual full heading
(s->heading) rather than the original no-target probe's final heading-512, and
projects its original192-unit forward offset along pitch from live eye height.
Apply eye/forward projection for this option even before free_aim is latched by
the first vertical mouse delta. Retain selected aim slope/target behavior; do not
enable free aim globally or alter other weapons' original targeting to fix this.
test-weapon-fixes now passes127090 ASan/UBSan/leak checks:46sound-loop cases,
6rockets/36frames and6Prion/36frames across level/up/down aim in both views.
Prion cases check exact independent XYZ movement under yaw/pitch/walk/jump,
initial sight-line/eye alignment and level aim before any pitch event. Original
controller/create/motion routines remain unchanged; this is the requested PC
adapter correction, not new matching progress. See analysis/weapon-fixes.md.
Final MSVC weapon-fixes check agrees on all127090checks,46loop cases,6rockets/
36frames and6Prion/36frames. All106libraries/game rebuild; both180frame hidden
Windows smoke launches pass. Default projectile integration remains37132checks/
3270frames across16maps. Current weapon-fixes and windows-weapon-fixes reports
bind the corrected source; windows-prion-smoke-validation.json binds the game
binary. Full campaign completions were not rerun for this launch correction.

Latest campaign continuation supersedes the eight-map checkpoint above:
SSA_NATIVE_CAMPAIGN_LEVELS=9 admits indices0..8 through normal advancement,
passwords and direct --level starts. The ninth uses the existing native game
composition; no gameplay collision/hazard/damage/inventory rules changed.
Full uninterrupted GCC ASan/UBSan/leaks and MSVC runs agree on110468frames,
ending at1446000score/53HP. Map7 now17484frames/1100000score/100HP; map8
15023frames/1290000score/78HP, reflecting the Gunrilla/Prion fixes. Map9 is
13227frames/106spawns/14pickups/53HP,148PC level/pitched draws,2382airborne,
6627thirdperson and4857pitched frames. Its real mixer renders10581600stereo
frames without decoder errors. Actual weapon9 pickup/firing adds20shots and
448E3 actor frames;20Prion shots yield12991converted-enemy actor frames.
The input bot reverses wall-blocked distant strafes, uses earned special
weapons, ignores converted allies as targets and jumps more often on later
maps. It waits18frames outside an original closing-door radius hazard and
looks away until closure clears it; it never changes mover/hazard state.
The nine-map completion test requires real E3/conversion/door-wait evidence.
test_campaign's optional last-map checkpoint argument saves earned entry data;
--resume LEVEL FILE is an explicitly labeled DEVELOPMENT probe, never full
campaign proof. Default test_campaign.py always runs the uninterrupted chain.
Portable password-start validation passes2221checks/756frames:27password,
27fresh,27carry and each entry/starter restart on all9maps/3difficulties.
All106MSVC libraries/game rebuild. Windows password checks cover6menu-password
and6direct-level starts/restarts in both renderers, including map9. Live matching
remains74/526functions,2152/87648catalogued bytes(2.46%),527compiler attempts;
this continuation adds no exact C match. Remaining campaign, later D8 natural
progression, interchapter/menu presentation and complete parity stay unfinished.
Final scripts/test-native-campaign.ps1 -SkipBuild passes all9completions against
the fresh GCC report, all8Enter boundaries and16entry restarts in both renderers.
Each renderer checks17original map presentations/1190frozen map ticks,
255black-hold and1020reveal ticks, with17weapon/HUD/movement/entry checks and
correct map-to-gameplay music. Native menu startup remains187ticks. Current
campaign, Windows campaign/password/build reports bind the new source/binary;
ROM SHA1 remains4ff48d8c18b70a5184758f1917343ae85a70858e.

Latest requested PC menus/save feature: Single Player rows are New Game0,
Select Level1, Password2. Options rows Music0/Credits1/Language2/Clear Data3.
SSA_MENU_LEVEL_SELECT14 shows5scrolling rows of actual localized ROM names48..59
(campaign0..11 only, no multiplayer), padlock icons, Up/Down and Left/Right page
navigation plus mouse rows/arrows. All12names appear; levels>=current frontend
limit9 remain explicitly unavailable, even if unlocked. Level0 is always free;
levelN needs completed bitN-1, on any difficulty. Password/direct starts alone
award nothing; real completion does. Selection then difficulty uses original
loadout difficulty*12+level+1, including0; ordinary New Game retains setting0,
advancement retains earned carry. Do not grant equipment based on a save.

profile.c/profile.h implement a value-only16byte PC save format: SSA-SAVE magic,
version1, languagebyte,12-bit LE completion mask, CRC32. It contains no inventory
or in-level snapshot. profile_win32 serializes writers with a transient lock,
reloads the latest disk data and applies ONLY the current event, flushes a unique
temporary file, then replaces atomically with MoveFileExW. This preserves other
sessions' updates and avoids resurrecting cleared progress from stale memory.
Default save.dat is in the working directory; play-native.cmd normalizes that to
the workspace beside the launcher. --save-file PATH overrides it. Missing data
defaults English/onlylevel1. Corrupt/unreadable data reports an error without
overwriting on load; failed replacement preserves the previous file. Settings
and completion save immediately. Normal startup restores language; init alone
does not unlock anything. Automated smoke/startup modes NEVER touch the user's
save by default; enable persistence only with an explicit isolated test path.

Frontend game_tick wraps the actual native tick and saves the first real VM exit
with finished state and living player before advancement/close. Its once-per-entry
latch resets at begin_map. SSA_MENU_CLEAR_DATA15 defaults to No; No/Esc preserves
everything. Yes emits one-step clear_requested; the host writes defaults and the
menu immediately resets languageEnglish and completed mask0. Music is not saved
and is not reset. Current original-game feature gaps remain; this is a requested
PC extension, not recovery of an original GBA save function.

Read analysis/profile.md. Driver test-profile passes405450ASan/UBSan/leak checks,
1143menu frames,81select-level starts/3languages/3difficulties, all4096masks and
128single-bit corruptions. Menu regression70547checks/9296frames/18New Games;
ROM fixture8694frames/15552000pixels explicitly skips the inserted PC-only Single
Player row for original navigation comparisons and adds new labels through the
original font routine. Ctypes Menu appends completed_levels(uint16), level_scroll,
selecting_level,clear_requested(unsigned). Do not treat new pages as ROM matches.
scripts/test-profile.ps1 -SkipBuild passes37disk/host cases, a genuine4977-frame
normal-input level1 completion through the frontend save hook, and separate
Windows launches that restore German and select newly unlocked level2 with its
original starter weapons/restart in both renderers. Only build/profile test files
are changed. All9Windows language/difficulty/menu cases and6password/6direct-level
cases pass, with2221portable password checks on9maps. All107libraries/game rebuild.
Old running executable was preserved by rename, not terminated. Current profile,
menu, password and build validation reports bind sources/binaries. Full9-map
campaign completions were NOT rerun for this UI/save addition. No new byte match.

Typed-password padding correction: src/menu.c password_equal keeps exact strcmp
for the recovered arrow-only editor, but typed input trims trailing underscores
on both input and the ROM code before comparison. The display already fills
missing slots visually; prior strcmp incorrectly rejected bare words until an
arrow edit materialized padding in the buffer. Typed EASY/EASY_/EASY____ and
MEDIUM/MEDIUM_/MEDIUM__ now work directly without modifying cursor/text state.
Partial words, wrong suffix letters and internal underscores do not match.
New tests first reproduced the failure, then pass all36codes/every allowed tail
padding length (123valid patterns), Enter/click without arrow edits and5invalid
word cases. Full shared menu regression72577ASan/UBSan/leak checks/9547frames/
18NewGames. Original ROM comparison still8694frames/15552000pixels, preserving
the exact legacy editor's MEDIUM_ quirk.
Windows --password-test automation now types only the bare word with WM_CHAR,
including backspace correction, in BOTH renderers. Reference mode submits Enter;
PC mode submits a letterboxed mouse click after the password page draws. It
never cycles a character. scripts/test-password-start.ps1 now compares the full
MSVC menu regression to GCC as well as original loadout/start/restart checks;
password_edges can be as few as6 for short words. All107libraries/game rebuild.
Reports menu-validation.json, menu-rom-validation.json and current Windows
password/build evidence track this fix; no gameplay/save format change or new
exact C match. Full campaign was not rerun for this input-only correction.
Final Windows password validation passes all6bare-word typed submissions (Enter
and click across3languages/2renderers), their map/music/loadout/restart checks,
6direct-level start/restart cases, the72577-check MSVC menu regression, and
2221original loadout checks. windows-password-start-validation.json now includes
the menu checker result/hash and verifies both portable reports' source hashes.

Mouse menu cycling cue: platform/win32/game.c menu_pointer_input wraps the
portable hit-test, compares selection before/after, and immediately sends original
menu command12 (sound1) only on a changed highlight. Both WM_MOUSEMOVE and direct
WM_LBUTTONDOWN use it. Same-row motion/clicks, blank space and reentry into the
same row do not replay cycling; activation keeps its existing confirmation cue.
Do not append this event to the old command queue: menu_step clears that queue
before the host consumes it. Portable menu/keyboard behavior is unchanged.
Existing --menu-test now verifies exact mixer-start increments through real mouse
messages, no repeats for same-row/blank/reentry/click, and byte-identical music
player0 while holding the recursive audio mutex. All9language/difficulty cases
pass in both renderers/music on-off,2mouse-check sequences each. All107Windows
libraries/game rebuild; windows-native-menu/build reports bind current source
and executable. This feedback change does not alter gameplay or save data.

PC sky pitch tracking: src/pc_renderer.c sky_row now translates the fixed-size
upper-three-quarter artwork by focal*tan(render_pitch), then clamps its top and
bottom edges per column. Looking up moves the image down; looking down moves it
up. Level aim keeps the prior sampling, including rounded coverage/last visible
row at tiny sizes. Use presentation render_pitch, including H convergence, not
raw weapon pitch. The sky remains flat/screen-facing; yaw scrolling is unchanged.
pc_renderer_pitch.inc refreshes both separable lookup axes per frame without
reallocation or geometry rebuilding. This supersedes older fixed-framing notes.
Free-look stripe fixtures now cover pitch reversals/return to level at640x480
and127x73. They temporarily mark floors as sky to expose bottom fill through real
geometry/HUD, then restore all map colors. Original renderer/ROM are unchanged.
Final sky checks:3899155ASan/UBSan/leak assertions,30100independent depth samples,
2591986stripe pixels,435241top-fill and598613bottom-fill pixels. The free-look
HUD fixture now references gameplay scanline bar colors (its old static palette
expectation predated that restoration). Seven-resolution renderer regression
still passes11169482checks with unchanged257587difference count at960x640.
All107MSVC libraries/game rebuild; Windows first-person up, H down,4K and
reference-renderer smoke tests pass, with inspected snapshots. Reports:
free-look-validation.json,pc-renderer-validation.json,windows-free-look-validation.json
andwindows-native-build-validation.json. No campaign replay or new byte match.

Time Strike correction: user explicitly chose ORIGINAL damage for every enemy,
not guaranteed ordinary-enemy kills. Keep100 explosion damage/difficulty/falloff,
boss resistance and original owner0/no credit. Tough enemies can survive.
Original11424 subtracts2 vertical velocity ONCE per call, then moves by vz*ticks;
calling this at60Hz caused accelerated falling. New test-only pinned mGBA oracle
tests/probe_original_time_strike.c boots unchanged ROM, warms180refreshes, then
converts one already linked pickup in RAM into an E3 at original XY/floor+520.
Original controller/render/frame code executes; no ROM patches. Nine steady
intervals total60hardware refreshes (3six,6seven). This measures one scene,
not a universal GBA frame cap or emulation of scene-dependent CPU slowdown.
scripts/test-original-time-strike.ps1 rebuilds/runs/validates original gravity
and trace; analysis/original-time-strike-validation.json records hashes.
Native projectile_runtime time_strike_gba_clock opt-in admits3updates/20refreshes
using16777216Hz/280896cycles rational time from60Hz ticks. Per-slot tick remainder
passes6/7/7 to unchanged E3 controller;520unit drop expires at67PCticks. Phase
clears on allocate/release/reuse; catch-up can run multiple admitted updates.
The native frontend enables it; default recovery runtime remains canonical.
weapon_runtime.time_strike_requires_target is another native-enabled opt-in.
Aim accepts CD..D8 with canonical health>0 and no flags1/20; invalid aim clears
pending click. New controller_step_targeted gates weapon9 both before animation
start and at attack commit, cancelling without ammo/stats/flash/audio/allocation.
Existing original/free/clocked controller entry points retain recovered behavior.
Do not turn a failed click into a later shot at a newly acquired target.
Special suite passes22349ASan/UBSan/leak checks:16maps,12timing/damage scenarios
across3difficulties/ordinary+bosshealth,10invalid/lost-target cases in2aim modes,
reuse/batchedcatchup,4normal native input/H cases with actual target-death checks.
MSVC/GCC agree; original weapon11652 and extended16384ROM comparisons pass.
Windows automatic-weapon/input regression remains53141checks. All107libraries
and frontend rebuilt; reports special-weapons/windows-special-weapons and
windows-weapon-cadence/build refreshed. No campaign replay/new byte match.

Enemy crowd movement (requested PC behavior extension): enemy_motion.c adds
ssa_enemy_crowd_move and ssa_enemy_walk_world_sliding. A blocked enemy takes a
tangent step along the blocker's original square face; persistent per-actor
SsaEnemyCrowd side avoids steering back into the same crowd. Head-on pairs
choose opposite world sides; a wall-blocked side tries the opposite. Requests
substep at<=4units/axis; lateral length stays within requested step length.
Walking still increments its counter and runs vertical physics ONCE, including
blocked walks. All horizontal commits use world contacts/geometry/clearance and
normal spatial relinking. No bodies are pushed or collision flags cleared.
world.c new PC-only try_move_crowd permits existing enemy overlap to separate
only if both absolute axis separations do not shrink, at least one grows, and
no center is crossed. It does not permit new overlap or player overlap. Each
other body is checked independently. PC broadphase expands128units to cover
original radii (Gunrilla90); original query/try_move remain32andROM-identical.
Failed lateral probes restore original enemy blocker for attack logic. Static
walls/player blockages do not trigger crowd sidesteps. All callbacks/reentry
ownership contracts remain. Preference caches a stable pointer identity only,
never dereferences it; zero/reset crowd state per enemy lifetime.
CD runtime crowd_slide and generic CE..D8 crowd_slide default0 (canonical);
native frontend enables generic and game.enemy_crowd_slide, copied to automatic
CD bindings. Generic lazy binding clears crowd state on slot reuse. API guards
check side/axis/busy. No changes to navigation/turning/speed/hazards/damage.
Read analysis/enemy-crowd.md; tests/test_enemy_crowd.py and
scripts/test-enemy-crowd.ps1 reproduce20354ASan/UBSan/leak checks,9scenarios,
1503diagnostic crowd frames,158monotonic-overlap checks,6144walks across16maps/
all enemy type IDs and512native input ticks/274enemy moves with CD policy bound.
MSVC/GCC agree. Original enemy motion88226ROM comparisons and original world
predicate/query/move/synchronous pickup/spatial/player/jump/H regressions pass.
Windows special-weapons checker still22349checks. All107libraries/game rebuild.
First-level normal-input run now completes5098frames/24000score/8pickups/
19spawns/94HP/13pistol+21shotgun, with0live enemies/0open script groups. The old
28000score floor failed despite reaching the real exit because more kamikazes
detonate without player credit. Completion now requires earned score in2000
increments, resolved live enemies/groups, all19spawns and original VM exit,
instead of a historical player-kill minimum. No input route/gameplay mutation.
Windows game/profile scripts compare all result fields to source-hashed current
portable evidence, replacing their old4977frame assumption. Profile completion
hook/save/unlock/language/ClearData and selected-level starts pass both renderers.
Full9-map campaign remains historical and was not rerun for crowd movement.
Reports enemy-crowd/windows-enemy-crowd, native-playability/windows-native-game,
windows-profile and windows-native-build bind this change. ROM SHA1 unchanged;
no byte matching claim or new catalogue match.

Select Level now uses fixed chapter pages:1..6and7..12. Keep level_scroll as
chapter start0or6, derived from selection/6; no mixed scrolling window. Up/Down
crosses6/7 naturally, Left/Right and page arrows preserve row across chapters
and stay put at outer page edges. Chapter/Kapitel/Chapitre label atY26. Six
small-font rows startY40, stride16 (lastY120), statusY144; pointer/lock drawing
use the same layout. Existing unlocks,9map cap, loadouts and save format unchanged.
Long localized names plus header/status exceed128glyphs if submitted together.
map_view.c extracts ssa_map_view_overlay (glyphs only, no background/fade) from
the original compositor; compose keeps its exact output/layout. Selector paints
header/status once then overlays each row as a separate bounded original-font
batch, before the existing menu fade. Do not enlarge the original canvas ABI.
Profile suite now590097ASan/UBSan/leak checks/1221frames/81starts, including all
three languages, page bounds/mouse arrows/row preservation,6/7crossing, back
from difficulty to chapter2 and independent complete row-pixel checks. Both
chapter previews inspected (including German/French); all6names/status fit.
Menu regression72577checks/9547frames and original8694menu-frame/15552000pixel
comparison pass; the included original map-view ROM suite passes too. All107
MSVC libraries/game rebuild. Windows profile/menu/disk/completion/unlock/reset
and selected-level starts/restarts pass in both renderers; source-hashed reports
profile/menu/menu-rom/map-view-rom/windows-profile/windows-native-build updated.
No gameplay change, campaign replay or new byte match for this UI pagination.

Select Level label cleanup: remove numeric prefixes from displayed ROM level
names; mouse hitboxes now measure the plain centered name. Fixed six-level pages
are titled Egypt/Rome, localized Aegypten/Rom and Egypte/Rome. Chapter numbering
is removed. Existing independent row-pixel fixture uses the unprefixed names;
portable590097checks/1221frames/81starts pass and previews were inspected.
The running game locked the old executable; preserve it by verified-path rename,
never terminate the user's session. Rebuilt launcher executable is for next run.

Continuation (2026-10-17): native PC player collision now uses
ssa_world_try_move_player / ssa_world_horizontal_step_player through the new
ssa_player_motion_world_step_pc entry in player_runtime.c. The original32-unit
entity broadphase can miss Gunrilla's90-unit body and let Sam enter it, then
reject retreat. The PC path scans128 units for CD..D8 centers and permits an
existing player/enemy overlap to separate monotonically on both axes, with one
strictly increasing and no center crossing. Never push/ignore/make enemies
nonsolid. All geometry, floor/ceiling and relinking checks still run. Only enemy
bodies use additional cells: E1 pickups keep original cell-based range/scripts.
Original world/horizontal/player-motion APIs and public structure layouts stay
unchanged. World ROM regressions pass; original064B8 passes8672 comparisons.
Enemy-crowd suite now20456ASan/UBSan/leak checks/12scenarios,6144walks on16maps,
512full-native frames/274enemy moves, plus16isolated player-runtime jump/H
escape frames with controlled hazard/presentation callbacks. The latter uses an
external registered body only in isolated player_runtime_step; the full game
requires pool/reserved ownership and rejects unregistered combat projections.
MSVC/GCC agree; windows-enemy-crowd report updated. First normal-input level now
finishes5110frames/24000score/8pickups/19spawns/100HP/13pistol+21shotgun,
0live enemies/groups. Windows native game/startup/both-renderer smoke pass.
All107libraries and launcher rebuilt, ROM SHA1 unchanged.

Campaign test now supports --fresh LEVEL_INDEX (2..11), starting from the actual
Normal-difficulty ROM loadout via init_campaign, not a granted test arsenal.
Python --fresh writes campaign-standalone-validation.json; default still demands
the uninterrupted admitted campaign and writes campaign-validation.json only
on success. Windows standalone report compares every result/source hash.
Map9(index9,tenth level) completes11468frames/58000score/8pickups/50spawns/27HP,
20Prion shots/8500converted actor frames,20TimeStrike shots/1409falling frames,
128rendered views and9174400stereo audio frames/0errors. GCC sanitizers/MSVC agree.
This is standalone proof, not consecutive carry or campaign admission.
SSA_NATIVE_CAMPAIGN_LEVELS remains9. The updated Windows chain completes
indices0..7, including realD7 defeat/HUD/projectiles/music, but the bot dies on
map8. Its latest resumed map8 probe also dies; do not call the historical9-map
campaign report current. Next: finish current map8 input-driven combat/supply
route, then validate carry into map9 before enabling the tenth map.
The input planner now dodges from early maps, retreats toward health while
firing, routes around low walls, keeps navigation destinations, uses visible
threats for evasion and explores real unvisited sectors after stalled combat.
It avoids rockets without muzzle-clearance prediction, and only treats distant
converted actors as safe outside700units. It changes no game health/ammo/world,
AI or script counters. Development checkpoint arguments additionally save each
earned entry as PATH.levelINDEX; --resume is still explicitly non-consecutive.
Matching is unchanged: live decomp.wtf74/526,2152/87648catalogued bytes,527attempts.

Requested weapon updates (2026-10-17): targeted Time Strike now starts its
original animation18 even without an eligible enemy. Set controlflag1000 to
consume that visual-only attempt; omit ammo/stats/palette/sound/E3 creation.
Losing a target at commit finishes the same silent gesture. time_strike_empty_pending
in weapon_runtime preserves empty quick clicks through raising, latching only
after all adjacent aim retries. Force those pending attempts to stay visual-only
if an enemy appears meanwhile; consume a weapon9 buffered click at animation
start. A second click during a gesture queues one more. Focus loss/disabled
controls, death/inactive players, pause/input-mode3, weapon changes and respawn
clear both pending fields. Finish-fire-before-switch still
waits for the empty animation. Default/original controller paths remain intact.

E3 smoothing is presentation-only: ssa_projectile_runtime_render_z blends the
current physicalZ toward the next6/7-tick clocked position, clamped using the
original integer landing test and floor. Never step gravity/damage from render.
NativeGame.source_z_offsets follows active source order. New actor collect/scene
offset entries use temporary body copies after finding original stable identity;
the PC renderer applies the same snapshots in both pitch paths. Original entry
wrappers passNULL offsets; ActorSource/projector/scene layouts stay unchanged.
Physical67-tick fixture lifetime,100damage and impact timing remain unchanged.

Prion fast_converted is enabled only on the native entity and actor-animation
runtimes. Living CD..D6 with flag400 gets two controller passes and two animation
steps per hosttick, including movement/turning/retarget/attack/drain clocks.
Damage per hit and canonical drain period30 stay original; drain time doubles
with behavior. Other actors/projectiles/player/music clocks stay normal. Stop
the extra pass on first-pass death/release/error; health<=0 or flag20 corpses use
normal clock, as does corpse animation/expiry. Pause skips both passes. Default
composition policies remain0 for original behavioral recovery.

Special suite now32226ASan/UBSan/leak checks on16maps:16blast/16fall/full-pool,
12timing/damage scenarios,732changing visual fall positions,16invalid/lost/
reacquired/raising/double-click gesture cases with648visual frames,4native
input/view cases and10actual native Prion-hit cases covering every eligible
type/60observed game ticks. They check1x/2x actual walk counts, retarget/drain/
animation counters, unconverted speed, pause, damage death and normal corpse
expiry. This is controlled integration, not campaign progress. Windows/GCC
agree. Weapon switch suite3295checks/20cases/506waiting frames now explicitly
checks the empty Time Strike gesture. Original weapon ROM11652 and actor
projection/collection ROM18048comparisons pass. Default Prion686, actor
animation/corpse75088 and entity-runtime85982 regressions passed before the
final Time Strike buffering adjustment (their broad source hashes predate it).
All107Windows libraries and launcher rebuild. Read analysis/special-weapons.md
and projectile-conversion.md. No campaign admission or new byte-matching claim
at that checkpoint; the later continuation below supersedes that campaign limit.

Campaign continuation (2026-10-17): SSA_NATIVE_CAMPAIGN_LEVELS is now10. The
updated uninterrupted normal-input chain completes indices0..9 in117841frames,
1390000score/33HP. GCC ASan/UBSan/leaks=1 and MSVC agree on every recorded field.
Map8 completes11554frames/1322000score/12pickups/106spawns/87HP; map9 completes
9249frames/1390000score/5pickups/50spawns/33HP. Both use20Prion and20TimeStrike
shots, real falling E3s/audio, both renderers, jumping and H. This supersedes the
historical nine-map proof and failed updated map8 probes. Fresh/password/selected
starts use original loadouts; all30level/difficulty starts and restarts pass.
Profile/menu suite now590142checks/1239frames/90selected-level starts; its limit
follows the production constant. Final two campaign maps remain unavailable.

The short16-unit escape probe on maps>=8 is test-input planning only. It lets
the bot leave corners while firing, where its old128-unit probe stood still.
That exposed an actual production failure on map8 frame8423: a convertedD6's
navigation target was an expired/cleared stable actor slot. Enemy1097C creation
now accepts a known stable target slot, as the ROM does, while requiring a live
registered combat source and rejecting foreign target pointers. Keep allocation
BEFORE target-coordinate reads: the new shot may reuse that exact slot. Never
snapshot old target coordinates or substitute the player. Default constructors,
AI retarget period and damage remain unchanged. See analysis/enemy-control.md.
test_enemy_control_runtime.py passes202356ASan/UBSan assertions:16Gunrilla shots
through released targets reused for cannonballs,32additional ownership guards,
738callback reentry probes, plus existing all16maps/nine-type behavior. The
1097C ROM oracle now8194comparisons, including cleared and aliased target slots.
Live decomp.wtf remains74/526,2152/87648catalogued bytes,527attempts; catalogue
coverage is incomplete and this continuation adds no exact byte matches.
Next gameplay work: validate index10 with original standalone loadout and earned
carry before admitting it, then natural D8 boss-map progression on index11.
Original continue/death flow, NETRICSA/scores/interchapter/presentation parity
and documented audio limitations remain unfinished. The port is a ten-level
native prototype, not the completed original-feature-parity target.
All107Windows libraries/launcher rebuilt. windows-native-campaign-validation.json
passes ten-map MSVC/GCC agreement plus36frontend stages/18restarts/19map views/
1330frozen map ticks per renderer, including live reveal weapons/HUD/movement
and black/mid/full pixel checks. windows-password-start-validation.json passes
all30original loadout cases plus password/fresh starts and restart of index9 in
both renderers. windows-profile and windows-native-startup reports also pass.
The first Windows campaign audit raced the old nine-level portable report; a
second full run passed after the ten-level sanitizer reference completed. The
password audit caught an older menu-source hash; test_menu.py refreshed it and
the complete password/frontend audit then passed. No failed checks were hidden.

Continuation (2026-10-17): level11/index10 (Praetorian Fort) is NOT admitted.
The original standalone loadout and the genuine earned entry in
build/native-campaign/eleven-entry.bin were exercised through normal input.
Early probes fell into original floor35 pits; later probes survived but stuck
on routes. No campaign-health/ammo/AI/script/collision/hazard bypass was added.
The final earned grid16 probe dies at1961frames/1418000score/34spawns/0HP.
The paired fresh probe was explicitly stopped after7200frames (last log:188HP,
70000score); grid16 routing was slow and there is no completion evidence.
Read its .aborted.txt. Keep SSA_NATIVE_CAMPAIGN_LEVELS=10 until a full earned
chain validates the next map. Earlier failed and aborted logs remain under
build/native-campaign/eleventh-*. Current ten-map results stay117841frames,
1390000score/33HP; this continuation does not claim the whole port complete.

The native shipped change fixes a real TimeStrike rendering failure exposed by
the original-loadout index10 probe at2880frames: D3 switched to animation72
(two base frames/six images) while flag800 held00908; its displayed frame4
belonged to the old walk animation. Reference projection failed image lookup;
PC could show another mip. native_game_prepare_view repairs only its source
snapshot when flag800, animation_frame==0, display<FFFE but>=resource.frames,
and the current sequence's first frame is valid. Canonical actors, clocks,
timing/damage and default recovery projection/animation APIs remain unchanged.
test_special_weapons.py now32318ASan/UBSan/leak assertions and Windows agrees:
12new cases in both views cover invalid/valid frozen frames, non-frozen input,
nonzero sequence index and FFFE/FFFF markers. Actual reference projections and
PC flat/pitched draws pass; memcmp preserves the full actor. See special-weapons.md.

Development input planner changes apply only on levels>=10: grid16, one-unit
clearance, original floor35/255 avoidance, highest touched stair floor from a
private query, supported escape from an already overhanging pit, short wall/axis
probes, no combat jumps near pits, persistent visible targets and near-projectile
evasion using observed velocities. Earlier maps retain grid64/four-unit margin
and their prior decisions. These are planner choices, not game movement rules.
Exact-corner grid32/radius32 routes caused sub-unit positioning stalls; adding
margin/grid16 changes early combat and the earned probe still fails. Prefer
profiling/caching the read-only planner and improving ordinary movement/supply
choices next; don't relax acceptance guards or change gameplay to pass it.
Experimental rocket selection was rejected after legitimate self-explosion and
removed. Current bot still avoids rockets. Failed-render diagnostics now print
queue/projection details and pure-map forward diagnostics on failed probes.
Live decomp.wtf remains74/526,2152/87648catalogued bytes (2.46%),527attempts;
the catalogue is incomplete, with no new exact match claimed this continuation.
Final verification for this continuation: all107Windows libraries and launcher
rebuilt; special suite32318checks/12frozen-render cases agrees on GCC/MSVC.
The current complete ten-map ASan/UBSan/leaks chain and Windows chain agree on
all fields (117841frames/1390000score/33HP). Windows campaign transitions pass
all36stages/18entry restarts/19map views per renderer, including immediate HUD,
weapons and movement during reveals and black/mid/full pixel checks. Launcher
startup/raw-input registration/initial paint/restart/error-log checks pass too.
Current reports: campaign-validation.json, windows-native-campaign-validation.json,
special-weapons-validation.json, windows-special-weapons-validation.json,
windows-native-build-validation.json and windows-native-startup-validation.json.
Original ROM SHA1 remains4ff48d8c18b70a5184758f1917343ae85a70858e.
