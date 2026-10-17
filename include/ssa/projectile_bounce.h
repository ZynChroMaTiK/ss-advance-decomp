#ifndef SSA_PROJECTILE_BOUNCE_H
#define SSA_PROJECTILE_BOUNCE_H
#include "ssa/enemy_ray.h"
typedef struct SsaProjectileBounce {
    SsaEnemyRay ray;
    int32_t fraction, previous_fraction;
    unsigned line, previous_line;
    const SsaBody *body;
    unsigned updating;
} SsaProjectileBounce;
/* Original4718/468C. Three corner wall rays, nearest blocking line, original
 * quantized vector reflection. No position move. Use independent storage.
 * Init/free return1/-1; step returns1 bounced,0 no wall,-1 invalid. */
int ssa_projectile_bounce_init(SsaProjectileBounce *,const SsaMapAssets *);
int ssa_projectile_bounce_free(SsaProjectileBounce *);
int ssa_projectile_bounce(SsaProjectileBounce *,SsaBody *,int32_t radius,
    const SsaMathTables *,uint8_t ticks);
#endif
