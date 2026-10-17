#include "ssa/enemy_boss.h"
#include "ssa/slide.h"

typedef struct Call { SsaEnemyBoss *b;const SsaEnemyBossOps *ops; } Call;
static int32_t bits(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)~x;}
static int half(uint16_t x){return x<32768?(int)x:(int)x-65536;}
static int emit(Call *p,SsaEnemyControlEventKind kind,int a,int b,int c,int d)
{
    SsaEnemyControlEvent ev={kind,NULL,{a,b,c,d,0,0}};
    const SsaEnemyControlOps *o=p->ops->control;
    return o->event(o->context,p->b->control,&ev)<0?-1:0;
}
static int animation(Call *p,unsigned id,unsigned period)
{p->b->control->animation_done=0;return emit(p,SSA_ENEMY_CONTROL_ANIMATION,(int)id,(int)period,0,0);}
static int life(void *ctx,const SsaEnemyActivation *a,SsaEnemyLifecycleEvent kind)
{
    Call *p=ctx;int result;(void)a;
    result=emit(p,kind==SSA_ENEMY_LIFECYCLE_DESTROY?SSA_ENEMY_CONTROL_DESTROY:SSA_ENEMY_CONTROL_GROUP_RELEASE,0,0,0,0);
    if(result>=0 && kind==SSA_ENEMY_LIFECYCLE_DESTROY)p->b->control->destroy_requested=1;
    return result;
}
static int activate(void *ctx,const SsaEnemyActivation *a,const SsaEnemyActivationGame *g,
    SsaEnemyActivationEvent kind,int32_t x,int32_t y,int32_t z,int32_t w)
{
    Call *p=ctx;(void)a;(void)g;
    if(kind==SSA_ENEMY_ACTIVATION_ANIMATION)return animation(p,(unsigned)x,(unsigned)y);
    return emit(p,kind==SSA_ENEMY_ACTIVATION_MUSIC?SSA_ENEMY_CONTROL_MUSIC:
        kind==SSA_ENEMY_ACTIVATION_NOTICE?SSA_ENEMY_CONTROL_NOTICE:SSA_ENEMY_CONTROL_SOUND,x,y,z,w);
}
int ssa_enemy_boss_rules_load(SsaEnemyBossRules *r,const uint8_t *rom,size_t size)
{
    unsigned i;
    if(!r || !rom || size<0x44c07c)return -1;
    for(i=0;i<2;++i){r->groups[0][i]=rom[0x44c078+i];r->scripts[0][i]=rom[0x44c07a+i];
        r->groups[1][i]=rom[0x44c074+i];r->scripts[1][i]=rom[0x44c076+i];}
    r->loaded=1;return 0;
}
static int face(Call *p,const SsaEntity *target,const SsaMathTables *math,unsigned ticks,int32_t *delta)
{
    SsaEnemyControl *c=p->b->control;
    SsaEnemyMotion m={c->activation->entity,c->navigation->speed,c->activation->facing,c->navigation->desired_facing,0};
    if(target && ssa_enemy_face(&m,target,math)<0)return -1;
    if(ssa_enemy_turn(&m,ticks,delta)<0)return -1;
    c->activation->facing=m.facing;c->navigation->desired_facing=m.desired_facing;return 0;
}
static int reaction(Call *p,SsaEnemyControlGame *g,const SsaMathTables *math,unsigned kind)
{
    SsaEnemyControl *c=p->b->control;SsaEnemyActivation *a=c->activation;
    SsaEntity pose={0};SsaEnemyMotion m={a->entity,c->navigation->speed,a->facing,c->navigation->desired_facing,0};
    SsaEnemyLifecycleOps ops={life,p};
    if(!(a->entity->body.flags&1))return 0;
    pose.body.x=g->player_x;pose.body.y=g->player_y;
    if(ssa_enemy_face(&m,&pose,math)<0)return -1;
    a->facing=m.desired_facing;c->navigation->desired_facing=m.desired_facing;
    if(a->health<=0){
        a->state=7;
        if(animation(p,kind?0x4e:0x54,kind?8:10)<0 || ssa_enemy_death_begin(a,&ops)<0 ||
           emit(p,SSA_ENEMY_CONTROL_SOUND,kind?0x66:0x62,0,0,0)<0 || emit(p,SSA_ENEMY_CONTROL_MUSIC,20,0,0,0)<0)return -1;
    }else{
        if(a->state==5){a->state=6;if(animation(p,kind?0x4f:0x55,kind?6:10)<0)return -1;}
        else a->entity->body.flags&=UINT16_C(0xfffe);
        if(!a->timer){a->timer=60;if(emit(p,SSA_ENEMY_CONTROL_SOUND,kind?0x63:0x5b,0,0,0)<0)return -1;}
    }
    return 0;
}
static int reset(Call *p,unsigned kind)
{
    SsaEnemyActivation *a=p->b->control->activation;
    a->boss_ticks=5;a->state=1;
    if(animation(p,kind?0x52:0x58,10)<0)return -1;
    a->entity->body.flags&=UINT16_C(0xefff);return 0;
}
static int shoot(Call *p,SsaEnemyControlGame *g,const SsaMathTables *math,unsigned kind)
{
    SsaEnemyControl *c=p->b->control;SsaEnemyActivation *a=c->activation;
    SsaProjectile *shot=NULL;SsaProjectileCreateRequest q={0x6a,a->entity,0x600000,0,0x400000,0,0};
    int32_t distance,dz,slope;int64_t product;uint16_t heading;
    heading=ssa_direction(math,(int16_t)half((uint16_t)(((uint32_t)g->player->body.x-(uint32_t)a->entity->body.x)>>16)),
        (int16_t)half((uint16_t)(((uint32_t)g->player->body.y-(uint32_t)a->entity->body.y)>>16)));
    if(ssa_enemy_distance(g->player,a->entity,math,&distance)<0)return -1;
    q.heading=heading;
    if(!p->ops->create || !p->ops->commit || p->ops->create(p->ops->context,p->b,&q,&shot)<0 || !shot || !shot->entity)return -1;
    if(emit(p,SSA_ENEMY_CONTROL_SOUND,kind?0x65:0x60,0,0,0)<0)return -1;
    dz=bits((uint32_t)g->player->body.z+(uint32_t)g->player->body.height-
        (uint32_t)(g->player->body.height/2)-(uint32_t)shot->entity->body.z);
    slope=ssa_trace_divide(dz,bits((uint32_t)distance<<16));
    product=(int64_t)shot->speed*slope;
    shot->entity->body.velocity_z=bits((uint32_t)((uint64_t)product>>16));
    if(kind)shot->state=1;
    if(p->ops->commit(p->ops->context,p->b,shot)<0)return -1;
    --a->boss_ticks;a->entity->body.flags|=0x1000;return 0;
}
int ssa_enemy_boss_step(SsaEnemyBoss *b,SsaEnemyControlGame *g,const SsaEnemyBossRules *rules,
    const uint8_t *alive,size_t count,const SsaMathTables *math,unsigned ticks,const SsaEnemyBossOps *ops)
{
    Call p={b,ops};SsaEnemyControl *c;SsaEnemyActivation *a;SsaEntity *e;
    SsaEnemyLifecycleOps lifecycle={life,&p};SsaEnemyActivationOps init={activate,&p};
    unsigned kind,state;int result=0;int32_t delta;
    if(!b || !(c=b->control) || !c->activation || !c->navigation || !(a=c->activation)->entity ||
       c->navigation->entity!=a->entity || c->updating || c->destroy_requested || a->updating || c->navigation->updating ||
       !g || !g->activation || !g->player || g->activation->boss_phase>1 || !rules || !rules->loaded ||
       !alive || count<256 || !math || ticks>255 || !ops || !ops->control || !ops->control->event ||
       (a->entity->owner && a->entity->owner->query_active))return -1;
    e=a->entity;if(e->type!=0xd7 && e->type!=0xd8)return -1;
    kind=e->type-0xd7;state=a->state;c->updating=1;
    /* Outer boss dispatcher ages this timer before ALL handlers. Corpse and
     * delayed activation handlers also age it internally: retain both steps. */
    if(a->timer){a->timer=(uint16_t)(a->timer-ticks);if(half(a->timer)<0)a->timer=0;}
    switch(state){
    case 0:result=ssa_enemy_activation_step(a,g->activation,&init,ticks);break;
    case 1:
        result=face(&p,g->player,math,ticks,&delta);
        if(!result && delta>=-1023 && delta<=1023){a->state=2;result=animation(&p,kind?0x50:0x56,kind?6:10);}
        break;
    case 2:
        if(b->animation_index<=(kind?6u:8u))result=face(&p,g->player,math,ticks,&delta);
        else if(!(e->body.flags&0x1000))result=shoot(&p,g,math,kind);
        if(result<0)break;
        if(c->animation_done){
            if(a->boss_ticks){e->body.flags&=UINT16_C(0xefff);a->state=1;}
            else a->state=3;
            result=animation(&p,kind?0x52:0x58,10);
        }
        if(!result)result=reaction(&p,g,math,kind);
        break;
    case 3:
        c->navigation->desired_facing=(uint8_t)a->saved_facing;
        result=face(&p,NULL,math,ticks,&delta);
        if(!result && delta>=-1023 && delta<=1023){a->facing=(uint8_t)a->saved_facing;a->state=4;
            result=animation(&p,kind?0x51:0x57,10);if(!result)b->summon_cycles=1;}
        break;
    case 4:
        if(c->animation_done){
            result=emit(&p,SSA_ENEMY_CONTROL_SCRIPT,rules->scripts[kind][g->activation->boss_phase],0,0,0);
            if(result<0)break;
            --b->summon_cycles;
            if(b->summon_cycles){result=animation(&p,kind?0x51:0x57,10);
                if(!result)result=emit(&p,SSA_ENEMY_CONTROL_SOUND,kind?0x64:0x5e,0,0,0);}
            else{a->state=5;result=animation(&p,kind?0x4d:0x53,10);}
        }
        if(!result)result=reaction(&p,g,math,kind);
        break;
    case 5:
        result=face(&p,g->player,math,ticks,&delta);
        if(!result && !alive[rules->groups[kind][g->activation->boss_phase]]){
            g->activation->boss_phase=(uint8_t)((g->activation->boss_phase+1)&1);result=reset(&p,kind);}
        if(!result)result=reaction(&p,g,math,kind);
        break;
    case 6:
        if(c->animation_done){a->state=5;e->body.flags&=UINT16_C(0xfffe);result=animation(&p,kind?0x4d:0x53,10);}
        break;
    case 7:g->activation->boss=NULL;result=ssa_enemy_corpse_step(a,c->animation_done,&lifecycle,ticks);break;
    default:break;
    }
    c->updating=0;return result<0?-1:c->destroy_requested?1:0;
}
