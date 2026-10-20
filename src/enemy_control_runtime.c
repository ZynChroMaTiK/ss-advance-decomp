#include "ssa/enemy_control_runtime.h"
#include "ssa/enemy_ranged.h"
#include "projectile_runtime_internal.h"
#include "enemy_contact_internal.h"
#include <string.h>

static int ready(const SsaEnemyControlRuntime *r) {
    SsaScriptSpawn *s=r && r->combat?r->combat->spawn:NULL;
    return r && s && r->projectiles && r->projectiles->combat==r->combat &&
        r->queries && r->queries->spawn==s && r->math && r->walk_count &&
        !r->updating && !r->combat->updating && !r->projectiles->updating &&
        !r->queries->updating && !s->updating && s->world && !s->world->query_active &&
        s->level && !s->level->updating && !s->level->script.updating &&
        !s->level->movers.updating && !s->level->switches.updating &&
        r->game.activation==&s->game && r->game.player;
}
int ssa_enemy_control_runtime_init(SsaEnemyControlRuntime *r,SsaCombatRuntime *combat,
    SsaProjectileRuntime *projectiles,SsaEnemyQueries *queries,const SsaMathTables *math,
    uint32_t *count,const uint8_t *rom,size_t size) {
    if(!r || r->combat || r->updating || !combat || !combat->spawn || !projectiles ||
       !queries || !math || !count || !combat->player_count || !combat->players[0] ||
       !combat->players[0]->player) return -1;
    memset(r,0,sizeof(*r));r->combat=combat;r->projectiles=projectiles;
    r->queries=queries;r->math=math;r->walk_count=count;r->game.activation=&combat->spawn->game;
    r->game.player=&combat->players[0]->player->entity;
    r->game.player_x=combat->players[0]->player->motion.x;
    r->game.player_y=combat->players[0]->player->motion.y;
    if(!ready(r) || ssa_enemy_control_periods(&r->game,rom,size) ||
       ssa_enemy_boss_rules_load(&r->boss_rules,rom,size)<0 ||
       ssa_weapon_aim_init(&r->aim,combat->spawn->world,math)<0) {
        if(r->aim.world) ssa_weapon_aim_free(&r->aim);
        memset(r,0,sizeof(*r));return -1;
    }
    return 0;
}
int ssa_enemy_control_runtime_free(SsaEnemyControlRuntime *r) {
    if(!r || (r->combat && !ready(r))) return -1;
    if(r->aim.world && ssa_weapon_aim_free(&r->aim)<0) return -1;
    memset(r,0,sizeof(*r));return 0;
}
typedef struct Call {
    SsaEnemyControlRuntime *r;
    const SsaEnemyCdFrame *frame;
    const SsaEnemyControlRuntimeOps *ops;
    SsaCombatScope *scope;
    unsigned slot;
    SsaPlayerHazardGame hazards;
    SsaEntity *contact;
    unsigned hold_range;
} Call;
static int visibility(void *ctx,int player,int enemy,int *blocked) {
    Call *c=ctx;SsaEnemyNavigationOps *o=&c->r->queries->ops;
    return o->visibility(o->context,player,enemy,blocked);
}
static int nearest(void *ctx,SsaEntity *source,SsaEntity **out) {
    Call *c=ctx;SsaEnemyNavigationOps *o=&c->r->queries->ops;
    return o->nearest(o->context,source,out);
}
static int path(void *ctx,const SsaEnemyNavigation *n,int32_t x0,int32_t y0,int32_t x1,
    int32_t y1,int32_t radius,unsigned special,unsigned mode,int *mask,int32_t *dx,int32_t *dy) {
    Call *c=ctx;SsaEnemyNavigationOps *o=&c->r->queries->ops;
    return o->path(o->context,n,x0,y0,x1,y1,radius,special,mode,mask,dx,dy);
}
static SsaEnemyMotion motion(Call *c) {
    SsaEnemyControl *v=&c->r->controllers[c->slot];
    SsaEnemyMotion m={v->activation->entity,v->navigation->speed,v->activation->facing,
        v->navigation->desired_facing,0};return m;
}
static int turn(void *ctx,SsaEnemyNavigation *n,int32_t rate,int32_t *delta) {
    Call *c=ctx;SsaEnemyMotion m=motion(c);(void)n;
    if(rate!=65536 || ssa_enemy_turn(&m,c->frame->ticks,delta)) return -1;
    c->r->controllers[c->slot].activation->facing=m.facing;return 0;
}
static int move(void *ctx,SsaEnemyNavigation *n,int32_t *result) {
    Call *c=ctx;SsaEnemyMotion m=motion(c);int moved;(void)n;
    if(c->hold_range)m.speed=0; /* Still run vertical physics and contact queries. */
    moved=c->r->crowd_slide?
        ssa_enemy_walk_world_sliding(&m,c->r->math,c->frame->ticks,c->r->walk_count,
            c->r->combat->spawn->world,&c->r->crowd[c->slot]):
        ssa_enemy_walk_world(&m,c->r->math,c->frame->ticks,c->r->walk_count,c->r->combat->spawn->world);
    c->r->game.blocking_entity=c->r->combat->spawn->world->blocking_entity;
    if(moved<0)return -1;
    c->contact=c->r->crowd_slide?c->r->crowd[c->slot].contact:NULL;
    if(!c->contact && !moved)c->contact=c->r->game.blocking_entity;
    *result=moved;return 0;
}
static int finish(void *ctx,SsaEnemyNavigation *n) {
    Call *c=ctx;SsaEnemyActivation *a=c->r->controllers[c->slot].activation;
    SsaEntity *hit=c->contact;
    int result=ssa_enemy_hazards(n->entity,&a->health,c->r->combat->spawn->level->map,&c->hazards);
    if(result<0)return result;
    if(c->r->converted_combat && a->health>0 &&
       ssa_enemy_converted_contact(c->r->combat->spawn,n->entity,hit))
        return ssa_combat_scope_melee(c->scope,hit,n->entity,
            n->entity->type==0xd0 || n->entity->type==0xd6?40u:20u);
    return 0;
}
static int aim(void *ctx,const SsaEnemyControl *v,int32_t z,uint16_t heading,int32_t range,SsaEntity **out) {
    Call *c=ctx;int32_t slope;
    if(ssa_weapon_aim_query(&c->r->aim,v->activation->entity,0,0,z,heading,range,&slope)<0)return -1;
    *out=c->r->aim.target;return 0;
}
static int event(void *ctx,const SsaEnemyControl *v,const SsaEnemyControlEvent *ev) {
    Call *c=ctx;SsaScriptSpawn *s=c->r->combat->spawn;
    SsaScriptActor *a=&s->actors[c->slot];SsaEntity *source=v->activation->entity;
    const int32_t *args=ev->args;unsigned i;
    a->parameter34=v->drain_period;a->parameter36=v->drain_countdown;
    switch(ev->kind) {
    case SSA_ENEMY_CONTROL_DAMAGE:
        return ssa_combat_scope_melee(c->scope,ev->other,source,(uint32_t)args[0]);
    case SSA_ENEMY_CONTROL_EXPLOSION:
        return ssa_combat_scope_explode(c->scope,source,source,NULL,(uint32_t)args[0],(uint32_t)args[1],args[2]);
    case SSA_ENEMY_CONTROL_PROJECTILE: {
        SsaProjectileCreateRequest q={(uint16_t)args[0],source,args[1],args[2],args[3],(uint32_t)args[4],0};
        const SsaEntity *target=v->navigation->target; /* exact original source+20, including NULL */
        if(c->r->converted_combat && (source->body.flags&0x400) && target){
            q.heading=ssa_direction(c->r->math,
                (int16_t)(((uint32_t)target->body.x-(uint32_t)source->body.x)>>16),
                (int16_t)(((uint32_t)target->body.y-(uint32_t)source->body.y)>>16));
        }
        if(args[5]!=0 && args[5]!=1)return -1;
        return ssa_projectile_runtime_enemy_scoped(c->r->projectiles,c->scope,&q,target,args[5]);
    }
    case SSA_ENEMY_CONTROL_KNOCKBACK:
        for(i=0;i<c->r->combat->player_count;++i) {
            SsaPlayerSpawn *p=c->r->combat->players[i]->player;
            if(&p->entity==ev->other) {
                p->motion.movement.impulse_heading=(uint16_t)args[1];
                p->motion.movement.impulse=args[0];return 0;
            }
        }
        return -1;
    case SSA_ENEMY_CONTROL_GROUP_RELEASE:
        if(a->group_released || !ssa_level_script_release(&s->level->script,a->script.trigger))return -1;
        a->group_released=1;return 0;
    case SSA_ENEMY_CONTROL_SCRIPT:
        return ssa_level_script_queue(&s->level->script,&s->level->data,s->level->level,(unsigned)args[0])?0:-1;
    case SSA_ENEMY_CONTROL_DESTROY: return 0;
    case SSA_ENEMY_CONTROL_ANIMATION:
        c->r->bosses[c->slot].animation_index=0;
        a->animation=(uint16_t)args[0];a->animation_ticks=(uint8_t)args[1];
        a->animation_frame=a->animation_counter=a->animation_flags=0;break;
    default:break;
    }
    return c->ops->presentation(c->ops->context,v,ev);
}
static int boss_create(void *ctx,const SsaEnemyBoss *b,const SsaProjectileCreateRequest *q,SsaProjectile **out) {
    Call *c=ctx;(void)b;
    return ssa_projectile_runtime_enemy_create_scoped(c->r->projectiles,c->scope,q,NULL,0,out);
}
static int boss_commit(void *ctx,const SsaEnemyBoss *b,SsaProjectile *p) {
    Call *c=ctx;(void)b;
    return ssa_projectile_runtime_enemy_commit_scoped(c->r->projectiles,c->scope,p);
}
/* Prefer a clear ranged shot within 768 units. Only converted actors fighting
 * enemies use this policy; player pursuit and recovered controllers stay intact.
 * Melee-only actors still approach, at half their current chase speed. */
