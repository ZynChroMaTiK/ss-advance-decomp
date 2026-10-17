# Native combined frame integration evidence

`tests/test_game_runtime.c` exercises the native `game_runtime.c` composition of
the recovered frame, entity, level, pickup, actor-animation, sprite-dispatch,
and weapon adapters. It uses real decoded maps and ROM data tables. This is a
headless integration test: input polling/distribution, control/mode handling,
rendering, audio, and presentation effects are explicit harness boundaries.
Their callbacks record requests and validate state; they are not implementations
of a platform backend, renderer, or playable executable. No ROM instructions
execute in this integration test.

Run `test-game-runtime` through the workspace driver, or directly run
`tests/test_game_runtime.py` under the Linux environment. The Python runner
builds the native sources with GCC, `-Wall -Wextra -Werror -pedantic`, ASan and
UBSan, `-fno-sanitize-recover=all`, and `-no-pie`. Leak scanning is disabled for
the established WSL sanitizer environment. Its report and source hashes are
`analysis/native-game-runtime-validation.json`.

Initialize the combined adapter after player/weapon startup, combat/projectiles,
native entity queries, actor animation and pickup-respawn adapters. All borrowed
storage must remain stable. Supply all 84 distinct sprite-slot pointers, including
empty slots; each bound weapon object appears once. The native dispatch follows
the supplied original active order and fails for unsupported controllers.

The adapter binds newly owned state-zero CD actors before their first entity
phase. It preserves externally supplied controllers, borrows canonical lifecycle
and CD global state for the duration of its binding, and restores previous shared
bindings on free. Input/control callbacks edit `life_game` for shared input,
message, continue and exit state. Free the combined adapter before any dependency;
freeing it unbinds its controllers without destroying their actors.

## Final measured coverage

The integration passes **997,410 assertions**:

| Scenario | Count |
| --- | ---: |
| Real maps | 16 |
| Single-player and additional frame calls | 8,220 |
| Genuine two-player frame calls | 132 |
| Main player movement/jump/H frames | 2,048 |
| Frames with actual player XY movement | 584 |
| Airborne main-player frames | 1,760 |
| Third-person main-player frames | 1,536 |
| Main scripted CD spawn/death/completion/release chains | 16 |
| Group-wait VM completions after those deaths | 16 |
| Native startup weapon-10 shots | 16 |
| First projectile update proven to be next frame | 16 |
| Projectile entity updates | 7,696 |
| Additional multiplayer startup shots | 4 |
| Paused frames | 2 |
| Display-local flag-8 render gate | 1 |
| Actual native sector-height change | 1 |
| Failure prefixes | 21 |
| Dependency/ownership guard checks | 51 |
| Callback reentry checks | 362,922 |

These are assertion/frame counts, not additional ROM differential comparisons
or exact matching progress. The underlying routines have their separate ROM
proofs documented in `frame.md`, `entity-dispatch.md`, `actor-animation.md`,
`sprite-dispatch.md`, and the subsystem analyses.

## Frame order and state transitions

Each complete unpaused frame verifies the reached order:

1. Poll, distribute input, control.
2. Native entity controllers.
3. Native sector movers, then script VM.
4. Native multiplayer pickup-respawn stage.
5. Native actor animation (`00908`).
6. Native sprite controllers (`F85C`), including bound weapons.
7. Native sprite animation (`E460`).
8. Sprite visibility/cache preparation and hardware OAM construction, when
   display-local flag 8 permits them. The old render names are compatibility
   aliases; this does not recover the world renderer.

The fixture uses actual `ssa_weapon_runtimes_start` loadouts. God-mode loadout
selection supplies weapon 10; it is fired through the normal controller and
sprite-animation stages. A projectile created during that sprite stage has
timer zero and has not appeared in the earlier entity traversal. The next
frame visits it through the native projectile handler. Actor animation is not
double-ticked in the sprite stage. The native sprite submitted queue is checked
after each append; the observer sees the low byte of `object.active_index`
already recorded by `101D0`.

