#include "ssa/projectile_create.h"
#include <limits.h>

static int32_t bits(uint32_t n) { return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n; }
static int32_t fixed(int32_t a,int32_t b) {
    int64_t p=(int64_t)a*b,q=p/65536;
    if (p<0 && p%65536) --q;
    return bits((uint32_t)q);
}
static int create(SsaProjectileCreator *c,const SsaProjectileCreateRequest *r,
                  const SsaMathTables *t,const SsaProjectileCreateOps *ops,
                  SsaProjectile **out,int aimed) {
    SsaProjectile *p=NULL;
    SsaEntity *e;
    int32_t s,co;
    int result=-1;
    if (!c || c->updating || !r || !r->source || !t || !ops || !ops->allocate ||
        !ops->insert || !out || (r->source->owner && r->source->owner->query_active)) return -1;
    *out=NULL; c->updating=1;
    if (ops->allocate(ops->context,&p)<0) { *out=p; goto done; }
    *out=p;
    if (!p) { result=0; goto done; }
    if (!p->entity || p->updating || p->entity==r->source || p->entity->owner) goto done;
    p->updating=1; e=p->entity;
    ssa_sincos(t,(uint16_t)r->heading,&s,&co);
    e->body.x=bits((uint32_t)r->source->body.x+(uint32_t)fixed(s,r->forward)+(uint32_t)fixed(co,r->side));
    e->body.y=bits((uint32_t)r->source->body.y+(uint32_t)fixed(co,r->forward)-(uint32_t)fixed(s,r->side));
    e->body.z=bits((uint32_t)r->source->body.z+(uint32_t)r->z_offset);
    if (r->source->body.flags&SSA_BODY_PLAYER) {
        e->body.velocity_x=0; e->body.velocity_y=0; e->body.velocity_z=0;
    } else {
        e->body.velocity_x=r->source->body.velocity_x;
        e->body.velocity_y=r->source->body.velocity_y;
        e->body.velocity_z=r->source->body.velocity_z;
    }
    p->speed=r->type==0x6a?0xc0000:0x100000;
    e->radius=e->body.height=r->type==0x6a?0x200000:0x100000;
    e->body.velocity_x=bits((uint32_t)e->body.velocity_x+(uint32_t)fixed(s,p->speed));
    e->body.velocity_y=bits((uint32_t)e->body.velocity_y+(uint32_t)fixed(co,p->speed));
    e->type=(uint16_t)r->type; p->state=0; p->facing=(uint8_t)(r->heading>>8);
    e->body.flags=0x104; p->timer=0;
    if (ops->insert(ops->context,p)<0 || p->entity!=e) { p->updating=0; goto done; }
    if (r->source->body.flags&SSA_BODY_PLAYER) {
        p->owner=(uint8_t)(r->source->player_index+1);
        e->player_index=p->owner;
        e->body.flags|=0x4000;
    }
    p->source=r->source;
    if (aimed) e->body.velocity_z=fixed(p->speed,r->slope);
    p->updating=0; result=1;
done:
    c->updating=0;
    return result;
}
int ssa_projectile_create(SsaProjectileCreator *c,const SsaProjectileCreateRequest *r,
                          const SsaMathTables *t,const SsaProjectileCreateOps *o,SsaProjectile **p) {
    return create(c,r,t,o,p,0);
}
int ssa_projectile_create_aimed(SsaProjectileCreator *c,const SsaProjectileCreateRequest *r,
                                const SsaMathTables *t,const SsaProjectileCreateOps *o,SsaProjectile **p) {
    return create(c,r,t,o,p,1);
}
