#ifndef SSA_PROJECTILE_CREATE_H
#define SSA_PROJECTILE_CREATE_H
#include "ssa/projectile.h"

typedef struct SsaProjectileCreator { unsigned updating; } SsaProjectileCreator;
typedef struct SsaProjectileCreateRequest {
    uint32_t type;
    SsaEntity *source;
    int32_t forward, side, z_offset;
    uint32_t heading;
    int32_t slope;
} SsaProjectileCreateRequest;
typedef struct SsaProjectileCreateOps {
    /* Return 0/-1; NULL output on success means exhausted pool. Keep allocated
     * projection/entity storage stable through creation and subsequent use.
     * Normally allocation clears its payload, as original 05170 does. */
    int (*allocate)(void *, SsaProjectile **);
    /* Original A5D0 boundary. Called BEFORE source/player ownership assignment.
     * May insert only this projectile; return 0/-1. Do not reenter creation,
     * replace/free storage or mutate source. Earlier effects persist on error. */
    int (*insert)(void *, SsaProjectile *);
    void *context;
} SsaProjectileCreateOps;
/* Full 10830 and 109F0. Returns 1 created, 0 pool exhausted, -1 invalid/error.
 * Output exposes an allocated projectile even on later failure; caller retains
 * ownership and must perform cleanup explicitly. Fixed arithmetic wraps as ARM.
 * The aimed wrapper overwrites inherited Z velocity after insertion. */
int ssa_projectile_create(SsaProjectileCreator *, const SsaProjectileCreateRequest *,
                          const SsaMathTables *, const SsaProjectileCreateOps *, SsaProjectile **);
int ssa_projectile_create_aimed(SsaProjectileCreator *, const SsaProjectileCreateRequest *,
                                const SsaMathTables *, const SsaProjectileCreateOps *, SsaProjectile **);
/* Whole1097C: explicit source+20 target, NULL means horizontal. The original
 * writes through NULL on pool exhaustion; native returns 0 safely instead. */
int ssa_projectile_create_target(SsaProjectileCreator *,const SsaProjectileCreateRequest *,
    const SsaEntity *target,const SsaMathTables *,const SsaProjectileCreateOps *,SsaProjectile **);
#endif
