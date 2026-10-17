#include "ssa/particle_create.h"
#include <limits.h>
#include <string.h>
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int32_t bits(uint32_t n) { return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n; }
int ssa_particle_rules_load(SsaParticleRules *r,const uint8_t *rom,size_t size) {
    SsaParticleRules next;unsigned i;
    if(!r || r->loaded || !rom || size<0x44c13cu) return -1;
    memset(&next,0,sizeof(next));
    for(i=0;i<16;++i) {
        next.type[i]=(uint16_t)read32(rom+0x44c07c+4*i);
        next.animation[i]=(uint16_t)read32(rom+0x44c0bc+4*i);
        next.period[i]=(uint16_t)read32(rom+0x44c0fc+4*i);
        if(next.type[i]!=0x96 || next.period[i]>255) return -1;
    }
    next.loaded=1;*r=next;return 1;
}
int ssa_particle_create(SsaProjectileCreator *c,const SsaParticleRules *rules,unsigned id,
    int32_t x,int32_t y,int32_t z,const SsaParticleCreateOps *ops,SsaProjectile **out) {
    static const uint8_t sound[8]={0x22,0x23,0x24,0x26,0x27,0x64,0x25,0x26};
    SsaProjectile *p=NULL;SsaEntity *entity;int result=-1;
    if(!c || c->updating || !rules || !rules->loaded || id>=16 || !out || !ops ||
       !ops->allocate || !ops->animation || !ops->insert || !ops->sound) return -1;
    *out=NULL;c->updating=1;
    if(ops->allocate(ops->context,&p)<0) { *out=p;goto done; }
    *out=p;
    if(!p) { result=0;goto done; }
    if(p->updating || !p->entity || p->entity->owner) goto done;
    p->updating=1;entity=p->entity;
    entity->body.x=x;entity->body.y=y;entity->body.z=z;
    entity->type=rules->type[id];p->state=0;
    p->animation=rules->animation[id];p->animation_period=(uint8_t)rules->period[id];
    p->animation_done=0;
    if(ops->animation(ops->context,p)<0 || p->entity!=entity) goto unlock;
    if(ops->insert(ops->context,p)<0 || p->entity!=entity) goto unlock;
    if(id<8 && (ops->sound(ops->context,p,sound[id])<0 || p->entity!=entity)) goto unlock;
    if(id>=8 && id<=12) entity->body.flags|=0x2000;
    entity->body.flags|=0x20;result=1;
unlock:
    p->updating=0;
done:
    c->updating=0;return result;
}
int ssa_particle_step(SsaProjectile *p) {
    if(!p || p->updating || !p->entity || p->entity->type!=0x96 ||
       (p->entity->owner && p->entity->owner->query_active)) return -1;
    return p->animation_done?1:0;
}
int ssa_weapon_impact(SsaWeaponImpactState *s,const SsaEntity *source,
    unsigned motion_index,uint16_t weapon,int target_hit,int32_t x,int32_t y,
    int32_t z,const SsaWeaponImpactOps *ops) {
    unsigned id;int result;
    if(!s || s->updating || !source || !ops || !ops->particle || motion_index>255 ||
       (source->owner && source->owner->query_active)) return -1;
    if(!(source->body.flags&SSA_BODY_PLAYER) || source->player_index!=motion_index) return 0;
    s->updating=1;
    id=target_hit?2:0;
    if(target_hit && weapon==1) {
        s->random_state=s->random_state*UINT32_C(0x41c64e6d)+UINT32_C(0x3039);
        z=bits((uint32_t)z+(((s->random_state>>16)&31u)<<16)-0x100000u);id=6;
    }
    result=ops->particle(ops->context,id,x,y,z)<0?-1:0;
    s->updating=0;return result;
}
