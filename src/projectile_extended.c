#include "ssa/projectile_extended.h"
#include <limits.h>
static int32_t bits(uint32_t n) { return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n; }
static int signed_timer(uint16_t n) { return n<=32767?(int)n:(int)n-65536; }
static int32_t fixed(int32_t a,int32_t b) {
    int64_t p=(int64_t)a*b,q=p/65536;if(p<0 && p%65536)--q;return bits((uint32_t)q);
}
static int emit(SsaProjectileController *c,const SsaProjectileExtendedOps *o,SsaProjectileEvent e,int32_t a,int32_t b,int32_t d) {
    return o->controller.event(o->controller.context,c,e,a,b,d)<0?-1:0;
}
static int animation(SsaProjectileController *c,const SsaProjectileExtendedOps *o,unsigned id,unsigned period) {
    if((uint16_t)id<=0x8d || (uint16_t)id==0xffff) {
        c->projectile->animation=(uint16_t)id;c->projectile->animation_period=(uint8_t)period;c->projectile->animation_done=0;
    }
    return emit(c,o,SSA_PROJECTILE_ANIMATION,(int32_t)id,(int32_t)period,0);
}
static int destroy(SsaProjectileController *c,const SsaProjectileExtendedOps *o) {
    if(emit(c,o,SSA_PROJECTILE_DESTROY,0,0,0)<0)return -1;
    c->destroy_requested=1;return 1;
}
static int explosion(SsaProjectileController *c,SsaProjectileControllerGame *g,const SsaProjectileExtendedOps *o,int32_t radius,int32_t damage) {
    int result;g->credit=c->projectile->owner;g->hit_weapon=c->projectile->weapon;
    result=emit(c,o,SSA_PROJECTILE_EXPLOSION,radius,damage,1);g->credit=g->hit_weapon=0;return result;
}
static int initialize68(SsaProjectileController *c,const SsaMathTables *math,const SsaProjectileExtendedOps *o) {
    int32_t s,co;SsaProjectile *p=c->projectile;
    if(animation(c,o,0x5d,5)<0)return -1;
    p->state=2;ssa_sincos(math,(uint16_t)((unsigned)p->facing<<8),&s,&co);
    p->entity->body.velocity_x=bits((uint32_t)p->entity->body.velocity_x+(uint32_t)fixed(s,p->speed));
    p->entity->body.velocity_y=bits((uint32_t)p->entity->body.velocity_y+(uint32_t)fixed(co,p->speed));
    p->timer=0;p->entity->body.flags|=0x200;return 0;
}
static int flight6a(SsaProjectileController *c,SsaProjectileControllerGame *g,const SsaMathTables *math,unsigned ticks,const SsaProjectileExtendedOps *o,unsigned depth) {
    SsaProjectile *p=c->projectile;int moved=o->controller.motion(o->controller.context,p);unsigned i;
    if(moved<0)return -1;
    if(!moved) {
        if(explosion(c,g,o,0x20000,5)<0 || emit(c,o,SSA_PROJECTILE_PARTICLE,p->animation==0x83?13:14,0,0)<0)return -1;
        return destroy(c,o);
    }
    if(signed_timer(p->timer)>(int)ticks && (p->timer&63u)<=ticks)for(i=0;i<2;++i) {
        int heading=signed_timer((uint16_t)((unsigned)p->facing<<8))+(i?-0x1800:0x1800),result,finish;
        SsaProjectileCreateRequest request={0x6a,p->entity,0,0,0,(uint32_t)heading,0};SsaProjectile *child=NULL;
        SsaProjectileController next;
        result=o->create(o->context,&request,&child);if(result<0)return -1;if(!result)continue;
        if(!child || child==p || !child->entity || child->updating || depth>=100)return -1;
        next=(SsaProjectileController){child,1,0};
        child->entity->body.velocity_x=bits((uint32_t)child->entity->body.velocity_x-(uint32_t)p->entity->body.velocity_x);
        child->entity->body.velocity_y=bits((uint32_t)child->entity->body.velocity_y-(uint32_t)p->entity->body.velocity_y);
        child->entity->body.velocity_z=p->entity->body.velocity_z;
        result=animation(&next,o,p->animation,5);
        if(!result) { child->state=2;child->entity->body.flags|=0x20;result=flight6a(&next,g,math,ticks,o,depth+1); }
        next.updating=0;finish=o->child_finish(o->context,&next);
        if(result<0 || finish<0)return -1;
    }
    return 0;
}
int ssa_projectile_extended_step(SsaProjectileController *c,SsaProjectileControllerGame *g,
    const SsaMathTables *math,unsigned ticks,const SsaProjectileExtendedOps *o) {
    SsaProjectile *p;unsigned type,i;int result=0;
    if(!c || c->updating || c->destroy_requested || !c->projectile || c->projectile->updating ||
       !c->projectile->entity || !g || !math || ticks>255 || !o || !o->controller.event)return -1;
    p=c->projectile;type=p->entity->type;
    if((p->entity->owner && p->entity->owner->query_active) ||
       (type!=0x64 && type!=0x68 && type!=0x6a && type!=0xe3) ||
       ((type==0x68 || type==0x6a) && (!o->controller.motion || !o->create || !o->child_finish)) ||
       (type==0x64 && !o->relocate))return -1;
    c->updating=1;
    if(type==0x64) {
        if(!p->state) {if(animation(c,o,0xb,5)<0){result=-1;goto done;}p->state=1;}
        p->entity->body.x=bits((uint32_t)p->entity->body.x+(uint32_t)p->entity->body.velocity_x*ticks);
        p->entity->body.y=bits((uint32_t)p->entity->body.y+(uint32_t)p->entity->body.velocity_y*ticks);
        p->entity->body.z=bits((uint32_t)p->entity->body.z+(uint32_t)p->entity->body.velocity_z*ticks);
        result=o->relocate(o->context,p)<0?-1:0;
    } else if(type==0xe3) {
        if(!p->state) {
            int32_t z;
            p->entity->body.velocity_z=bits((uint32_t)p->entity->body.velocity_z-0x20000u);
            z=p->entity->body.z=bits((uint32_t)p->entity->body.z+(uint32_t)p->entity->body.velocity_z*ticks);
            if(signed_timer((uint16_t)((uint32_t)z>>16))<=p->entity->body.floor) {
                p->entity->body.z=bits((uint32_t)(int32_t)p->entity->body.floor<<16);
                p->entity->body.velocity_z=0;p->state=1;
            }
        } else if(p->state==1) {
            if(emit(c,o,SSA_PROJECTILE_PARTICLE,7,0,0)<0 || explosion(c,g,o,0x8000,100)<0)result=-1;
            else result=destroy(c,o);
        }
    } else if(type==0x6a) {
        if(p->state!=2) {
            if(animation(c,o,0x83+2u*p->state,5)<0){result=-1;goto done;}
            p->state=2;p->timer=0;p->entity->body.flags|=0x20;
        }
        result=flight6a(c,g,math,ticks,o,0);
    } else {
        if(!p->state) {
            if(animation(c,o,0x5c,5)<0){result=-1;goto done;}
            p->state=1;p->entity->body.flags|=0x200;p->timer=90;
        }
        if(p->state>=1 && p->state<=3) {
            if(o->controller.motion(o->controller.context,p)<0){result=-1;goto done;}
            if(p->state==3) {if(p->animation_done)result=destroy(c,o);goto done;}
            p->timer=(uint16_t)(p->timer+ticks);
            if(signed_timer(p->timer)>480) {
                if(p->state==2) {p->state=3;result=animation(c,o,0x5e,10);}
                else {
                    uint16_t heading=(uint16_t)((unsigned)p->facing<<8);
                    for(i=0;i<15;++i) {
                        SsaProjectileCreateRequest request;SsaProjectile *child=NULL;SsaProjectileController next;int finish;
                        heading=(uint16_t)(heading+0x1000);
                        request=(SsaProjectileCreateRequest){0x68,p->entity,0,0,0,(uint32_t)signed_timer(heading),0};
                        result=o->create(o->context,&request,&child);if(result<0)goto done;if(!result)continue;
                        if(!child || child==p || !child->entity || child->updating){result=-1;goto done;}
                        next=(SsaProjectileController){child,1,0};result=initialize68(&next,math,o);next.updating=0;
                        finish=o->child_finish(o->context,&next);if(result<0 || finish<0){result=-1;goto done;}
                    }
                    result=initialize68(c,math,o);
                }
            }
        }
    }
done:c->updating=0;return result;
}
