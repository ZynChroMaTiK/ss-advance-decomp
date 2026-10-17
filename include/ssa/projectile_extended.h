#ifndef SSA_PROJECTILE_EXTENDED_H
#define SSA_PROJECTILE_EXTENDED_H
#include "ssa/projectile_controller.h"
#include "ssa/projectile_create.h"
typedef struct SsaProjectileExtendedOps {
    SsaProjectileControllerOps controller;
    /* Whole10830 boundary; returns1/0/-1, as projectile_create. */
    int (*create)(void *,const SsaProjectileCreateRequest *,SsaProjectile **);
    /* Child postlude after immediate10F10 call. Synchronize and, when requested,
     * release child now. Called even on an earlier child failure. 0/-1. */
    int (*child_finish)(void *,const SsaProjectileController *);
    /* Whole053D0 portal relocation, required ONLY for64. Not ordinary collision. */
    int (*relocate)(void *,SsaProjectile *);
    void *context;
} SsaProjectileExtendedOps;
/* Whole10D7C/11090/11350/11424: types64/6A/68/E3 respectively.
 * 0 updated,1 deferred destruction,-1 error. Earlier effects persist. Child6A
 * is updated immediately; type68's15 children are initialized but not stepped.
 * Motion return0 is intentionally ignored by68. Timer adds twice in states1/2.
 * Relocation remains an explicit boundary; callers must not fake success.
 * No storage may be released except successful child_finish destruction. */
int ssa_projectile_extended_step(SsaProjectileController *,SsaProjectileControllerGame *,
    const SsaMathTables *,unsigned ticks,const SsaProjectileExtendedOps *);
#endif
