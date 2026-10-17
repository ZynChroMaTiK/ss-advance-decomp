#ifndef SSA_ENEMY_CD_H
#define SSA_ENEMY_CD_H
#include "ssa/enemy_navigation.h"
#include "ssa/enemy_lifecycle.h"
#include "ssa/enemy_motion.h"

/* Type CD's complete controller (13978), including its sound postlude. The
 * navigation/activation views refer to the same stable entity. Caller owns all
 * storage and animation metadata. Navigation.heading aliases the original +48
 * field; activation.saved_facing is not used for this non-boss controller. */
typedef struct SsaEnemyCd {
    SsaEnemyActivation *activation;
    SsaEnemyNavigation *navigation;
    uint16_t drain_period, drain_countdown;
    uint8_t animation_done, destroy_requested;
    unsigned updating;
} SsaEnemyCd;
typedef struct SsaEnemyCdGame {
    SsaEnemyActivationGame *activation;
    SsaEntity *player, *blocking_entity, *loop_source;
    /* 0456C uses player-record XY; proximity/navigation use its body. */
    int32_t player_x, player_y;
} SsaEnemyCdGame;
typedef enum SsaEnemyCdEvent {
    SSA_ENEMY_CD_ANIMATION,
    SSA_ENEMY_CD_SOUND,
    SSA_ENEMY_CD_STOP_SOUND,
    SSA_ENEMY_CD_CONTACT_DAMAGE,
    SSA_ENEMY_CD_GROUP_RELEASE,
    SSA_ENEMY_CD_DESTROY,
    SSA_ENEMY_CD_EXPLOSION
} SsaEnemyCdEvent;
typedef struct SsaEnemyCdOps {
    const SsaEnemyNavigationOps *navigation;
    /* >=0 success/<0 error; observe-only except external animation/effect data.
     * ANIMATION(id,period,0) observes animation_done already reset (native
     * setter effect); update remaining external animation metadata here.
     * SOUND/STOP(id,0,0), CONTACT(other,damage,0,0),
     * EXPLOSION(radius,damage,flags). GROUP_RELEASE and DESTROY use zero args.
     * DESTROY is a request: keep every view/body stable until step returns,
     * then unlink/release/clear. Do not mutate/reenter bound state in callbacks.
     * The original postlude observes cleared fields after destruction; this
     * adapter reproduces its sound result without prematurely clearing storage.
     */
    int (*event)(void *, const SsaEnemyCd *, SsaEnemyCdEvent, SsaEntity *other,
                 int32_t, int32_t, int32_t);
    void *context;
} SsaEnemyCdOps;
/* Returns 0 updated, 1 destruction requested, -1 error. ticks<=255; speed_factor
 * comes from navigation_speeds[0]. Native navigation, activation, face/distance,
 * drain/death/corpse paths run here. Navigation's sight/nearest/world operations
 * and presentation/combat effects remain explicit required boundaries.
 * Check destroy_requested even on error: a later sound-stop callback may fail.
 * Earlier effects remain on failure. A requested-destroy actor cannot step again.
 */
int ssa_enemy_cd_step(SsaEnemyCd *, SsaEnemyCdGame *, const SsaMathTables *,
                       unsigned ticks, int32_t speed_factor, const SsaEnemyCdOps *);
#endif
