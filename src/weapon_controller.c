#include "ssa/weapon_controller.h"
#include <limits.h>
#include <string.h>

static int32_t i32(uint32_t n) { return n <= INT32_MAX ? (int32_t)n : -1-(int32_t)~n; }
static int16_t i16(uint16_t n) { return (int16_t)(n <= INT16_MAX ? (int)n : (int)n-65536); }
static int32_t sar(int32_t n,unsigned s) { return i32(((uint32_t)n>>s) | (n<0 ? ~(UINT32_MAX>>s):0)); }
static int valid(const SsaWeaponController *s) {
    return s && !s->updating && s->inventory && s->control && s->object
        && s->source && s->control->current>=1 && s->control->current<=10;
}
int ssa_weapon_rules_load(SsaWeaponRules *out,const uint8_t *rom,size_t n) {
    SsaWeaponRules r; unsigned i;
    if (!out || !rom || n<0x44bd04u) return -1;
    for(i=0;i<10;i++) {
        const uint8_t *p=rom+0x44bc8c+i*12;
        r.continuous[i]=p[0]; r.period[i]=p[4];
        r.range[i]=(uint16_t)(p[8]|((unsigned)p[9]<<8));
    }
    *out=r;return 1;
}
int ssa_weapon_controller_cooldowns(SsaWeaponController *s,uint8_t ticks) {
    unsigned i;
    if(!valid(s)) return -1;
    for(i=0;i<10;i++) {
        s->cooldown[i]=(uint16_t)(s->cooldown[i]-ticks);
        if(s->cooldown[i]&0x8000u) s->cooldown[i]=0;
    }
    return 1;
}
int ssa_weapon_controller_switch(SsaWeaponController *s,uint8_t ticks) {
    SsaWeaponControl *c;int32_t y;
    if(!valid(s) || s->control->requested<1 || s->control->requested>10) return -1;
    c=s->control;
    if(c->keys&0x10) c->flags|=0x100;
    if(c->flags&0x100) {
        y=i32((uint32_t)s->object->y+((uint32_t)ticks<<11));
        s->object->y=y;
        if(y>0xdbff) {
            s->object->y=0xdc00;c->flags&=~0x100u;c->current=c->requested;
            ssa_weapon_animation_set(s->object,(uint16_t)(c->current-1),3);
        }
    } else {
        y=i32((uint32_t)s->object->y-((uint32_t)ticks<<11));
        s->object->y=y;
        if(y<=0x5000) {s->object->y=0x5000;c->flags&=~0x2000u;}
    }
    return 1;
}
static int emit(const SsaWeaponControllerOps *o,const SsaWeaponController *s,
                enum SsaWeaponEventKind kind,int32_t a,int32_t b,int32_t c,
                int32_t d,int32_t e,int32_t f,int32_t g,int32_t h,
                SsaEntity *target,SsaWeaponEffect **effect,int32_t *result) {
    SsaWeaponEvent ev={kind,{a,b,c,d,e,f,g,h},target,effect?*effect:NULL,0};
    if(o->event(o->context,s,&ev)<0) return -1;
    if(effect) *effect=ev.effect;
    if(result) *result=ev.result;
    return 1;
}
#define EMIT(k,a,b,c,d,e,f,g,h,t,p,r) do { if(emit(o,s,k,a,b,c,d,e,f,g,h,t,p,r)<0) return -1; } while(0)
#define SOUND(k,n) EMIT(k,n,0,0,0,0,0,0,0,NULL,NULL,NULL)
static int particle(const SsaWeaponControllerOps *o,SsaWeaponController *s,
                    unsigned id,int32_t x,int32_t y,int32_t z) {
    EMIT(SSA_WEAPON_PARTICLE,(int32_t)id,x,y,z,0,0,0,0,NULL,NULL,NULL);return 1;
}
static int hit(const SsaWeaponControllerOps *o,SsaWeaponController *s,
               uint16_t heading,int32_t range,int32_t slope,int32_t damage) {
    EMIT(SSA_WEAPON_HITSCAN,0,0,0x600000,heading,range,slope,damage,0,NULL,NULL,NULL);return 1;
}
static int projectile(const SsaWeaponControllerOps *o,SsaWeaponController *s,
                      unsigned type,uint16_t heading,int32_t range,int32_t slope) {
    int32_t distance=0x500000,side=0,speed=0x500000;
    SsaWeaponEffect *p=NULL;
    if(type==0x69) {distance=0xc00000;side=0x280000;speed=0x400000;}
    else {
        if(s->aim.fraction<INT32_MAX) {
            int64_t v=(int64_t)range*s->aim.fraction;
            int32_t product=i32((uint32_t)((uint64_t)v>>16));
            if(product<=0x63ffff) distance=i32((uint32_t)product-(uint32_t)sar(product,2));
        }
        if(type==0x67) {side=0x80000;speed=0x400000;}
    }
    EMIT(SSA_WEAPON_PROJECTILE_CREATE,(int32_t)type,distance,side,speed,
         i16(heading),slope,0,0,NULL,&p,NULL);
    if(!p) return -1;
    if(type!=0x69 && distance!=0x500000) p->travel=0;
    p->owner=(uint8_t)(s->source->player_index+1);
    if(type!=0x69) p->weapon=(uint8_t)(type==0x65?6:10);
    EMIT(SSA_WEAPON_PROJECTILE_COMMIT,0,0,0,0,0,0,0,0,NULL,&p,NULL);
    return 1;
}
static int special(const SsaWeaponControllerOps *o,SsaWeaponController *s,SsaWeaponGame *g) {
    SsaEntity *t=s->aim.target;SsaWeaponEffect *p=NULL;unsigned direction,animation;
    if(!t) return 1;
    if((uint16_t)(t->type-0xcd)<=9 && !(t->body.flags&1)) t->body.flags|=0x800;
    g->request=0x40000009u;
    EMIT(SSA_WEAPON_REQUEST,i32(g->request),0,0,0,0,0,0,0,NULL,NULL,NULL);
    EMIT(SSA_WEAPON_EFFECT_ALLOCATE,0,0,0,0,0,0,0,0,NULL,&p,NULL);
    if(!p) return 1;
    p->type=0xe3;p->state=0;p->target=t;
    p->x=t->body.x;p->y=t->body.y;
    p->z=i32((uint32_t)t->body.z+(uint32_t)t->body.height+0x1900000u);
    p->vx=p->vy=p->vz=0;p->flags|=0x20;p->weapon=9;
    direction=((uint32_t)t->body.x>>18)&3;
    animation=direction==1?0x62:direction==3?0x61:0x60;
    p->radius=direction==1?0x400000:direction==3?0x300000:0x800000;
    EMIT(SSA_WEAPON_EFFECT_ANIMATE,(int32_t)animation,5,0,0,0,0,0,0,t,&p,NULL);
    EMIT(SSA_WEAPON_EFFECT_INSERT,0,0,0,0,0,0,0,0,t,&p,NULL);
    return 1;
}
static int run(SsaWeaponController *s,SsaWeaponGame *g,const SsaWeaponRules *r,
               const SsaWeaponControllerOps *o,int free_aim,int automatic_ticks,int targeted_strike) {
    SsaWeaponControl *c=s->control;unsigned w=c->current;
    int local=g->motion_index==s->source->player_index;
    int16_t ammo=s->inventory->ammo[w-1];
    uint16_t heading=s->heading;
    int32_t range=i32((uint32_t)r->range[w-1]<<16),slope;
    int32_t x=s->source->body.x,y=s->source->body.y,z=s->source->body.z;
    s->aim.marker=0x69696969;s->aim.fraction=INT32_MAX;
    if(o->aim(o->context,s,heading,range,&s->aim)<0) return -1;
    if(!s->aim.target && !free_aim) {
        heading=(uint16_t)(heading+512);range=sar(range,1);
        if(o->aim(o->context,s,heading,range,&s->aim)<0) return -1;
        if(!s->aim.target) {
            heading=(uint16_t)(heading-1024);
            if(o->aim(o->context,s,heading,range,&s->aim)<0) return -1;
        }
    }
    slope=s->aim.slope;
    if(s->aim.marker==0x69696969) c->flags&=~0x20000u;
    else c->flags|=0x20000u;
    s->aim.marker=0;s->aim_slope=slope;
    if(!(c->flags&0x400)) {
        if((g->flags&0x400) && (s->held&1) && local) return 1;
        if(local) g->flags&=(uint16_t)~0x400u;
        if(!(s->held&1) || (g->local_flags&8) || s->object->y!=0x5000
            || s->cooldown[w-1]) return 1;
        if(ammo==0) {
            if((s->pressed&1) && local) SOUND(SSA_WEAPON_SOUND_PLAY,0x28);
            return 1;
        }
        c->flags=(c->flags&~0x1000u)|0x400;
        /* An empty Time Strike still gestures, but consumes this attempt at
         * start so a target entering the ray later cannot turn it into a shot. */
        if(targeted_strike && w==9 && !s->aim.target)c->flags|=0x1000;
        ssa_weapon_animation_set(s->object,(uint16_t)(w==3 && ammo==1?0x16:w+9),r->period[w-1]);
        if(local) {
            static const uint8_t sounds[5]={0x12,0x14,0x15,0x16,0x19};
            if((w==2 || w==3) && particle(o,s,w==2?8:11,x,y,z)<0) return -1;
            if(w<=5) SOUND(SSA_WEAPON_SOUND_PLAY,sounds[w-1]);
        }
        return 1;
    }
    if(local) g->flags&=(uint16_t)~0x400u;
    /* Losing the target at commit also finishes the silent gesture. */
    if(targeted_strike && w==9 && !s->aim.target)c->flags|=0x1000;
    if(!r->continuous[w-1]) {
        if(!(c->flags&0x1000)) {
            switch(w) {
            case 2: if(local && hit(o,s,heading,range,slope,10)<0) return -1;break;
            case 3:
                if(local) {
                    if(hit(o,s,heading,range,slope,40)<0
                       || hit(o,s,(uint16_t)(heading-1024),range/4,slope,10)<0
                       || hit(o,s,(uint16_t)(heading+1024),range/4,slope,10)<0) return -1;
                }break;
            case 6:
                if(projectile(o,s,0x65,heading,range,slope)<0) return -1;
                if(local) {if(particle(o,s,12,x,y,z)<0) return -1;SOUND(SSA_WEAPON_SOUND_PLAY,0x17);}break;
            case 7:
                if(projectile(o,s,0x69,heading,range,slope)<0) return -1;
                if(local) SOUND(SSA_WEAPON_SOUND_PLAY,0x20);
                break;
            case 8:
                if(local) SOUND(SSA_WEAPON_SOUND_PLAY,0x1c);
                s->cooldown[7]=120;g->request=0x80000009u;
                EMIT(SSA_WEAPON_REQUEST,i32(g->request),0,0,0,0,0,0,0,NULL,NULL,NULL);
                EMIT(SSA_WEAPON_EXPLOSION,0,1000,0,0,0,0,0,0,NULL,NULL,NULL);break;
            case 9:
                if(special(o,s,g)<0) return -1;
                if(local) SOUND(SSA_WEAPON_SOUND_PLAY,0x1e);
                break;
            case 10:
                if(projectile(o,s,0x67,heading,range,slope)<0) return -1;
                if(local) SOUND(SSA_WEAPON_SOUND_PLAY,0x1b);
                break;
            default:break;
            }
            c->flags|=0x1000;
            if(w!=1) {
                s->inventory->ammo[w-1]=i16((uint16_t)(s->inventory->ammo[w-1]-1));
                if(w==3 && s->inventory->ammo[2]>0) --s->inventory->ammo[2];
            }
            ++g->shots[w-1];
        }
        if(s->object->done) ssa_weapon_animation_idle(s->object,c);
        return 1;
    }
    if(!(s->held&1) || ammo==0 || s->source_health==0) {
        int32_t result;
        ssa_weapon_animation_idle(s->object,c);
        if(local && (w==1 || w==5)) {
            EMIT(SSA_WEAPON_SOUND_PLAY,w==1?0x13:0x1a,0,0,0,0,0,0,0,NULL,NULL,&result);
            if(!result) SOUND(SSA_WEAPON_SOUND_STOP,w==1?0x12:0x19);
        } else if(local && w==4) SOUND(SSA_WEAPON_SOUND_STOP,0x16);
        return 1;
    }
    if((w==1 || w==4 || w==5) && automatic_ticks==0)return 1;
    if(w==1 || w==4 || w==5) {
        unsigned ticks=automatic_ticks>0?(unsigned)automatic_ticks:g->ticks;
        int32_t damage=g->players==1 ? (int32_t)ticks*(w==1?1:w==4?3:4) : w==1?2:w==4?3:4;
        if((g->players==1 || local) && hit(o,s,heading,range,slope,damage)<0) return -1;
        if(w==1) {
            if(s->object->animation==10 && s->object->done)
                ssa_weapon_animation_set(s->object,20,4);
        } else if(local && !(g->frame&3) && particle(o,s,w==4?9:10,x,y,z)<0) return -1;
    }
    if(w!=1) s->inventory->ammo[w-1]=i16((uint16_t)(s->inventory->ammo[w-1]-1));
    ++g->shots[w-1];
    return 1;
}
int ssa_weapon_controller_step(SsaWeaponController *s,SsaWeaponGame *g,
                               const SsaWeaponRules *r,const SsaWeaponControllerOps *o) {
    int result;
    if(!valid(s) || !g || !r || !o || !o->aim || !o->event) return -1;
    s->updating=1;result=run(s,g,r,o,0,-1,0);s->updating=0;return result;
}
int ssa_weapon_controller_step_free(SsaWeaponController *s,SsaWeaponGame *g,
                               const SsaWeaponRules *r,const SsaWeaponControllerOps *o) {
    int result;
    if(!valid(s) || !g || !r || !o || !o->aim || !o->event)return -1;
    s->updating=1;result=run(s,g,r,o,1,-1,0);s->updating=0;return result;
}
int ssa_weapon_controller_step_clocked(SsaWeaponController *s,SsaWeaponGame *g,
    const SsaWeaponRules *r,const SsaWeaponControllerOps *o,unsigned ticks,unsigned free_aim) {
    int result;
    if(!valid(s) || !g || !r || !o || !o->aim || !o->event || ticks>510 || free_aim>1)return -1;
    s->updating=1;result=run(s,g,r,o,(int)free_aim,(int)ticks,0);s->updating=0;return result;
}
int ssa_weapon_controller_step_targeted(SsaWeaponController *s,SsaWeaponGame *g,
    const SsaWeaponRules *r,const SsaWeaponControllerOps *o,unsigned free_aim) {
    int result;
    if(!valid(s) || !g || !r || !o || !o->aim || !o->event || free_aim>1)return -1;
    s->updating=1;result=run(s,g,r,o,(int)free_aim,-1,1);s->updating=0;return result;
}
