#ifndef SSA_PLAYER_GAMEPLAY_H
#define SSA_PLAYER_GAMEPLAY_H
#include "ssa/player_runtime.h"
#include "ssa/player_lifecycle.h"
#include "ssa/player_hazards.h"
#include "ssa/respawn_select.h"

typedef struct SsaPlayerGameplay {
    SsaPlayerRuntime *runtime;
    const SsaSpawnPlan *plan;
    SsaPlayerLifecycle lifecycle;
    SsaPlayerLifecycleGame life_game;
    SsaWeaponControl weapon;
    SsaRespawnWeaponView weapon_view;
    SsaPlayerHazardGame hazard_game;
    SsaMapCollision interaction;
    uint32_t weapon_action;
    unsigned updating;
    /* Optional live display-local BC flags, read at the hazard stage rather
     * than snapshotted before death/respawn. Borrowed, stable during update. */
    const uint16_t *display_flags;
    unsigned immediate_use; /* PC buttons activate without waiting for stowing. */
} SsaPlayerGameplay;

typedef struct SsaPlayerGameplayOps {
    /* Presentation/mode boundaries: NOTICE, MODE, EFFECT, WEAPON_ANIMATION.
     * Native code handles UNLINK/INSERT; animation/sound use callbacks below.
     * Callbacks may change scalar gameplay state, including life_game fields,
     * but cannot replace/free pointers, change map layout, or reenter. */
    int (*event)(void *, SsaPlayerGameplay *, enum SsaPlayerLifecycleEvent,
                 int32_t, int32_t, int32_t, int32_t);
    int (*line)(void *, SsaPlayerGameplay *, unsigned kind, uint16_t line);
    int (*sector)(void *, SsaPlayerRuntime *, int16_t);
    int (*animation)(void *, SsaPlayerRuntime *, unsigned, unsigned);
    int (*sound)(void *, SsaPlayerRuntime *, unsigned);
    void *context;
} SsaPlayerGameplayOps;

/* Zero-init g; runtime must already be bound and plan must use its map.
 * Allocates separate interaction scratch. No weapon inventory is invented:
 * callers supply current/requested weapon and weapon_action when known.
 * weapon.flags mirrors runtime movement.weapon_flags at each step's entry.
 * life_game continue/exit/message/input state persist across frames.
 * hazard_game counters/sector list are caller-owned inputs; for a remote
 * player its local_flags must describe the display-local player.
 * Plan, runtime, map and hazard-sector storage must remain valid. Returns0/-1. */
int ssa_player_gameplay_init(SsaPlayerGameplay *, SsaPlayerRuntime *, const SsaSpawnPlan *);
int ssa_player_gameplay_free(SsaPlayerGameplay *);
/* Full existing native runtime with actual death/respawn and interaction/hazard
 * stages. Multiplayer peer body positions must match their player positions.
 * Selector failure aborts (original would consume uninitialized stack data).
 * All callbacks required, return >=0 success/<0 failure. Earlier effects remain
 * on failure, possibly including an unlinked body. No reentry. Jump velocity
 * customization and H-view mode persist across respawn; held jump stays held.
 * Rendering, sound playback, scripts and main-loop state changes remain external. */
int ssa_player_gameplay_step(SsaPlayerGameplay *, const SsaPlayerRuntimeInput *,
                              const SsaPlayerGameplayOps *);
#endif
