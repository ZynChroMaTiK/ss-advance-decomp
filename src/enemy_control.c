#include "ssa/enemy_control.h"
#include <string.h>

static int32_t bits(uint32_t x) { int32_t r; memcpy(&r,&x,4); return r; }
static int32_t sar(int32_t x,unsigned n) {
    return x>=0?x/(int32_t)(1u<<n):-1-(int32_t)((uint32_t)(-(int64_t)x-1)>>n);
}
static int valid(SsaEnemyControl *c,SsaEnemyControlGame *g,const SsaEnemyControlOps *o) {
    return c && c->activation && c->navigation && c->activation->entity &&
        c->navigation->entity==c->activation->entity && g && g->player && o && o->event;
}
static int emit(SsaEnemyControl *c,const SsaEnemyControlOps *o,
                 SsaEnemyControlEventKind kind,SsaEntity *other,int32_t a,int32_t b,
                 int32_t d,int32_t e,int32_t f,int32_t h) {
    SsaEnemyControlEvent event={kind,other,{a,b,d,e,f,h}};
    return o->event(o->context,c,&event)<0?-1:0;
}
int ssa_enemy_control_periods(SsaEnemyControlGame *g,const uint8_t *rom,size_t size) {
    unsigned i;
    if (!g || !rom || size<0x44bfd8+12*8) return -1;
    for(i=0;i<12;++i) g->attack_period[i]=(uint16_t)(rom[0x44bfd8+i*8]|(uint16_t)rom[0x44bfd9+i*8]<<8);
    return 0;
}
int ssa_enemy_control_contact(SsaEnemyControl *c,SsaEnemyControlGame *g,
                              unsigned mode,const SsaEnemyControlOps *o) {
    SsaEntity *e;
    (void)mode; /* Original r1 is overwritten before use. */
    if (!valid(c,g,o)) return -1;
    e=c->activation->entity;
    if(c->navigation->move_result || c->navigation->phase!=2 ||
       !g->blocking_entity || !(g->blocking_entity->body.flags&0x10)) return 0;
    return emit(c,o,SSA_ENEMY_CONTROL_DAMAGE,g->blocking_entity,
                e->type==0xd0 || e->type==0xd6?40:20,0,0,0,0,0)?-1:1;
}
int ssa_enemy_control_attack(SsaEnemyControl *c,SsaEnemyControlGame *g,
                             unsigned ticks,const SsaEnemyControlOps *o) {
    SsaEnemyActivation *a; SsaEntity *target=NULL; int32_t z;
    if (!valid(c,g,o) || ticks>255 || c->activation->entity->type<0xcd ||
        c->activation->entity->type>0xd8) return -1;
    a=c->activation; a->timer=(uint16_t)(a->timer-ticks);
    if (!(a->timer&0x8000)) return 0;
    a->timer=0;
    if (!(c->navigation->navigation_flags&1)) return 0;
    z=bits((uint32_t)a->entity->body.height-(uint32_t)sar(a->entity->body.height,2));
    if (!o->aim || o->aim(o->context,c,z,(uint16_t)(((unsigned)a->facing+128)<<8),
                         0xc000000,&target)<0) return -1;
    if ((!target || !(target->body.flags&0x10)) && !(a->entity->body.flags&0x400)) return 0;
    a->timer=g->attack_period[a->entity->type-0xcd]; return 1;
}
typedef struct Call { SsaEnemyControl *c; const SsaEnemyControlOps *o; } Call;
static int life(void *ctx,const SsaEnemyActivation *a,SsaEnemyLifecycleEvent ev) {
    Call *p=ctx; int r; (void)a;
    r=emit(p->c,p->o,ev==SSA_ENEMY_LIFECYCLE_DESTROY?SSA_ENEMY_CONTROL_DESTROY:
           SSA_ENEMY_CONTROL_GROUP_RELEASE,NULL,0,0,0,0,0,0);
    if (!r && ev==SSA_ENEMY_LIFECYCLE_DESTROY) p->c->destroy_requested=1;
    return r;
}
static int animation(SsaEnemyControl *c,const SsaEnemyControlOps *o,int id,int period) {
    c->animation_done=0;
    return emit(c,o,SSA_ENEMY_CONTROL_ANIMATION,NULL,id,period,0,0,0,0);
}
static int sound(SsaEnemyControl *c,const SsaEnemyControlOps *o,int id) {
    return id<0?0:emit(c,o,SSA_ENEMY_CONTROL_SOUND,NULL,id,0,0,0,0,0);
}
static void bob(SsaEnemyControl *c) {
    SsaBody *b=&c->activation->entity->body;
    uint32_t floor=(uint32_t)(uint16_t)b->floor<<16;
    int32_t h=bits((uint32_t)b->z-floor),delta;
    if(h<=0) h=0x10000;
    delta=sar(h>0xc0000?bits(0x180000u-(uint32_t)h):h,5);
    if(!c->bob_direction) {
        h=bits((uint32_t)h+(uint32_t)delta);
        if(h>0x16ffff) { c->bob_direction^=1; h=0x170000; }
    } else {
        h=bits((uint32_t)h-(uint32_t)delta);
        if(h<=0x10000) { c->bob_direction^=1; h=0x10000; }
    }
    b->z=bits(floor+(uint32_t)h);
}
int ssa_enemy_basic_step(SsaEnemyControl *c,SsaEnemyControlGame *g,
                          const SsaMathTables *math,unsigned ticks,int32_t speed,
                          const SsaEnemyControlOps *o) {
    static const int walk[]={7,6,4,9},hurt[]={0x14,0x12,0x1e,0x16};
    static const int death[]={0x13,0x11,5,0x15},pain[]={0x42,-1,0x50,0x58};
    static const int diesound[]={0x44,-1,0x50,0x5a};
    static const int attack[]={0x14,0,0x43,0x89},attperiod[]={10,0,10,4};
    static const int shotsound[]={0x43,-1,0x50,0x59};
    SsaEnemyActivation *a; SsaEntity *e,pose={0}; SsaEnemyMotion motion;
    Call call={c,o}; SsaEnemyLifecycleOps lifecycle={life,&call};
    int result=0,kind,corpse,hurtstate,gate;
    if(!valid(c,g,o) || c->updating || c->destroy_requested || c->activation->updating ||
       c->navigation->updating || !g->activation || !math || ticks>255 ||
       c->activation->entity->type<0xcf || c->activation->entity->type>0xd2 ||
       (c->activation->entity->owner && c->activation->entity->owner->query_active)) return -1;
    a=c->activation;e=a->entity;kind=e->type-0xcf;corpse=kind==1?3:4;hurtstate=kind==1?2:3;
    c->updating=1;
    if(!a->state) result=ssa_enemy_activation_step(a,g->activation,NULL,ticks);
    else if(a->state==corpse) result=ssa_enemy_corpse_step(a,c->animation_done,&lifecycle,ticks);
    else if(a->state==hurtstate || (kind!=1 && a->state==2)) {
        if(c->animation_done) {
            if(a->state==hurtstate) e->body.flags&=0xfffe;
            gate=a->state==2 && kind==3?10:6;
            a->state=1;result=animation(c,o,walk[kind],gate);
        }
    } else if(a->state==1) {
        if(!kind) bob(c);
        if(ssa_enemy_navigation_step(c->navigation,g->player,math,ticks,speed,o->navigation)<0 ||
           ssa_enemy_control_contact(c,g,3,o)<0 ||
           ssa_enemy_drain_step(a,c->drain_period,&c->drain_countdown,ticks)<0) { result=-1;goto done; }
        if(e->body.flags&1) {
            pose.body.x=g->player_x;pose.body.y=g->player_y;
            motion=(SsaEnemyMotion){e,c->navigation->speed,a->facing,c->navigation->desired_facing,0};
            if(ssa_enemy_face(&motion,&pose,math)<0) { result=-1;goto done; }
            a->facing=c->navigation->desired_facing=motion.desired_facing;
            if(a->health<=0) {
                a->state=(uint8_t)corpse;
                if(animation(c,o,death[kind],kind==0?6:10) || ssa_enemy_death_begin(a,&lifecycle) ||
                   sound(c,o,diesound[kind])) result=-1;
            } else {
                a->state=(uint8_t)hurtstate;
                if(animation(c,o,hurt[kind],kind==2?10:6) || sound(c,o,pain[kind]) ||
                   (kind==2 && sound(c,o,0x53))) result=-1;
            }
        } else if(kind!=1) {
            gate=ssa_enemy_control_attack(c,g,ticks,o);
            if(gate<0) result=-1;
            else if(gate) {
                a->state=2;
                if(animation(c,o,attack[kind],attperiod[kind]) ||
                   emit(c,o,SSA_ENEMY_CONTROL_PROJECTILE,NULL,kind==3?0x6b:0x66,
                        0x500000,0,kind==2?0x460000:0x500000,(int32_t)a->facing*256-32768,1) ||
                   sound(c,o,shotsound[kind])) result=-1;
            }
        }
    }
done:
    c->updating=0;
    return result<0?-1:c->destroy_requested?1:0;
}
