# Native scripted entity construction

`src/script_spawn.c` connects the recovered level-script interpreter to the
native entity pool, spatial world and enemy activation routines. It is a host
integration layer, not a byte-matched GBA routine. It creates actual registered
native bodies from original script records; ROM instructions are not executed.

## Creation and frame order

Initialize the map and `SsaLevelRuntime` before inserting players and map
objects. Then initialize `SsaScriptSpawn` with that level and world. It reserves
one pool slot per existing world entity and owns stable storage for subsequent
scripted actors. The total pool budget is the original 100 slots. Existing map
objects and players remain externally owned; this adapter does not migrate
them into a complete shared dispatcher. Their count must stay fixed while the
adapter is bound. A smaller world capacity is also respected.

Run player/entity updates first. Call `ssa_script_spawn_step` once per world
frame; it invokes the existing level runtime, which advances moving sectors
before the VM. Script mover commands therefore still take effect after that
frame's height update. The VM's allocation, animation and insertion callbacks
now perform native work. Particle effects remain a required presentation
callback, and mover sound uses the existing optional sound boundary.

The VM writes type, position, variant/state, facing, trigger tag, countdown 60
and visibility flag 80. The original animation setter `08005394` accepts the
low 16-bit ID when it is at most 8D or FFFF; period truncates to one byte and
three animation counters/flags reset to zero. This layer stores that metadata;
it does not play animation frames or decode sprites.

Insertion resolves actual BSP/sector/cell membership and floor/ceiling. The VM
assigns floor Z **after** insertion returns. The adapter flushes that value into
the registered body before the particle callback, so both observers see the
same position. It also flushes the pending value on failure. Registered X/Y are
never changed by that flush; later movement must use the world API.

Pool or world-capacity exhaustion returns normal allocation failure to the VM.
The original undefined/stale particle Z remains the explicit `fallback_z`
argument, including the VM's last successful spawn Z within a frame. A bad
insertion or failed particle callback reports an error while retaining prior
effects. Allocated actors remain owned and can be inspected and released.

## Activation and lifetime

For a state-zero CD..D8 actor, call `ssa_script_spawn_activate` during the
entity-update phase. This uses the verified original initializer, including
the flag-80 timer transition: expiration of the initial 60 ticks clears the
flag and starts another 30 ticks; the next expiration activates the enemy.
Nonzero states are rejected rather than overwritten. The resulting health,
dimensions, flags, counters and boss fields are native. See
`enemy-activation.md` for all twelve original paths and effect ordering.

The entity body and activation view are canonical after insertion. The script
view retains spawn coordinates and tag; activation copies its changed timer,
flags, state and facing back for observation. Boss animation events first
update the actor's animation metadata, then reach the presentation callback.
Sound, music and notice still require the caller's effect implementation.

`ssa_script_spawn_defeat` decrements the original script group once, allowing
wait-for-group opcodes to advance. It is an explicit notification for a future
death handler; the adapter does not simulate combat. Tag zero and bit 100 retain
the original release exceptions. The VM's separate `alive_before` byte still
models group-zero spawn underflow safely.

`ssa_script_spawn_release` removes world links, releases the index and clears
the actor. It does **not** notify the group: original `080051C8` performs no
group notification. Active-index swapping never moves payload storage. Releasing
the current boss additionally clears the host boss pointer to avoid a dangling
reference. `free` releases all owned actors without defeat notifications and
leaves externally owned entities registered. Call it before freeing level,
world, map or ROM storage.

Callback errors are partial updates, not rollback. The bridge rejects reentry,
active world queries and busy level subsystems. All callback views are observe
only: callbacks must not mutate or free any bound state through lower APIs.
This bridge cannot release pickups from inside a collision query; the existing
special-contact adapter has a separate, narrower removal contract.

## Validation and remaining work

Run `.\decomp.ps1 test-script-spawn`. The sanitizer suite uses all 16 actual
maps and all 116 original script entries with native insertion. Test-driven
defeat/release unblocks group waits; this is not enemy combat. Separate custom
script records in a private ROM buffer exercise capacity, timing and error
paths. The original ROM file is unchanged. The suite also runs player movement,
jumping and H-camera updates while spawned bodies occupy the world.

The final suite passes 82,853 assertions across 3,139 script frames and 3,072
player frames, including 1,983 frames with actual horizontal motion while an
actor remains registered. All 16 maps have such movement. It includes 192
enemy initializations, 32 blocked world queries, 20 failure cases, 676 guards
and 1,666 callback reentry checks. AddressSanitizer and UndefinedBehaviorSanitizer
pass with `-no-pie` and the established WSL `detect_leaks=0` workaround;
LeakSanitizer coverage is not claimed.

Exact counts, source hashes and sanitizer settings are recorded in
`native-script-spawn-validation.json`. The pool separately passes 36,697
original-routine comparisons and enemy activation passes 14,576. Those tests
use Unicorn only as an isolated oracle; the native libraries contain no emulator.

This adapter does not itself implement subsequent enemy states or death. The
separate `enemy_cd_runtime.c` now provides type-CD navigation and death/lifetime
integration; see `enemy-cd.md` for its remaining query/combat boundaries. Pickup
respawn, the complete entity dispatcher, particles, animation playback,
renderer/audio, outer exit/menu handling, OS input and a game executable remain
unfinished.
