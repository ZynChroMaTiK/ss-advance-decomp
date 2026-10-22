#ifndef SSA_ENEMY_ACTIVATION_H
#define SSA_ENEMY_ACTIVATION_H
#include "ssa/world.h"

/* Host projection of original entity fields; not a binary GBA overlay.
 * state=+3E, facing=+3F, saved_facing=+48, timer=+4E, boss_ticks=+51. */
typedef struct SsaEnemyActivation {
    SsaEntity *entity;
    int16_t health;
    uint16_t timer, saved_facing;
    uint8_t state, facing, boss_ticks;
    unsigned updating;
} SsaEnemyActivation;

typedef struct SsaEnemyActivationGame {
    uint32_t spawned[12]; /* GAME+130, stride 12, first word only; CD..D8 */
    SsaEntity *boss;      /* original 03005710; caller owns stable storage */
    uint8_t boss_phase;   /* original 03005714 */
} SsaEnemyActivationGame;

typedef enum SsaEnemyActivationEvent {
    SSA_ENEMY_ACTIVATION_SOUND,
    SSA_ENEMY_ACTIVATION_ANIMATION,
    SSA_ENEMY_ACTIVATION_MUSIC,
    SSA_ENEMY_ACTIVATION_NOTICE
} SsaEnemyActivationEvent;

typedef struct SsaEnemyActivationOps {
    /* Return >=0 success, <0 failure. Update external animation/audio/UI state;
     * do not mutate/free/reenter the supplied view, entity or game. Animation
     * event arguments are id, period, 0, 0; notice is id, 0, 48, 0. */
    int (*event)(void *, const SsaEnemyActivation *,
                 const SsaEnemyActivationGame *, SsaEnemyActivationEvent,
                 int32_t, int32_t, int32_t, int32_t);
    void *context;
} SsaEnemyActivationOps;

/* Original state-zero initializers for CD..D8, including 125BC counters and
 * boss state-entry helpers. 1=activated, 0=countdown waiting, -1=invalid/error.
 * Requires state zero, ticks<=255 and no active world query. CE, D7 and D8
 * require event callback; others permit NULL ops. Countdown, prior fields and
 * effects remain changed on callback failure. Does not run AI, movement,
 * attacks, death or the outer entity dispatcher. */
/* Original species maximum; zero for non-enemies. */
unsigned ssa_enemy_initial_health(unsigned type);
int ssa_enemy_activation_step(SsaEnemyActivation *, SsaEnemyActivationGame *,
                              const SsaEnemyActivationOps *, unsigned ticks);
#endif