On each of the 16 maps, the VM creates a CD after that frame's entity phase.
The actor retains timer 60 through that frame, binds automatically at the next
entity phase, and then decrements to 59. State-zero initialization, later AI,
native world queries, damage/death, group notification, animation playback,
corpse expiry, pool release, and waiting VM continuation all execute through
their native adapters. Native combat kills any surviving fixture CD after the
initial movement window. No test assignment sets its animation completion
byte. The next controller consumes the completion produced by `00908`.

The main fixture runs 512 frames per map. The first 128 include movement, jump
edges, relative yaw, and H press/release events; later frames allow native
projectile and corpse lifetimes to finish. Entity and GAME counters are seeded
differently so accidental use of the world traversal counter as the shared
GAME counter is detectable. Weapon, combat, and hazard projections observe the
same GAME counter and timestep.

The pause check verifies that entity state, actor animation, and elapsed
simulation time stay unchanged while the frame counter and sprite stage still
advance. Paused type-2 weapon animation remains unchanged; an independent
type-1 sprite's elapsed byte advances. The separate display-local flag-8 test
skips both render callbacks while preserving the sprite stages. A native mover
changes sector height before the same frame's script issues its closing command.

## Shared state across players and enemies

The multiplayer case uses two distinct registered bodies, player runtimes,
gameplay adapters, startup weapons, actor-animation projections, and persistent
kill counters. Both weapons share the actual combat/projectile pool and execute
in sprite order. Both players run native entity updates, movement, and animation;
ordinary map interaction also triggers a real CD spawn. The additional cases
verify four shared-state requirements:

- A local player's death at flags `18`, death timer 300, and exit flag 1 sets
  global input mode 3. The later alive player's weapon is suppressed in the
  same frame, with no shot or firing-state change.
- A local native respawn clears flag 8 before the remote player's hazard
  phase. The remote hazard projection sees the newly cleared display-local
  flags rather than the earlier frame-entry value.
- Native VM opcode 9 sets the same input mode consumed later by F150, also
  suppressing an alive player's held fire in that frame.
- An externally bound CD's existing sound-loop source is inherited by the
  combined adapter. Its native stop event clears the canonical source and
  mirrors that value to the external game view before presentation. A newly
  scripted automatic CD then uses that same shared game. External controller
  game pointers and borrowed walk-count ownership remain valid through teardown.

Lifecycle presentation callbacks also compare their player projection with the
canonical shared input/message state. A deliberately failing local death SOUND
callback sees message timer 600 already applied. The error retains that value,
clears update guards, and prevents later stages/rendering.

## Failure scope and limitations

The 21 failures cover all 12 before-stage prefixes, all five remaining external
stage boundaries, an unsupported sprite controller, a script-particle failure,
an invalid actor-animation ID, and the lifecycle sound failure above. Earlier
native effects remain. In particular, a failed script-particle callback leaves
its allocated actor present; retrying the unchanged VM entry can allocate
another actor. Both actors subsequently bind to the shared CD game. Invalid
actor animation stops before sprite processing, while retaining the preceding
entity timer decrement. A failed frame never writes the successful final
presentation latch.

The dependency checks include 18 busy runtime/workspace locks, duplicate sprite
body bindings, a missing bound weapon object, and a missing required external
stage callback. Nested combined-frame updates/free calls are attempted from
every observed stage and effect. Unsupported sprite type 3 fails explicitly;
the harness does not supply a successful placeholder controller.

Pickup-respawn is invoked at its actual place in every unpaused frame here,
but these particular fixtures do not exercise a level-12..15 multiplayer
pickup allocation. That behavior has separate native integration coverage in
`pickup-respawn.md`. The two-player fixture does not implement network
transport. Sprite records are stable caller-owned test objects; this does not
recover the original sprite allocator or every non-weapon sprite controller.
Remaining enemy types, graphics/texture decoding, rendering, sound playback,
platform input, and a runnable PC game remain separate work.
