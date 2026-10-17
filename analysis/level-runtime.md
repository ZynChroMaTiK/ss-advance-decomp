# Native level interactions and script scheduling

`src/level_runtime.c` connects native sector movers, switches and the recovered
script interpreter to `player_gameplay`. Initialize it with a decoded map and
the user's ROM before constructing players or map entities, because mover
initialization changes sector floor heights. The level binding owns its switch
records and borrows the map and immutable ROM. Free it before releasing either.

`ssa_level_runtime_player_step` runs the existing native player frame, replacing
its line and sector callbacks with recovered behavior. Type-1 lines activate
the first eligible sector mover. Type-2 lines activate a switch once, change
its front-side middle texture, enqueue its script and request sound 41.
The player hazard stage receives the current closing-door sector list.
First sector entry marks that sector visited and enqueues its low-byte script
ID when sector flag `0x100` is set. These are actual native state changes;
sound and presentation requests still require platform implementations.

The adapter restores the caller's hazard-list pointer/count after each player
call. Other hazard counters remain caller inputs. All player presentation
callbacks are required. They must preserve state/map identities and cannot
reenter or free the binding. An error preserves earlier changes and returns -1.

After entity/player updates, call `ssa_level_runtime_step` **once per world
frame**, not once per player. Original `0800244C` calls entity dispatch
`08004E04`, then mover update `0801C13C`, then script update `0801C6DC`, then
pickup respawn and other systems. The adapter implements the mover/script
portion of that order. A script opening a door changes its control flags after
that frame's height update; movement starts on the next update. Player line
activation occurs earlier and can affect the current frame's height update.

The interpreter executes one instruction per active queue slot. Mover opcodes
4/5/6/7 route to native open/close/unlock/lock commands. This low-level API keeps
script allocation, animation, insertion and particles as explicit callbacks.
`script_spawn.c` now supplies native allocation, animation metadata and insertion;
see `script-spawn.md` for that separately tested adapter. Particle presentation
still belongs to the caller. In particular, insertion must
supply the entity's floor; the interpreter writes its final Z after insertion.
The caller explicitly supplies `fallback_z` for the original undefined/stale
register used by particles when entity allocation fails.

Script `game_state` and `exit_toggle` expose original level-exit requests.
The future outer game loop must consume them. This adapter does not load the
next level, schedule enemies, implement weapon firing, produce audio or render
the world. It also does not fabricate changes to entity height caches beyond
the existing recovered movement/world paths. Map geometry queries and chase
camera queries read the changed sector heights normally.

Native bounds checks reject invalid script IDs, queue overflow, missing switch
records and malformed map/ROM accesses. Such errors can retain a latched switch,
changed map height or other earlier effects. The original often performs an
out-of-bounds access in those cases; the native API does not emulate memory
corruption. Guarded reentry includes directly active mover/switch/VM operations.

The constituent suites are `test-sector-movers`, `test-switches` and
`test-level-script`. They compare recovered behavior with actual ROM routines,
using explicit test boundaries for external effects. The script suite exercises
all 116 real script entries with controlled entity/effect callbacks; that is
interpreter coverage, not evidence of complete playable campaign behavior.

## Integration verification

`decomp.ps1 test-level-runtime` passes **43,622 assertions over 4,128 native
player frames on all 16 maps**, with AddressSanitizer and UndefinedBehaviorSanitizer.
This includes 3,376 airborne frames and 2,592 third-person frames, 16 native line
activation and mover cases, 16 closing-door deaths, four real switch cases,
and twelve first-entry sector queues. Geometry queries observe the actual
mutated sector heights. Queue-full and callback failures verify partial-effect
semantics: a switch remains latched after failed enqueueing, while failed
first-sector enqueueing leaves that sector unvisited. Twenty-seven failure
cases and 126 callback reentry checks pass.

Tests use real maps and switch/trigger metadata. Additional synthetic script
records live only in a private ROM copy to isolate frame ordering and error
boundaries. Script-spawn and presentation callbacks are controlled dependencies.
The test runner uses `-no-pie` and the established WSL `detect_leaks=0` workaround;
this does not claim LeakSanitizer coverage. The final PowerShell driver and all
26 native CMake library targets pass. See `native-level-runtime-validation.json`
for the exact source hashes and evidence scope.
