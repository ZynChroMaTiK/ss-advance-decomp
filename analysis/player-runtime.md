# Native player runtime composition

`src/player_runtime.c` connects the recovered player initializer, complete
player-frame wrapper, movement, dynamic world collision, wall sliding, vertical
physics, camera height, new jumping, and H-toggle chase camera. It executes native
C against the decoded maps and data tables. There is no ROM execution in this
adapter and no emulator dependency.

This is a player runtime component, not a playable executable or a complete game
loop. Death, hazards, sector scripts, animation playback and audio remain required
external callbacks. There is no renderer, OS input backend, enemy activation,
combat, network scheduling, menu loop, or real-time clock integration here.

## Ownership and setup

Load a map, spawn plan and math tables, initialize a world and slide workspace,
then call `ssa_players_spawn`. Bind a zero-initialized `SsaPlayerRuntime` to one
active spawned player using `ssa_player_runtime_init`. Its player/world/slide/
table/map references must remain valid and stable until it is freed. The runtime
allocates and owns a dedicated `SsaMapCollision` for camera obstruction queries;
it never borrows gameplay query scratch. Free runtimes before the map. Runtime
free does not unregister or free the player, world, or slide workspace.

Initialization imports `player.entity_health` into `player.inventory.health`.
This joins two previously separate host views of the original entity health
field; original player initialization deliberately leaves its inventory record
zero while setting entity health to 100. Once bound, **inventory.health is
authoritative** for pickup and gameplay hooks. The adapter mirrors it back to
entity_health after every stage and external callback, including callback
failure. Hooks must therefore change inventory.health, not the entity mirror.

Similarly, `motion.movement.flags` is authoritative for the shared original
player flags. It is synchronized to `vertical.flags`; actual motion updates
both. Camera height mutates eye offset/velocity, but does not change these flags.
The adapter refreshes the frame wrapper's cached position and eye Z from motion
and camera state. External hooks change the player/motion/camera fields, not those
frame caches. The wrapper's entity timer and animation-state byte are owned by
`runtime.frame` and may be changed there by the callbacks.

World coordinates remain governed by the world API. If a hook teleports or moves
a registered player, it must use the appropriate world operation and update
motion X/Y to the committed body coordinates. Direct coordinate replacement
would invalidate spatial links. Frame callbacks cannot replace/free pointers or
reenter the runtime.

## Input and frame ordering

`SsaPlayerRuntimeInput` supplies already translated logical keys, a relative yaw
delta in 65536 units per full turn, held jump action, controls-enabled state,
original tick count, camera counter, and player metadata. A future platform
backend must convert keyboard/mouse events and elapsed time into these values.
This API does not claim to bind OS keys or capture the mouse.

The runtime retains all 16 heading bits after motion so small mouse yaw deltas
survive the original entity's 8-bit facing copy. It enables jump-aware vertical
substeps with the existing edge/landing/head-contact rules. Disabled controls
suppress logical keys and mouse yaw, consume simulated jump edges, and allow
existing movement velocities to decay under the original movement rules.

Call `ssa_player_runtime_key` for H key events. Fresh presses toggle mode; repeats,
held presses and disabled input follow the established `ssa_view_key` contract.
`ssa_player_runtime_focus_lost` clears the H-held latch. Jump remains a held action
supplied each simulated frame. These calls do not create an OS event loop.

The native frame calls required death processing, actual motion/world/sliding/
jump, actual camera height, required hazards, required sector handling, and any
animation request in original order. Camera footsteps are forwarded through the
required sound callback. The wrapper copies the local view before hazards and
sector scripting. After a successful frame, the runtime computes its final
first/third-person pose using the separate camera workspace.

Two original player indices are deliberately independent:

| Input | Original source | Uses |
| --- | --- | --- |
| `input.motion.local_player` | game state `+68` | Protection timer, multiplayer push and remote horizontal-movement gate |
| `input.local_player` | global `0300464E` | Local camera/audio and local-view copying |

The corresponding wrapper fields are `game.protected_player` and
`game.local_player`. Each index must fit `motion.player_count`. Multiplayer
supplies a stable entity-pointer array and slot for the current player. Changing
the gameplay metadata inside a hook is supported within these bounds; the next
movement stage rechecks the count/index constraints.

## Errors and verification

Every external callback is required and returns nonnegative success or negative
failure. Missing callbacks or invalid entry metadata fail before the runtime
changes player state. Reentry is rejected. Callback or geometry failures retain
earlier effects, synchronize health/flags/cache mirrors, clear the busy marker,
and return `-1`. The final view pose is not refreshed after a failed frame;
callers must check the return before rendering it. These are explicit native
integration rules, not behavior matched to an original error API.

`tests/test_player_runtime.py` builds the native composition with strict compiler
warnings, AddressSanitizer and UndefinedBehaviorSanitizer. Leak scanning follows
the existing WSL workaround (`ASAN_OPTIONS=detect_leaks=0`); this is not a claim of
LeakSanitizer coverage. Three PIE sanitizer executions timed out locally despite
the same binary completing cleanly on direct rerun. The test runner now builds
its sanitizer executable with `-no-pie` and limits execution to 20 seconds;
the rebuilt expanded suite, a separate direct run, and the final PowerShell
driver invocation completed successfully.

The tests use all 16 actual maps and actual original player spawn placement.
They exercise thousands of consecutive frames with native movement, jump arcs,
held-jump landing, mouse-yaw precision, disabled controls, H press/repeat/focus
handling, camera obstruction, local-view copying and coherent spatial ownership.
Each map also has a real E1 health pickup removed synchronously from a native
world query during movement; its updated inventory health survives the camera
and external callbacks. This fixture uses a known pickup at the player position;
it does not activate every map object or enemy.

Another 256 frames deliberately disagree about the protected/movement index and
display index, checking the remote movement gate, protection expiry, camera
locality and local-view preservation. Callback failures at death, hazards, sector,
animation and audio boundaries verify partial-effect and stale-pose semantics.
See `native-player-runtime-validation.json` for the exact current assertion and
frame counts. These are native composition tests, not additional ROM comparisons;
the constituent original routines have their own differential reports.
