#ifndef SSA_ENEMY_CD_RUNTIME_H
#define SSA_ENEMY_CD_RUNTIME_H
#include "ssa/enemy_cd.h"
#include "ssa/script_spawn.h"

typedef struct SsaEnemyCdRuntime {
    SsaScriptSpawn *spawn;
    const SsaMathTables *math;
    SsaEnemyCdGame *game;
    uint32_t *walk_count;
    SsaEnemyNavigation navigation;
    SsaEnemyCd controller;
    unsigned slot, active, updating;
    unsigned crowd_slide; /* Optional PC enemy/body sliding; default is original. */
    SsaEnemyCrowd crowd;
} SsaEnemyCdRuntime;
typedef struct SsaEnemyCdFrame {
    unsigned ticks;
    int32_t speed_factor; /* navigation_speeds[0] */
    uint32_t frame_counter;
    uint16_t level_counter;
} SsaEnemyCdFrame;
typedef struct SsaEnemyCdRuntimeOps {
    /* Only visibility/nearest/path are used. Native turn/walk/hazards replace
     * the other callbacks. Sight/nearest are explicit, not approximated by a
     * point movement query. Their context remains queries->context. */
    const SsaEnemyNavigationOps *queries;
    int (*event)(void *, const SsaEnemyCd *, SsaEnemyCdEvent, SsaEntity *,
                 int32_t, int32_t, int32_t);
    void *context;
} SsaEnemyCdRuntimeOps;
/* Bind once to a newly spawned state-zero CD actor. Zero-init runtime. Stable
 * storage required throughout that actor lifetime. No allocation is owned here.
 * game->activation must be &spawn->game; update game player body/record XY each
 * frame. walk_count is the shared original movement-call counter. Do not release
 * or reuse the slot externally while bound. Do not copy this self-referencing
 * runtime. The outer entity dispatcher/order remains caller responsibility.
 * Returns 0/-1. */
int ssa_enemy_cd_runtime_init(SsaEnemyCdRuntime *, SsaScriptSpawn *, unsigned slot,
                               const SsaMathTables *, SsaEnemyCdGame *, uint32_t *walk_count);
/* Call in the entity phase, before script_spawn_step. Runs native controller,
 * turn, world movement, gravity and actual live mover/floor hazards. Group
 * notification and destruction use the script-spawn ownership layer. Animation
 * metadata resets before presentation events. CONTACT/EXPLOSION callbacks still
 * require real combat implementations; audio/animation playback remain external.
 * GROUP_RELEASE/DESTROY are handled internally, not forwarded as events.
 * combat_runtime.h supplies a wrapper with actual native contact/radial damage.
 * Return 0 updated, 1 actor released (binding inactive), -1 error. Earlier
 * effects persist; destruction may complete even if its sound postlude fails.
 * All callbacks must keep bound storage stable and not reenter/mutate it except
 * documented navigation results and external effects. */
int ssa_enemy_cd_runtime_step(SsaEnemyCdRuntime *, const SsaEnemyCdFrame *,
                               const SsaEnemyCdRuntimeOps *);
#endif
