#include "ssa/entity_runtime.h"
#include "ssa/entity_timer.h"
#include <string.h>

static int idle(const SsaEntityRuntime *r) {
    SsaScriptSpawn *s=r && r->combat?r->combat->spawn:NULL;unsigned i;
    if(!s || !s->world || !s->level || r->fast_converted>1 || r->nearest_converted>1 || r->updating || r->dispatch.updating ||
       r->combat->updating || !r->combat->damage || r->combat->damage->updating ||
       r->combat->explosion.updating || !r->projectiles || r->projectiles->combat!=r->combat ||
       r->projectiles->updating || !r->queries || r->queries->spawn!=s || r->queries->updating ||
       r->queries->path.updating || r->queries->ray.updating || s->updating ||
       s->world->query_active || s->level->updating || s->level->script.updating ||
       s->level->movers.updating || s->level->switches.updating ||
       !r->combat->player_count || r->combat->player_count>4 || s->reserved>100 ||
       s->reserved!=r->combat->reserved_count || s->pool.active_count>100 ||
       s->pool.active_count!=s->world->entity_count) return 0;
    for(i=0;i<r->combat->player_count;++i)
        if(!r->players[i] || r->players[i]->runtime!=r->combat->players[i] ||
           r->players[i]->updating || r->players[i]->runtime->updating) return 0;
    for(i=0;i<100;++i) if(r->enemies[i] && r->enemies[i]->updating) return 0;
    return 1;
}
int ssa_entity_runtime_init(SsaEntityRuntime *r,SsaCombatRuntime *combat,SsaProjectileRuntime *p,
    SsaEnemyQueries *q,SsaPlayerGameplay *const *players,uint16_t tail) {
    unsigned i;
    if(!r || r->combat || r->updating || !combat || !combat->spawn || !players ||
       !combat->player_count || combat->player_count>4 || combat->reserved_count>100) return -1;
    memset(r,0,sizeof(*r));r->combat=combat;r->projectiles=p;r->queries=q;
    for(i=0;i<combat->player_count;++i)r->players[i]=players[i];
    if(!idle(r)) {memset(r,0,sizeof(*r));return -1;}
    for(i=0;i<100;++i)r->entities[i]=combat->spawn->actors[i].owned || i>=combat->reserved_count?
        &combat->spawn->actors[i].entity:combat->reserved[i];
    r->dispatch.pool=&combat->spawn->pool;r->dispatch.entities=r->entities;
    r->dispatch.current=r->entities[0];r->dispatch.active_tail=tail;return 0;
}
int ssa_entity_runtime_bind_cd(SsaEntityRuntime *r,SsaEnemyCdRuntime *cd) {
    unsigned slot;
    if(!idle(r) || !cd || !cd->active || cd->updating || cd->spawn!=r->combat->spawn ||
       cd->slot>=100 || !cd->walk_count ||
       cd->controller.updating || cd->navigation.updating) return -1;
    slot=cd->slot;
    if(r->enemies[slot] || !cd->spawn->pool.occupied[slot] || !cd->spawn->actors[slot].owned ||
       cd->navigation.entity!=r->entities[slot] || r->entities[slot]->type!=0xcd) return -1;
    r->enemies[slot]=cd;r->previous_walk_count[slot]=cd->walk_count;
    cd->walk_count=&r->dispatch.walk_count;return 0;
}
int ssa_entity_runtime_bind_generic(SsaEntityRuntime *r,SsaEnemyControlRuntime *g,
    const SsaEnemyControlRuntimeOps *ops) {
    if(!idle(r) || r->generic || !g || g->combat!=r->combat ||
       g->projectiles!=r->projectiles || g->queries!=r->queries || g->updating ||
       g->walk_count!=&r->dispatch.walk_count || !ops || !ops->combat || !ops->presentation) return -1;
    r->generic=g;r->generic_ops=ops;return 0;
}
static void detach(SsaEntityRuntime *r,unsigned slot) {
    r->enemies[slot]->walk_count=r->previous_walk_count[slot];
    r->enemies[slot]=NULL;r->previous_walk_count[slot]=NULL;
}
int ssa_entity_runtime_unbind_cd(SsaEntityRuntime *r,unsigned slot) {
    if(!idle(r) || slot>=100 || !r->enemies[slot]) return -1;
    detach(r,slot);return 0;
}
int ssa_entity_runtime_free(SsaEntityRuntime *r) {
    unsigned i;if(!idle(r)) return -1;
    for(i=0;i<100;++i)if(r->enemies[i])detach(r,i);
    memset(r,0,sizeof(*r));return 0;
}
typedef struct Call { SsaEntityRuntime *r;const SsaEntityRuntimeFrame *frame;const SsaEntityRuntimeOps *ops;
    SsaEnemyCdGame *previous_cd_game; } Call;
