# Native entity phase

`entity_runtime.c` composes the recovered `04E04` scheduler with existing native
player gameplay, CD enemies, projectiles and owned E1 pickup timers. This is an
entity phase, not the complete game loop. Run it once before mover/VM updates,
then run weapon controllers in their original later sprite stage. Weapons fired
there create projectiles whose first entity update is on the next frame.

Initialize after combat, projectile runtime, native enemy queries and player
gameplay. All references must describe the same stable world and entity pool.
The adapter fills all100 slot pointers, including inactive actor payloads, and
uses the existing pool's dynamic active prefix. Its explicit `active_tail`
argument represents the original physical halfword after the100th active index;
it is used by the scheduler's auxiliary output when the pool is full. See
`entity-dispatch.md` for the independently verified original ordering quirks.

Players run `ssa_level_runtime_player_step`, so movement, jump, camera, hazards,
death/respawn and line/sector interactions remain native. Each indexed player
receives its own input and presentation callbacks. Ticks must agree across
players and the enemy frame. `target_player` explicitly selects the original
enemy player target; it does not silently stand in for display-local or motion
indices. Other game counters remain explicit caller inputs.

Bind already initialized CD runtimes using `ssa_entity_runtime_bind_cd`. Their
walk-counter pointer temporarily refers to the scheduler's shared counter and
is restored when unbound, destroyed or freed. CD updates use native sight,
nearest-enemy selection, wall paths, world movement and combat. The native
query path count resets at frame entry. Type69 conversion therefore uses the
same canonical drain fields and alternate targeting described in
`projectile-conversion.md`; there is no frozen-entity gate.

Bound projectile slots run the native controller/motion/combat adapter,
including attached E2 effects. Owned E1 actors run their original timer wrapper.
Other handlers present in the original table fail explicitly until implemented
and bound. Types with no original table handler keep the original no-op. This
does not add health projections for initial reserved non-player actors, recover
all enemy AI, or implement actor animation playback.

The original dispatcher reads active count after every handler. A release swaps
the final index into the released position, but the iterator still advances:
the moved actor may be skipped until the next frame. New allocations can be
visited this frame. The bridge deliberately retains this behavior. It also
retains original auxiliary-list construction and current-source restoration to
slot0. Presentation failure retains earlier gameplay effects; dispatcher cleanup
terminates the partial list and releases the bridge's update guard.

The optional before-handler callback observes scheduling only. It must not
mutate/reenter the world, pool, bindings or controllers, or run another frame
stage. Existing native controllers take their own player/combat/spawn locks.
The entity bridge does not hold those locks across the entire phase, because
the controllers must enter their normal guarded APIs. Free the bridge before
freeing its players, enemies, queries, projectiles, combat or world. It owns no
entities and restores every remaining bound walk-counter pointer on teardown.

`test-entity-runtime` exercises the adapter using actual maps and existing
native controllers under ASan and UBSan. Final counts and source hashes are in
`analysis/native-entity-runtime-validation.json`. The independently controlled
ROM scheduler test is a separate proof, not a replacement for integration.

The integration passes 85,982 assertions across all 16 maps: 2,062 entity phases,
2,048 movement/jump/H frames, 1,011 projectile updates, 2,050 CD updates (747
moves) and 2,053 pickup timer updates. Actual projectile/corpse releases prove
two swapped-slot skips and next-frame execution. Tests also cover automatic CD
binding detachment, unsupported CE rejection, the original D9 no-op, two callback
failures and 28,766 reentry/guard checks. Scripted fixtures and controlled
presentation/corpse animation boundaries do not constitute a playable level.
