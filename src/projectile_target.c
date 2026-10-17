#include "ssa/projectile_create.h"
#include "ssa/enemy_motion.h"
#include "ssa/slide.h"
#include <limits.h>
static int32_t bits(uint32_t n) { return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n; }
int ssa_projectile_create_target(SsaProjectileCreator *c,const SsaProjectileCreateRequest *r,
    const SsaEntity *target,const SsaMathTables *math,const SsaProjectileCreateOps *ops,SsaProjectile **out) {
    int result=ssa_projectile_create(c,r,math,ops,out);int32_t distance,dz,slope;int64_t product,q;
    if(result!=1) return result;
    if(!target) { (*out)->entity->body.velocity_z=0;return 1; }
    if(ssa_enemy_distance(target,r->source,math,&distance)<0) return -1;
    dz=bits((uint32_t)target->body.z+(uint32_t)(target->body.height/2)-(uint32_t)(*out)->entity->body.z);
    slope=ssa_trace_divide(dz,bits((uint32_t)distance<<16));
    product=(int64_t)(*out)->speed*slope;q=product/65536;
    if(product<0 && product%65536) --q;
    (*out)->entity->body.velocity_z=bits((uint32_t)q);return 1;
}