typedef struct PlayerCall {SsaEntityRuntime *r;SsaPlayerGameplay *g;const SsaLevelPlayerOps *ops;} PlayerCall;
static void mirror_life(PlayerCall *c) {
    if(c->r->shared_life_game)*c->r->shared_life_game=c->g->life_game;
}
static int player_event(void *context,SsaPlayerGameplay *g,enum SsaPlayerLifecycleEvent event,
    int32_t a,int32_t b,int32_t c,int32_t d) {
    PlayerCall *p=context;int result;
    mirror_life(p);
    result=p->ops->event(p->ops->context,g,event,a,b,c,d);
    mirror_life(p);return result;
}
static int player_animation(void *context,SsaPlayerRuntime *r,unsigned id,unsigned ticks) {
    PlayerCall *p=context;int result;
    mirror_life(p);
    result=p->ops->animation(p->ops->context,r,id,ticks);
    mirror_life(p);return result;
}
static int player_sound(void *context,SsaPlayerRuntime *r,unsigned id) {
    PlayerCall *p=context;int result;
    mirror_life(p);
    result=p->ops->sound(p->ops->context,r,id);
    mirror_life(p);return result;
}
static void mirror_cd(Call *c) {
    unsigned i;SsaEnemyCdGame *shared=c->r->shared_cd_game;
    if(!shared) return;
    if(c->previous_cd_game && c->previous_cd_game!=shared)*c->previous_cd_game=*shared;
    for(i=0;i<100;++i) if(c->r->enemies[i] && c->r->enemies[i]->game &&
        c->r->enemies[i]->game!=shared)*c->r->enemies[i]->game=*shared;
}
static int cd_presentation(void *context,const SsaEnemyCd *cd,SsaEnemyCdEvent event,
    SsaEntity *other,int32_t a,int32_t b,int32_t d) {
    Call *c=context;const SsaCombatCdOps *ops=c->ops->enemies;
    mirror_cd(c);
    return ops->presentation(ops->context,cd,event,other,a,b,d);
}
/* PC conversion policy. Use canonical health and the active pool, never stale
 * navigation pointers. The distance is between body centers in3D, at1/256-unit
 * precision; differences/squares fit64bits even across the full coordinate
 * range. Equal distances retain active order with no player/type preference. */
