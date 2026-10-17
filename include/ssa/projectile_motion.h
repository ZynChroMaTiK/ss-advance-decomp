#ifndef SSA_PROJECTILE_MOTION_H
#define SSA_PROJECTILE_MOTION_H
#include "ssa/projectile.h"
typedef struct SsaProjectileMotionGame {
    const SsaEntity *players[4];
    uint16_t headings[4];
    uint32_t credit, hit_weapon;
    uint8_t ticks, player_count;
} SsaProjectileMotionGame;
typedef struct SsaProjectileMotionOps {
    int (*move)(void *, SsaProjectile *, int32_t, int32_t);
    int (*vertical)(void *, SsaProjectile *);
    /* 400C: return 0 with optional blocking body, -1 on error. */
    int (*blocker)(void *, SsaProjectile *, SsaEntity **);
    int (*health)(void *, const SsaEntity *, int16_t *);
    int (*damage)(void *, SsaProjectile *, SsaEntity *, uint32_t);
    /* Legacy callback name: implements conversion, not movement freezing.
     * Set target health-drain period parameter34=30 AFTER caller sets flag400
     * (alternate-enemy targeting). Preserve countdown parameter36. Then create
     * E2: source=target, XYZ=(target.x,y,z+height+16), zero velocity, flag20,
     * animation5B period5, insert. Allocation exhaustion is success. */
    int (*freeze)(void *, SsaProjectile *, SsaEntity *);
    int (*unlink)(void *, SsaProjectile *);
    int (*insert)(void *, SsaProjectile *);
    int (*bounce)(void *, SsaProjectile *);
    void *context;
} SsaProjectileMotionOps;
/* Whole 10A20. 1 continues, 0 impacts/expires, -1 invalid/callback error.
 * Effects persist on error. Callbacks may mutate only documented targets;
 * may not release this projectile or reenter this API. */
int ssa_projectile_motion(SsaProjectile *, SsaProjectileMotionGame *,
                          const SsaMathTables *, const SsaProjectileMotionOps *);
/* PC player shots continue until impact; age saturates to avoid timer wrap.
 * Enemy projectiles retain original expiry. */
int ssa_projectile_motion_unlimited(SsaProjectile *, SsaProjectileMotionGame *,
                          const SsaMathTables *, const SsaProjectileMotionOps *);
#endif
