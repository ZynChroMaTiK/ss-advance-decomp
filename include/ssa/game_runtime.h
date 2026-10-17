#ifndef SSA_GAME_RUNTIME_H
#define SSA_GAME_RUNTIME_H
#include "ssa/frame.h"
#include "ssa/entity_runtime.h"
#include "ssa/actor_animation_runtime.h"
#include "ssa/pickup_respawn_runtime.h"
#include "ssa/weapon_runtime.h"
#include "ssa/sprite_dispatch.h"

typedef struct SsaGameRuntimeInput {
    SsaEntityRuntimeFrame entities;
    uint16_t held[4], pressed[4];
    int32_t script_fallback_z, pickup_fallback_z;
} SsaGameRuntimeInput;

typedef struct SsaGameRuntime {
    SsaFrame *frame;
    SsaEntityRuntime *entities;
    SsaActorAnimationRuntime *animations;
    SsaPickupRespawnRuntime *pickups;
    SsaSpriteDispatch *sprites;
    const SsaWeaponAnimations *sprite_animations;
    SsaWeaponRuntime *weapons[4];
    SsaWeaponGame *weapon_game;
    /* Stable, self-referencing CD controllers for newly scripted actors. */
    SsaEnemyCdRuntime owned_cd[100];
    SsaEnemyCdGame cd_game;
    SsaPlayerLifecycleGame life_game;
    SsaEnemyCdGame *previous_cd_game;
    SsaPlayerLifecycleGame *previous_life_game;
    uint32_t walk_count;
    SsaGameRuntimeInput input;
    SsaFrameStage stage;
    unsigned updating, external_active;
    uint64_t step_serial; /* Native bookkeeping, distinct from original GAME counters. */
    unsigned enemy_crowd_slide; /* PC policy for automatically bound CD actors. */
} SsaGameRuntime;

typedef struct SsaGameRuntimeOps {
    const SsaEntityRuntimeOps *entities;
    const SsaScriptSpawnOps *scripts;
    const SsaSectorMoverOps *movers;
    const SsaPickupRespawnRuntimeOps *pickups;
    const SsaWeaponRuntimeOps *weapons[4];
    /* Optional observer/implementation for remaining sprite controllers.
     * Bound type2 weapons run natively; any other original handler requires
     * sprites->handler or fails. submit observes the original native queue. */
    const SsaSpriteDispatchOps *sprites;
    /* Mandatory for reached POLL, DISTRIBUTE, CONTROL and sprite preparation/OAM.
     * Input stages may update frame
     * projections and r->input, CONTROL may change scalar game/mode state.
     * Other callbacks must not mutate/reenter unrelated simulation stages or
     * replace/free runtime dependencies. Native rendering/platform/mode logic
     * is still required here; reporting success must mean that stage ran.
     * DBDC is sprite visibility/cache preparation, not the world renderer. */
    int (*external)(void *,SsaGameRuntime *,SsaFrameStage);
    /* Optional observer before each reached stage; cannot mutate or reenter. */
    int (*before)(void *,const SsaGameRuntime *,SsaFrameStage);
    void *context;
} SsaGameRuntimeOps;

/* Compose the recovered0244C orchestration with existing native subsystems.
 * Borrow all stable storage; zero-init, free before dependencies. frame flags
 * pointers are bound to indexed gameplay players. Weapon objects remain in
 * their caller's sprite order; no sprite pool or game initialization is invented.
 * Existing external CD bindings are retained. New owned state-zero CD actors
 * bind automatically before their first entity phase. Unsupported enemies fail.
 * life_game and cd_game are the canonical shared lifecycle/input and CD globals
 * while bound; prior entity-stage bindings restore on free. Use these scalars
 * for input/control changes rather than editing stale per-player copies.
 * VM opcode9 shares life_game.input_state with player death and weapon gating.
 * Returns0/-1. Free unbinds only this adapter's CD controllers, never entities. */
int ssa_game_runtime_init(SsaGameRuntime *,SsaFrame *,SsaEntityRuntime *,
    SsaActorAnimationRuntime *,SsaPickupRespawnRuntime *,
    SsaSpriteDispatch *,const SsaWeaponAnimations *,
    SsaWeaponRuntime *const *weapons,SsaWeaponGame *);
int ssa_game_runtime_free(SsaGameRuntime *);
/* Updates through all reached stages. GAME.counter/ticks/flags are shared with
 * weapon/combat/hazard projections; motion/protected-player and level counters
 * remain explicit inputs. Pause gates entities through actor animation once;
 * sprite stages still run. Prefix effects persist on error and guards clear.
 * A failed frame must not render a stale camera pose. No concurrent stage APIs.
 * The copied input may be changed only by the three external input/control stages. */
int ssa_game_runtime_step(SsaGameRuntime *,const SsaGameRuntimeInput *,
    const SsaGameRuntimeOps *);
#endif