static int hostile_target(SsaEntityRuntime *r,SsaEntity *source,SsaEntity **out)
{
    SsaScriptSpawn *s=r->combat->spawn;SsaEntity *best=NULL;uint64_t distance=UINT64_MAX;
    unsigned i,j;
    for(i=0;i<s->pool.active_count;++i){
        unsigned slot=s->pool.active_slots[i];SsaEntity *e;
        int living=0;int64_t dx,dy,dz;uint64_t squared;
        if(slot>=100 || !s->pool.occupied[slot] || s->pool.positions[slot]!=i)return -1;
        e=r->entities[slot];if(!e || e->owner!=s->world)return -1;
        if(e==source || (e->body.flags&0x20))continue;
        if(e->body.flags&0x10){
            for(j=0;j<r->combat->player_count;++j)if(e==&r->players[j]->runtime->player->entity){
                SsaPlayerSpawn *p=r->players[j]->runtime->player;
                living=p->inventory.health>0 && (p->motion.movement.flags&4);break;
            }
        }else if(e->type>=0xcd && e->type<=0xd8 && s->actors[slot].owned &&
                 ((source->body.flags|e->body.flags)&0x400))
            living=s->actors[slot].activation.health>0;
        if(!living)continue;
        dx=((int64_t)e->body.x-source->body.x)/256;
        dy=((int64_t)e->body.y-source->body.y)/256;
        dz=((int64_t)e->body.z+e->body.height/2-source->body.z-source->body.height/2)/256;
        squared=(uint64_t)(dx*dx)+(uint64_t)(dy*dy)+(uint64_t)(dz*dz);
        if(squared<distance){distance=squared;best=e;}
    }
    *out=best;return 0;
}
static int converted_select(SsaEntityRuntime *r,SsaEntity *e,SsaEnemyNavigation *n,SsaEntity **target)
{
    if(!r->nearest_converted || e->type<0xcd || e->type>0xd8 ||
       (e->body.flags&0x20))
        return 0;
    if(hostile_target(r,e,target)<0)return -1;
    if(n->target!=*target){n->phase=0;n->navigation_flags&=UINT8_C(0xfe);}
    n->target=*target;n->target_timer=600;
    return 1;
}
static int handler(void *context,SsaEntityDispatch *dispatch,uint32_t entry,unsigned slot) {
    Call *c=context;SsaEntityRuntime *r=c->r;SsaScriptSpawn *s=r->combat->spawn;
    SsaEntity *e=r->entities[slot];int result=-1;unsigned i,pass;
    unsigned repeats=r->fast_converted && c->frame->enemies.ticks && e->type>=0xcd && e->type<=0xd6 &&
        (e->body.flags&0x400) && !(e->body.flags&0x20) && s->actors[slot].activation.health>0?2u:1u;
    if(dispatch!=&r->dispatch || e->owner!=s->world || !s->pool.occupied[slot]) return -1;
    r->combat->damage->source=e;r->combat->damage->source_owner=NULL;
    if(c->ops->before && c->ops->before(c->ops->context,r,entry,slot)<0) return -1;
    if(entry==0x08005ccc) {
        const uint16_t *previous_flags;SsaPlayerGameplay *g;unsigned local;
        PlayerCall pc;SsaLevelPlayerOps adapters;const SsaLevelPlayerOps *original;
        for(i=0;i<r->combat->player_count && e!=&r->players[i]->runtime->player->entity;++i) {}
        if(i==r->combat->player_count || !c->ops->players[i]) return -1;
        original=c->ops->players[i];
        if(!original->event || !original->animation || !original->sound) return -1;
        local=c->frame->players[i].local_player;
        if(local>=r->combat->player_count) return -1;
        g=r->players[i];previous_flags=g->display_flags;
        g->display_flags=&r->players[local]->runtime->player->motion.movement.flags;
        if(r->shared_life_game)g->life_game=*r->shared_life_game;
        pc=(PlayerCall){r,g,original};
        adapters=(SsaLevelPlayerOps){player_event,player_animation,player_sound,&pc};
        result=ssa_level_runtime_player_step(s->level,g,&c->frame->players[i],&adapters);
        if(r->shared_life_game)*r->shared_life_game=g->life_game;
        g->display_flags=previous_flags;
    } else if(entry==0x08013978) {
        SsaEnemyCdRuntime *cd=r->enemies[slot];SsaPlayerSpawn *player;
        SsaEnemyCdFrame frame=c->frame->enemies;
        SsaCombatCdOps adapters;
        if(!cd || !cd->active || !cd->game || !c->ops->enemies ||
           !c->ops->enemies->presentation || cd->walk_count!=&dispatch->walk_count ||
           (r->shared_cd_game && r->shared_cd_game->activation!=&s->game)) return -1;
        c->previous_cd_game=cd->game;
        if(r->shared_cd_game)cd->game=r->shared_cd_game;
        player=r->players[c->frame->target_player]->runtime->player;
        cd->game->player=&player->entity;cd->game->player_x=player->motion.x;cd->game->player_y=player->motion.y;
        mirror_cd(c);
        adapters=(SsaCombatCdOps){c->ops->enemies->combat,cd_presentation,c};
        if(r->enemy_speeds)frame.speed_factor=r->enemy_speeds[0];
        cd->converted_combat=r->nearest_converted;
        for(pass=0;pass<repeats;++pass){
            SsaEntity *target=NULL;int selected=0;
            if(s->actors[slot].activation.health>0)selected=converted_select(r,e,&cd->navigation,&target);
            if(selected<0){result=-1;break;}
            if(selected && !target){result=0;break;}
            if(selected){cd->game->player=target;cd->game->player_x=target->body.x;cd->game->player_y=target->body.y;}
            result=ssa_combat_cd_step(r->combat,cd,&frame,&r->queries->ops,&adapters);
            cd->game->player=&player->entity;cd->game->player_x=player->motion.x;cd->game->player_y=player->motion.y;
            if(result<0 || !cd->active || s->actors[slot].activation.health<=0 || (e->body.flags&0x20))break;
        }
        mirror_cd(c);cd->game=c->previous_cd_game;c->previous_cd_game=NULL;
        if(!cd->active)detach(r,slot);
    } else if(e->type>=0xce && e->type<=0xd8) {
        SsaEnemyControlRuntime *g=r->generic;
        SsaEnemyCdFrame frame=c->frame->enemies;
        SsaPlayerSpawn *p=r->players[c->frame->target_player]->runtime->player;
        if(!g || !r->generic_ops) return -1;
        g->game.player=&p->entity;g->game.player_x=p->motion.x;g->game.player_y=p->motion.y;
        if(r->enemy_speeds)frame.speed_factor=r->enemy_speeds[e->type-0xcd];
        g->converted_combat=r->nearest_converted;
        for(pass=0;pass<repeats;++pass){
            SsaEntity *target=NULL;int selected=0;
            if(g->active[slot] && s->actors[slot].activation.health>0)selected=converted_select(r,e,&g->navigation[slot],&target);
            if(selected<0){result=-1;break;}
            if(selected && !target){result=0;break;}
            if(selected){g->game.player=target;g->game.player_x=target->body.x;g->game.player_y=target->body.y;}
            result=ssa_enemy_control_runtime_step(g,slot,&frame,r->generic_ops);
            g->game.player=&p->entity;g->game.player_x=p->motion.x;g->game.player_y=p->motion.y;
            if(result<0 || !g->active[slot] || s->actors[slot].activation.health<=0 || (e->body.flags&0x20))break;
        }
    } else if(entry==0x0801b848) {
        SsaScriptActor *a=&s->actors[slot];
        if(!a->owned) return -1;
        result=ssa_entity_timer_step(&e->body.flags,&a->activation.timer,c->frame->enemies.ticks);
        a->script.flags=e->body.flags;a->script.timer=a->activation.timer;
    } else if(entry==0x08010d7c || entry==0x08010e5c || entry==0x08010ef0 || entry==0x080111c0 ||
              entry==0x08011350 || entry==0x08011090 || entry==0x08011424 ||
              entry==0x080113c4 || entry==0x08011120 || entry==0x080113e4 || entry==0x08014c58) {
        if(!r->projectiles->active[slot] || !c->ops->projectiles) return -1;
        result=ssa_projectile_runtime_step(r->projectiles,slot,c->frame->enemies.ticks,c->ops->projectiles);
    }
    dispatch->path_count=r->queries->path.segment_count;
    return result<0?-1:0;
}
int ssa_entity_runtime_step(SsaEntityRuntime *r,const SsaEntityRuntimeFrame *frame,const SsaEntityRuntimeOps *ops) {
    Call c={r,frame,ops,NULL};SsaEntityDispatchOps dispatch_ops={handler,&c};unsigned i;int result;
    if(!idle(r) || !frame || frame->enemies.ticks>255 || frame->target_player>=r->combat->player_count || !ops) return -1;
    for(i=0;i<r->combat->player_count;++i)
        if(frame->players[i].motion.ticks!=frame->enemies.ticks ||
           frame->players[i].motion.player_count!=r->combat->player_count || !ops->players[i]) return -1;
    r->updating=1;r->queries->path.segment_count=0;
    result=ssa_entity_dispatch_step(&r->dispatch,&dispatch_ops);
    r->combat->damage->source=r->dispatch.current;r->combat->damage->source_owner=NULL;
    r->updating=0;return result;
}