static int prion_tactics(Call *c,int32_t *speed) {
    SsaEnemyControlRuntime *r=c->r;SsaEnemyNavigation *n=&r->navigation[c->slot];
    SsaEntity *e=n->entity,*target=n->target;int32_t distance,slope;
    unsigned type=e->type;uint16_t heading;
    if(!r->combat->prion_balance || !(e->body.flags&0x400) || !target ||
       target->type<0xcd || target->type>0xd8 || (target->body.flags&(0x10|0x20)))return 0;
    *speed/=2;
    if(type!=0xcf && type!=0xd1 && type!=0xd2 && type!=0xd3 && type!=0xd4 && type!=0xd6)return 0;
    if(ssa_enemy_distance(target,e,r->math,&distance)<0)return -1;
    if(distance<0 || distance>768)return 0;
    heading=ssa_direction(r->math,
        (int16_t)(((uint32_t)target->body.x-(uint32_t)e->body.x)>>16),
        (int16_t)(((uint32_t)target->body.y-(uint32_t)e->body.y)>>16));
    if(ssa_weapon_aim_query(&r->aim,e,0,0,e->body.height-e->body.height/4,
        heading,0xc000000,&slope)<0)return -1;
    if(r->aim.target!=target)return 0; /* Chase around occluders instead of camping. */
    c->hold_range=1;n->phase=2;
    n->waypoint_x=(uint16_t)((uint32_t)target->body.x>>16);
    n->waypoint_y=(uint16_t)((uint32_t)target->body.y>>16);
    return 0;
}
static int run(void *ctx,SsaCombatScope *scope) {
    Call *c=ctx;SsaEnemyControlRuntime *r=c->r;SsaScriptSpawn *s=r->combat->spawn;
    SsaScriptActor *a=&s->actors[c->slot];SsaEnemyControl *v=&r->controllers[c->slot];
    SsaEnemyNavigationOps nav={visibility,nearest,path,turn,move,finish,c};
    SsaEnemyControlOps ops={&nav,aim,event,c};int result;int32_t speed=c->frame->speed_factor;
    c->scope=scope;r->game.blocking_entity=s->world->blocking_entity;
    v->animation_done=a->animation_flags;v->drain_period=a->parameter34;v->drain_countdown=a->parameter36;
    if(a->activation.state==1 && a->activation.health>0 && prion_tactics(c,&speed)<0)return -1;
    if(a->entity.type>=0xd7){
        SsaEnemyBossOps boss_ops={&ops,boss_create,boss_commit,c};
        r->bosses[c->slot].animation_index=a->animation_frame;
        result=ssa_enemy_boss_step(&r->bosses[c->slot],&r->game,&r->boss_rules,
            s->level->script.alive,sizeof(s->level->script.alive),r->math,c->frame->ticks,&boss_ops);
    }else if(a->entity.type>=0xcf && a->entity.type<=0xd2)
        result=ssa_enemy_basic_step(v,&r->game,r->math,c->frame->ticks,speed,&ops);
    else result=ssa_enemy_ranged_step(v,&r->game,r->math,c->frame->ticks,speed,&ops);
    a->script.flags=a->entity.body.flags;a->script.timer=a->activation.timer;
    a->script.variant=a->activation.state;a->script.facing=a->activation.facing;
    a->animation_flags=v->animation_done;a->parameter34=v->drain_period;a->parameter36=v->drain_countdown;
    return result;
}
int ssa_enemy_control_runtime_step(SsaEnemyControlRuntime *r,unsigned slot,
    const SsaEnemyCdFrame *frame,const SsaEnemyControlRuntimeOps *ops) {
    Call c={r,frame,ops,NULL,slot,{0},0,0};SsaScriptSpawn *s;SsaScriptActor *a;
    SsaEnemyControl *v;unsigned i;int result;
    if(!ready(r) || r->crowd_slide>1 || r->converted_combat>1 || slot>=100 || !frame || frame->ticks>255 || !ops || !ops->combat ||
       !ops->combat->event || !ops->presentation || !r->queries->ops.visibility ||
       !r->queries->ops.nearest || !r->queries->ops.path)return -1;
    s=r->combat->spawn;a=&s->actors[slot];v=&r->controllers[slot];
    if(!s->pool.occupied[slot] || !a->owned || a->entity.owner!=s->world ||
       a->entity.type<0xce || a->entity.type>0xd8 || a->activation.updating ||
       s->level->movers.hazard_count>8 || r->navigation[slot].updating || v->updating)return -1;
    if(!r->active[slot]) {
        if(a->activation.state)return -1;
        memset(v,0,sizeof(*v));memset(&r->navigation[slot],0,sizeof(r->navigation[slot]));
        memset(&r->crowd[slot],0,sizeof(r->crowd[slot]));
        v->activation=&a->activation;v->navigation=&r->navigation[slot];
        memset(&r->bosses[slot],0,sizeof(r->bosses[slot]));r->bosses[slot].control=v;
        v->navigation->entity=&a->entity;v->navigation->heading=a->activation.saved_facing;
        r->active[slot]=1;
    } else if(v->activation!=&a->activation || v->navigation!=&r->navigation[slot] ||
              v->navigation->entity!=&a->entity || v->destroy_requested)return -1;
    c.hazards.frame_counter=frame->frame_counter;c.hazards.level_counter=frame->level_counter;
    c.hazards.hazard_count=s->level->movers.hazard_count;
    for(i=0;i<c.hazards.hazard_count;++i)s->level->hazard_sectors[i]=(int32_t)s->level->movers.hazard_sectors[i];
    c.hazards.hazard_sectors=s->level->hazard_sectors;
    r->updating=1;result=ssa_combat_scope_run(r->combat,ops->combat,run,&c);
    if(v->destroy_requested) {
        if(ssa_script_spawn_release(s,slot))result=-1;
        else r->active[slot]=0;
    }
    r->updating=0;return result;
}
