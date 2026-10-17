#include "ssa/enemy_ranged.h"

typedef struct Call {SsaEnemyControl *c;const SsaEnemyControlOps *ops;} Call;
static int32_t bits(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int half(uint16_t v){return v<32768u?(int)v:(int)v-65536;}
static int emit(Call *p,SsaEnemyControlEventKind kind,SsaEntity *other,
    int32_t a,int32_t b,int32_t c,int32_t d,int32_t e,int32_t f)
{
    SsaEnemyControlEvent event={kind,other,{a,b,c,d,e,f}};
    return p->ops->event(p->ops->context,p->c,&event)<0?-1:0;
}
static int animation(Call *p,unsigned id,unsigned period)
{p->c->animation_done=0;return emit(p,SSA_ENEMY_CONTROL_ANIMATION,NULL,(int32_t)id,(int32_t)period,0,0,0,0);}
static int sound(Call *p,unsigned id)
{return emit(p,SSA_ENEMY_CONTROL_SOUND,NULL,(int32_t)id,0,0,0,0,0);}
static int lifecycle(void *context,const SsaEnemyActivation *a,SsaEnemyLifecycleEvent kind)
{
    Call *p=context;int result;(void)a;
    result=emit(p,kind==SSA_ENEMY_LIFECYCLE_GROUP_RELEASE?SSA_ENEMY_CONTROL_GROUP_RELEASE:SSA_ENEMY_CONTROL_DESTROY,NULL,0,0,0,0,0,0);
    if(result>=0 && kind==SSA_ENEMY_LIFECYCLE_DESTROY)p->c->destroy_requested=1;
    return result;
}
static int activation(void *context,const SsaEnemyActivation *a,
    const SsaEnemyActivationGame *g,SsaEnemyActivationEvent kind,
    int32_t x,int32_t y,int32_t z,int32_t w)
{
    Call *p=context;(void)a;(void)g;
    if(kind!=SSA_ENEMY_ACTIVATION_SOUND)return -1;
    return emit(p,SSA_ENEMY_CONTROL_SOUND,NULL,x,y,z,w,0,0);
}
static int reaction(Call *p,SsaEnemyControlGame *g,const SsaMathTables *math,
    unsigned ticks,const SsaEnemyLifecycleOps *life)
{
    SsaEnemyActivation *a=p->c->activation;SsaEnemyNavigation *n=p->c->navigation;
    SsaEntity pose={0};SsaEnemyMotion m={a->entity,n->speed,a->facing,n->desired_facing,0};
    unsigned type=a->entity->type,deadstate,deadanim,deadperiod,hurtstate,hurtanim,hurtperiod;
    if(ssa_enemy_drain_step(a,p->c->drain_period,&p->c->drain_countdown,ticks)<0)return -1;
    if(!(a->entity->body.flags&1))return 0;
    pose.body.x=g->player_x;pose.body.y=g->player_y;
    if(ssa_enemy_face(&m,&pose,math)<0)return -1;
    a->facing=m.desired_facing;n->desired_facing=m.desired_facing;
    switch(type){
    case 0xce:deadstate=3;deadanim=0x17;deadperiod=10;hurtstate=2;hurtanim=0x18;hurtperiod=6;break;
    case 0xd3:deadstate=4;deadanim=0x46;deadperiod=10;hurtstate=3;hurtanim=0x47;hurtperiod=6;break;
    case 0xd4:deadstate=4;deadanim=0x4a;deadperiod=6;hurtstate=3;hurtanim=0x4b;hurtperiod=6;break;
    case 0xd5:deadstate=6;deadanim=0x41;deadperiod=10;hurtstate=2;hurtanim=0x42;hurtperiod=6;break;
    default:deadstate=4;deadanim=0x1a;deadperiod=10;hurtstate=2;hurtanim=0x1b;hurtperiod=10;break;
    }
    if(a->health<=0){
        a->state=(uint8_t)deadstate;
        if(animation(p,deadanim,deadperiod)<0 || ssa_enemy_death_begin(a,life)<0)return -1;
        if(type==0xce && sound(p,0x4f)<0)return -1;
        if(type==0xd3 && sound(p,0x3e)<0)return -1;
        if(type==0xd4 && sound(p,0x41)<0)return -1;
    }else{
        a->state=(uint8_t)hurtstate;
        if(animation(p,hurtanim,hurtperiod)<0)return -1;
        if(type==0xd3 && sound(p,0x3b)<0)return -1;
        if(type==0xd4 && sound(p,0x3f)<0)return -1;
    }
    return 1;
}
static int projectile(Call *p,unsigned type,int32_t speed)
{
    int32_t heading=((int32_t)p->c->activation->facing<<8)-32768;
    return emit(p,SSA_ENEMY_CONTROL_PROJECTILE,NULL,(int32_t)type,0x500000,0,speed,heading,1);
}
static int knockback(Call *p,SsaEnemyControlGame *g,const SsaMathTables *math)
{
    SsaEntity *e=p->c->activation->entity;int32_t sine,cosine,delta,impulse;int64_t product;
    uint16_t angle=ssa_direction(math,
        (int16_t)half((uint16_t)(((uint32_t)g->player->body.x-(uint32_t)e->body.x)>>16)),
        (int16_t)half((uint16_t)(((uint32_t)g->player->body.y-(uint32_t)e->body.y)>>16)));
    delta=half(angle)-(half((uint16_t)((unsigned)p->c->activation->facing<<8))-32768);
    if(delta>32768)delta-=65536;else if(delta< -32768)delta+=65536;
    if(delta<0)delta=-delta;
    if(delta>16383)return 0;
    ssa_sincos(math,(uint16_t)delta,&sine,&cosine);(void)sine;
    product=(int64_t)p->c->navigation->speed*cosine;
    impulse=bits((uint32_t)((uint64_t)product>>16));
    return emit(p,SSA_ENEMY_CONTROL_KNOCKBACK,g->player,impulse,angle,0,0,0,0);
}
static int walk(Call *p)
{
    int32_t ignored;
    p->c->navigation->speed=0;
    if(!p->ops->navigation || !p->ops->navigation->move)return -1;
    return p->ops->navigation->move(p->ops->navigation->context,p->c->navigation,&ignored)<0?-1:0;
}
static int resume(Call *p,unsigned animation_id,unsigned period,int clear_hurt)
{
    if(!p->c->animation_done)return 0;
    p->c->activation->state=1;
    if(clear_hurt)p->c->activation->entity->body.flags&=UINT16_C(0xfffe);
    return animation(p,animation_id,period);
}
static int ce_loop(Call *p,SsaEnemyControlGame *g)
{
    SsaEnemyNavigation *n=p->c->navigation;SsaEntity *e=p->c->activation->entity;
    unsigned phase=p->c->destroy_requested?0:n->phase;
    unsigned state=p->c->destroy_requested?0:p->c->activation->state;
    int distance=half(n->distance);
    if(phase==2 && distance<=399 && distance && !g->loop_source && state!=3){
        g->loop_source=e;return sound(p,0x4e);
    }
    if(g->loop_source==e && (phase!=2 || distance>400 || state==3)){
        g->loop_source=NULL;return emit(p,SSA_ENEMY_CONTROL_STOP_SOUND,NULL,0x4e,0,0,0,0,0);
    }
    return 0;
}
static int active(Call *p,SsaEnemyControlGame *g,const SsaMathTables *math,
    unsigned ticks,int32_t speed,const SsaEnemyLifecycleOps *life)
{
    SsaEnemyActivation *a=p->c->activation;SsaEnemyNavigation *n=p->c->navigation;
    SsaEntity *e=a->entity;unsigned type=e->type;int result;int32_t distance;
    if(type==0xd5)n->speed=0x10000;
    if(type==0xd4 && bits((uint32_t)e->body.z-(uint32_t)g->player->body.z)>0x7fffff){
        SsaEnemyMotion m={e,n->speed,a->facing,n->desired_facing,0};int32_t delta;
        n->target=g->player;
        if(ssa_enemy_face(&m,g->player,math)<0 || ssa_enemy_turn(&m,ticks,&delta)<0)return -1;
        a->facing=m.facing;n->desired_facing=m.desired_facing;
        if(delta<0)delta=-delta;
        if(delta<=1999)n->navigation_flags|=1;else n->navigation_flags&=UINT8_C(0xfe);
        if(!p->ops->navigation || !p->ops->navigation->finish ||
            p->ops->navigation->finish(p->ops->navigation->context,n)<0)return -1;
    }else if(ssa_enemy_navigation_step(n,g->player,math,ticks,speed,p->ops->navigation)<0)return -1;
    if(type==0xd5){
        if(n->phase==2 && half(n->distance)<=255 &&
            (uint16_t)((uint32_t)e->body.z>>16)==(uint16_t)e->body.floor)e->body.velocity_z=0x50000;
        if(ssa_enemy_distance(g->player,e,math,&distance)<0)return -1;
        if((uint32_t)distance>767u){
            a->state=3;e->body.flags&=UINT16_C(0xfffd);
            if(animation(p,0x8a,6)<0)return -1;
        }
    }
    result=ssa_enemy_control_contact(p->c,g,3,p->ops);
    if(result<0)return -1;
    if(type==0xce && result && knockback(p,g,math)<0)return -1;
    result=reaction(p,g,math,ticks,life);
    if(result<0 || result || type==0xce || type==0xd5)return result<0?-1:0;
    result=ssa_enemy_control_attack(p->c,g,ticks,p->ops);
    if(result<=0)return result;
    if(type==0xd3){a->state=2;if(animation(p,0x48,10)<0 || projectile(p,0x66,0x640000)<0 || sound(p,0x3c)<0)return -1;}
    else if(type==0xd4){a->state=2;if(animation(p,0x4c,6)<0 || sound(p,0x40)<0)return -1;}
    else{a->state=3;if(animation(p,0x8d,6)<0 || projectile(p,0x67,0x500000)<0 || sound(p,0x1b)<0)return -1;}
    return 0;
}
int ssa_enemy_ranged_step(SsaEnemyControl *c,SsaEnemyControlGame *g,
    const SsaMathTables *math,unsigned ticks,int32_t speed,const SsaEnemyControlOps *ops)
{
    Call p={c,ops};SsaEnemyLifecycleOps life={lifecycle,&p};SsaEnemyActivationOps init={activation,&p};
    SsaEnemyActivation *a;unsigned type,state;int result=0;int32_t distance;
    if(!c || c->updating || c->destroy_requested || !c->activation || !c->navigation ||
        c->activation->updating || c->navigation->updating || !c->activation->entity ||
        c->navigation->entity!=c->activation->entity || !g || !g->activation || !g->player ||
        !math || ticks>255 || !ops || !ops->event ||
        (c->activation->entity->owner && c->activation->entity->owner->query_active))return -1;
    a=c->activation;type=a->entity->type;state=a->state;
    if(type!=0xce && (type<0xd3 || type>0xd6))return -1;
    c->updating=1;
    if(!state)result=ssa_enemy_activation_step(a,g->activation,&init,ticks);
    else if(state==1)result=active(&p,g,math,ticks,speed,&life);
    else if(state==(type==0xce?3u:type==0xd5?6u:4u))result=ssa_enemy_corpse_step(a,c->animation_done,&life,ticks);
    else if(type==0xce && state==2)result=resume(&p,10,5,1);
    else if(type==0xd3){if(state==2)result=resume(&p,0x45,6,0);else if(state==3)result=resume(&p,0x45,6,1);}
    else if(type==0xd4){
        if(state==3)result=resume(&p,0x49,6,1);
        else if(state==2){
            if(c->animation_done==0xfa){c->animation_done=0;result=projectile(&p,0x66,0x5a0000);}
            else if(c->animation_done==0xfe){a->state=1;result=animation(&p,0x49,6);}
        }
    }else if(type==0xd6){if(state==2)result=resume(&p,0x19,6,1);else if(state==3)result=resume(&p,0x19,6,0);}
    else if(type==0xd5 && state<=5){
        result=walk(&p);
        if(result>=0){
            if(state==2)result=resume(&p,0x40,6,1);
            else if(state==3 && c->animation_done){a->state=4;a->entity->body.flags&=UINT16_C(0xfffd);result=animation(&p,0x8b,6);}
            else if(state==4){
                result=ssa_enemy_distance(g->player,a->entity,math,&distance);
                if(result>=0 && (uint32_t)distance<=511u){a->state=5;result=animation(&p,0x8c,6);}
            }else if(state==5 && c->animation_done){a->state=1;a->entity->body.flags|=2;result=animation(&p,0x40,6);}
        }
    }
    if(result>=0 && type==0xce)result=ce_loop(&p,g);
    c->updating=0;return result<0?-1:c->destroy_requested?1:0;
}
