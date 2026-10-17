#ifndef SSA_ACTOR_ANIMATION_RUNTIME_H
#define SSA_ACTOR_ANIMATION_RUNTIME_H
#include "ssa/actor_animation.h"
#include "ssa/script_spawn.h"
typedef struct SsaActorAnimationRuntime {
    SsaScriptSpawn *spawn;
    const SsaActorAnimations *animations;
    SsaEntity *reserved[SSA_ENTITY_POOL_CAPACITY];
    SsaActorAnimation *states[SSA_ENTITY_POOL_CAPACITY];
    unsigned reserved_count, updating;
    unsigned fast_converted; /* PC: animate living CD..D6 Prion victims at2x. */
} SsaActorAnimationRuntime;
/* Zero-init. Copies pointer arrays for all original reserved slots. Their
 * entities/states and loaded data stay stable until free, before spawn teardown.
 * Owned actors are projected automatically, including adopted prefix and
 * new/reused slots. Their external state pointer can be NULL. External owners
 * must forward every animation setter to their supplied state.
 * No external mutation/release during a call; no callbacks execute here.
 * This is 00908 only: after pickup respawn, before F85C and sprite E460.
 * Returns 0/-1; invalid inputs leave animation states unchanged. */
int ssa_actor_animation_runtime_init(SsaActorAnimationRuntime *,SsaScriptSpawn *,
    const SsaActorAnimations *,SsaEntity *const *reserved,
    SsaActorAnimation *const *states,size_t count);
int ssa_actor_animation_runtime_step(SsaActorAnimationRuntime *,unsigned ticks);
int ssa_actor_animation_runtime_free(SsaActorAnimationRuntime *);
#endif
