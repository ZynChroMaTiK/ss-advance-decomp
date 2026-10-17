#include "ssa/projectile_effect.h"
#include <limits.h>
int ssa_projectile_effect_follow(SsaProjectile *p,const SsaProjectileMotionOps *ops) {
    uint32_t z;int result=-1;
    if(!p || p->updating || !p->entity || !p->source || p->entity==p->source ||
       p->entity->type!=0xe2 || !ops || !ops->unlink || !ops->insert ||
       (p->entity->owner && p->entity->owner->query_active)) return -1;
    p->updating=1;
    if(p->source->body.flags&0x20) {result=0;goto done;}
    if(ops->unlink(ops->context,p)<0) goto done;
    p->entity->body.x=p->source->body.x;p->entity->body.y=p->source->body.y;
    z=(uint32_t)p->source->body.z+(uint32_t)p->source->body.height+0x300000u;
    p->entity->body.z=z<=INT32_MAX?(int32_t)z:-1-(int32_t)~z;
    if(ops->insert(ops->context,p)<0) goto done;
    result=1;
done:
    p->updating=0;return result;
}
