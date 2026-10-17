#ifndef SSA_ENEMY_LIFECYCLE_H
#define SSA_ENEMY_LIFECYCLE_H
#include "ssa/enemy_activation.h"

typedef enum SsaEnemyLifecycleEvent {
    SSA_ENEMY_LIFECYCLE_GROUP_RELEASE,
    SSA_ENEMY_LIFECYCLE_DESTROY
} SsaEnemyLifecycleEvent;
typedef struct SsaEnemyLifecycleOps {
    /* >=0 success, <0 failure. GROUP_RELEASE observes health=0/flag2 clear,
     * before the corpse timer is assigned. DESTROY requests ordered world
     * unlink and pool release; defer clearing/freeing this view until return.
     * Do not mutate/reenter the view or body from either callback. */
    int (*event)(void *, const SsaEnemyActivation *, SsaEnemyLifecycleEvent);
    void *context;
} SsaEnemyLifecycleOps;

/* 12D5C. 0 success/-1 invalid or callback failure. No state/animation changes;
 * the enemy controller supplies those. Every invocation releases the group;
 * controller must call once on death entry. Earlier effects survive failures. */
int ssa_enemy_death_begin(SsaEnemyActivation *, const SsaEnemyLifecycleOps *);
/* 12D9C. 1 destruction requested, 0 corpse retained, -1 invalid/error.
 * animation_done projects original byte +5F. ticks is GAME+7 (0..255). */
int ssa_enemy_corpse_step(SsaEnemyActivation *, unsigned animation_done,
                          const SsaEnemyLifecycleOps *, unsigned ticks);
/* 12E1C periodic health drain: period=entity+34, countdown=entity+36.
 * Zero period leaves the countdown unchanged. Signed halfword checks wrap.
 * 0 success/-1 invalid. This is not general combat damage 037F4. */
int ssa_enemy_drain_step(SsaEnemyActivation *, uint16_t period,
                         uint16_t *countdown, unsigned ticks);
#endif
