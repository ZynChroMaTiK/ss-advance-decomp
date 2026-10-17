# Native actor animation

`actor_animation.c` recovers entity animation setter `08005394` and the complete
active-entity animation traversal `08000908`. `actor_animation_runtime.c` binds
the latter to the shared entity pool and stable script actors. This is native
animation sequence playback, including its completion/event bytes. It does not
decode or draw sprites, textures, or animation images.

The original frame order distinguishes two animation systems:

1. Entity controllers, sector movers, scripts, and multiplayer pickup respawn.
2. `08000908`: actor animation, over the entity pool.
3. `0800F85C`: sprite controllers, including weapon control.
4. `0800E460`: sprite animation, already recovered in `weapon_animation.c`.
5. Rendering.

Do not tick actor animations again in E460. Both animation traversals proceed
forward through their respective active-index arrays. The actor array is a
100-slot pool with 16-bit indices at `03004470` and a 16-bit live count at
`03004354`; its body storage pointer is `03004644` with 116-byte records.
E460 instead uses a separate 84-slot pool of 48-byte sprite records, 32-bit
active indices through `030055C0`, count at `030055C4`, and body pointer
`0300559C`. The complete non-weapon sprite controller/pool adapter is not added
by this change.

## Exact actor semantics

| Entity offset | Native projection | Meaning |
| --- | --- | --- |
| +58 halfword | animation | Sequence ID |
| +5A halfword | frame | Current sequence frame word |
| +5C byte | index | Sequence word index |
| +5D byte | elapsed | Elapsed byte counter |
| +5E byte | period | Frame period |
| +5F byte | done | Completion/event byte |

The setter accepts IDs through `8D`, or `FFFF`; other IDs leave everything
unchanged. It stores the low byte of the period and clears index, elapsed, and
done, preserving the current displayed frame word.

00908 skips the entire entity when body flag `0800` is set. Otherwise it loads
the sequence pointer from `08449D30 + animation * 12`, writes the current frame,
and advances only when the period is nonzero. It adds the game's byte timestep
to elapsed with byte wrap **before** comparing against the period. Each consumed
period reads the next sequence word:

- `FFFF`: index becomes zero, done becomes `FF`, and frame comes from the start.
- `FFFE`: done becomes `FE`, period becomes `FF`, and the previous index/frame
  remain. The loop exits immediately.
- `FFFA` through `FFFD`: done receives the low marker byte; index advances past
  the marker to the next frame.
- Other values: index advances normally, with byte wrap.

Done is not cleared by ordinary advancement. Unlike E460, actor loop/hold
markers do not normalize it to 1, frame words have no high-bit merge, markers
are skipped, and no sprite flag substitutes a fixed timestep of one. Game
pause flags are not consulted by 00908.

`ssa_actor_animations_load` copies 772 halfwords from `08449728..08449D30` and
142 sequence offsets. It does not retain the ROM pointer or execute ROM code.
The other two words of each 12-byte table record are not needed by 00908 and
are not a recovered rendering format. IDs 0..3 start at terminal words, so the
loader retains the contiguous original sequence data rather than inventing
nonempty-sequence rules. The original setter permits ID `FFFF`, but an unskipped
00908 would dereference outside the table; native code rejects it without
mutation. Invalid indices, truncated data, bad pointers, and ticks above 255
are similarly guarded.

## Runtime ownership

Zero-initialize `SsaActorAnimationRuntime`, then initialize with the stable
spawn adapter, loaded animation data, reserved body array in original slot
order, and one stable `SsaActorAnimation *` for each reserved body. These
pointers are copied, not owned. The caller forwards every reserved body's
animation-set event to its projection, including player motion and lifecycle
events. Free the animation adapter before spawn/world teardown.

Owned script actors are projected automatically each frame, including new and
reused slots. Existing metadata fields remain canonical; the appended
`animation_display_frame` stores +5A. The adapter writes all six fields back
after native advancement. CD and projectile adapters already load the done
byte from the actor on their next update. No completion callback or test-driven
done assignment is needed.

The runtime checks world/pool ownership, positions, duplicate bodies, and busy
spawn/world/level workspaces. No callbacks execute during advancement. External
owners must not mutate bound entities/state or call this stage during another
controller/player update. The outer frame adapter is responsible for scheduling
and its higher-level reentry guard. A failed animation advance leaves all
animation projections unchanged; animation playback does not change pool order,
entity movement, health, timers, groups, or ownership.

## Evidence

`tests/test_actor_animation_rom.py` executes whole original 05394 and 00908
routines in test-only Unicorn with no instruction hooks. It compares complete
116-byte entities, including unchanged bytes, and active-index storage.
`actor-animation-validation.json` records **218,240** comparisons:

- 131,072 setter comparisons covering all IDs and all periods.
- 36,352 actual-sequence/timestep combinations.
- 45,440 consecutive animation frames.
- 2,048 flag-0800 skips, including arbitrary invalid IDs.
- 3,072 explicit marker-data cases.
- 256 complete shuffled/sparse pool frames, containing 11,531 active updates.

There are also 13 native guards, including all-or-nothing pool failure.
The native library runs with UBSan.

`tests/test_actor_animation_runtime.py` compiles the real native adapters with
ASan and UBSan (`-no-pie`, leak scanning disabled). The real-map integration
passes **75,088 assertions** on all 16 maps: 11,584 animation stages, 32 natural
CD death-animation completions, 32 corpse releases, 16 reused slots, 2,048
player jump/H frames, 16 actual player animation setter events, and 560 guards.
The CD death sequence reaches its original `FE` completion marker at tick 66;
the next controller observes it and clears the hit flag. Corpse destruction
still follows its separate 360-tick timer, not an invented completion gate.
Death uses native combat and the controller uses native enemy queries.
Presentation callbacks observe events; no fixture writes animation completion.
Results and source hashes are in `native-actor-animation-validation.json`.

`recovered/actor_animation_set.c` matched 05394 exactly through the actual
decomp.wtf `compile_and_diff` engine on the first manual attempt: **60 bytes,
30 instructions**. Its claim was released automatically. 00908 is behaviorally
validated native C, not an exact matching claim. The latest export includes this
60-byte C setter. An independent rebuild with 68 matched C functions reproduces
the complete original ROM byte for byte; see `rebuild-validation.json`.
