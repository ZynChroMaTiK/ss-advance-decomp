#include "ssa/game_runtime.h"
#include <string.h>

static int idle(const SsaGameRuntime *r) {
    SsaEntityRuntime *e=r?r->entities:NULL;
    SsaCombatRuntime *c=e?e->combat:NULL;
    SsaScriptSpawn *s=c?c->spawn:NULL;
    unsigned i,j;
    if(!r || r->updating || r->enemy_crowd_slide>1 || !r->frame || r->frame->updating || !r->frame->game ||
       !e || e->updating || e->dispatch.updating || !c || c->updating ||
       !c->damage || c->damage->updating || c->explosion.updating || !s || s->updating ||
       !s->world || s->world->query_active || !s->level || s->level->updating ||
       s->level->script.updating || s->level->movers.updating || s->level->switches.updating ||
       !e->queries || e->queries->updating || e->queries->path.updating || e->queries->ray.updating ||
       !e->projectiles || e->projectiles->updating || !r->weapon_game ||
       !r->sprites || r->sprites->updating || r->sprites->active_count>84 ||
       !r->sprite_animations || r->sprite_animations->loaded!=1 ||
       !r->animations || r->animations->updating || r->animations->spawn!=s ||
       !r->pickups || r->pickups->updating || r->pickups->combat!=c ||
       !c->player_count || c->player_count>4 || r->frame->local_index>=c->player_count ||
       s->reserved!=c->reserved_count || s->pool.active_count!=s->world->entity_count) return 0;
    for(i=0;i<c->player_count;++i) {
        SsaPlayerGameplay *g=e->players[i];SsaWeaponRuntime *w=r->weapons[i];
        if(!g || g->updating || !g->runtime || g->runtime->updating ||
           g->runtime!=c->players[i] || !g->runtime->player ||
           r->frame->player_flags[i]!=&g->runtime->player->motion.movement.flags ||
           (w && (w->updating || w->controller.updating || w->trace.updating ||
                  w->player!=g || w->combat!=c || w->projectiles!=e->projectiles ||
                  w->animations!=r->sprite_animations))) return 0;
    }
    for(i=0;i<100;++i) if(e->enemies[i] && e->enemies[i]->updating) return 0;
    for(i=0;i<84;++i) {
        if(!r->sprites->objects[i]) return 0;
        for(j=0;j<i;++j) if(r->sprites->objects[i]==r->sprites->objects[j]) return 0;
    }
    for(i=0;i<c->player_count;++i) if(r->weapons[i]) {
        for(j=0;j<84 && r->sprites->objects[j]!=&r->weapons[i]->object;++j) {}
        if(j==84) return 0;
    }
    return 1;
}
int ssa_game_runtime_init(SsaGameRuntime *r,SsaFrame *f,SsaEntityRuntime *e,
    SsaActorAnimationRuntime *a,SsaPickupRespawnRuntime *p,
    SsaSpriteDispatch *sprites,const SsaWeaponAnimations *sprite_animations,
    SsaWeaponRuntime *const *weapons,SsaWeaponGame *wg) {
    unsigned i;const uint16_t *saved[4];
    if(!r || r->frame || r->updating || !f || f->updating || !e || !e->combat ||
       !e->combat->player_count || e->combat->player_count>4 || !weapons) return -1;
    for(i=0;i<4;++i)saved[i]=f->player_flags[i];
    memset(r,0,sizeof(*r));r->frame=f;r->entities=e;r->animations=a;r->pickups=p;r->weapon_game=wg;
    r->sprites=sprites;r->sprite_animations=sprite_animations;
    for(i=0;i<e->combat->player_count;++i) {
        SsaPlayerGameplay *g=e->players[i];
        if(!g || !g->runtime || !g->runtime->player) goto invalid;
        r->weapons[i]=weapons[i];f->player_flags[i]=&g->runtime->player->motion.movement.flags;
    }
    if(!idle(r)) goto invalid;
    r->cd_game.activation=&e->combat->spawn->game;
    r->cd_game.player=&e->players[0]->runtime->player->entity;
    if(e->shared_cd_game)r->cd_game=*e->shared_cd_game;
    else for(i=0;i<100;++i) if(e->enemies[i] && e->enemies[i]->game &&
        e->enemies[i]->game->loop_source) {
        if(r->cd_game.loop_source && r->cd_game.loop_source!=e->enemies[i]->game->loop_source) goto invalid;
        r->cd_game=*e->enemies[i]->game;
    }
    if(r->cd_game.activation!=&e->combat->spawn->game || !r->cd_game.player) goto invalid;
    r->life_game=e->shared_life_game?*e->shared_life_game:e->players[0]->life_game;
    r->previous_cd_game=e->shared_cd_game;r->previous_life_game=e->shared_life_game;
    e->shared_cd_game=&r->cd_game;e->shared_life_game=&r->life_game;
    return 0;
invalid:
    for(i=0;i<4;++i)f->player_flags[i]=saved[i];
    memset(r,0,sizeof(*r));return -1;
}
int ssa_game_runtime_free(SsaGameRuntime *r) {
    unsigned i;
    if(!idle(r) || r->entities->shared_cd_game!=&r->cd_game ||
       r->entities->shared_life_game!=&r->life_game) return -1;
    for(i=0;i<100;++i) if(r->entities->enemies[i]==&r->owned_cd[i])
        if(ssa_entity_runtime_unbind_cd(r->entities,i)<0) return -1;
    if(r->previous_cd_game)*r->previous_cd_game=r->cd_game;
    if(r->previous_life_game)*r->previous_life_game=r->life_game;
    r->entities->shared_cd_game=r->previous_cd_game;
    r->entities->shared_life_game=r->previous_life_game;
    memset(r,0,sizeof(*r));return 0;
}
static int bind_spawns(SsaGameRuntime *r) {
    SsaEntityRuntime *e=r->entities;SsaScriptSpawn *s=e->combat->spawn;
    unsigned i,slot;
    for(i=0;i<s->pool.active_count;++i) {
        slot=s->pool.active_slots[i];
        if(slot>=100) return -1;
        if(!s->actors[slot].owned || s->actors[slot].entity.type!=0xcd || e->enemies[slot]) continue;
        if(ssa_enemy_cd_runtime_init(&r->owned_cd[slot],s,slot,e->queries->math,
            &r->cd_game,&r->walk_count)<0) return -1;
        r->owned_cd[slot].crowd_slide=r->enemy_crowd_slide;
        if(ssa_entity_runtime_bind_cd(e,&r->owned_cd[slot])<0) {
            memset(&r->owned_cd[slot],0,sizeof(r->owned_cd[slot]));return -1;
        }
    }
    return 0;
}
static void shared(SsaGameRuntime *r) {
    SsaFrame *f=r->frame;SsaCombatRuntime *c=r->entities->combat;SsaWeaponGame *g=r->weapon_game;
    g->frame=f->game->counter;g->flags=f->game->flags;g->ticks=f->game->ticks;
    g->players=(uint8_t)c->player_count;
    c->damage->frame_counter=f->game->counter;c->damage->local_player=f->local_index;
}
typedef struct Call {SsaGameRuntime *r;const SsaGameRuntimeOps *ops;} Call;
static int sprite_handler(void *context,SsaSpriteDispatch *d,uint32_t entry,unsigned slot) {
    Call *call=context;SsaGameRuntime *r=call->r;unsigned i;
    if(d!=r->sprites || slot>=84) return -1;
    if(entry==0x0800f150) {
        for(i=0;i<r->entities->combat->player_count;++i)
            if(r->weapons[i] && d->objects[slot]==&r->weapons[i]->object) {
                if(!r->frame->game || r->frame->local_index>=r->entities->combat->player_count) return -1;
                shared(r);
                r->weapons[i]->player->life_game.input_state=r->life_game.input_state;
                return ssa_weapon_runtime_step(r->weapons[i],r->weapon_game,
                    r->input.held[i],r->input.pressed[i],call->ops->weapons[i]);
            }
        return -1;
    }
    return call->ops->sprites && call->ops->sprites->handler ?
        call->ops->sprites->handler(call->ops->sprites->context,d,entry,slot) : -1;
}
static int sprite_submit(void *context,SsaSpriteDispatch *d,SsaWeaponObject *object) {
    Call *call=context;
    return call->ops->sprites && call->ops->sprites->submit ?
        call->ops->sprites->submit(call->ops->sprites->context,d,object) : 0;
}
static int stage(void *context,SsaFrame *f,SsaFrameStage which) {
    Call *call=context;SsaGameRuntime *r=call->r;const SsaGameRuntimeOps *ops=call->ops;
    SsaCombatRuntime *combat=r->entities->combat;SsaScriptSpawn *spawn=combat->spawn;
    unsigned i;
    if(f!=r->frame || !f->game) return -1;
    r->stage=which;
    if(ops->before && ops->before(ops->context,r,which)<0) return -1;
    switch(which) {
    case SSA_FRAME_ENTITIES:
        if(f->local_index>=combat->player_count || bind_spawns(r)<0) return -1;
        shared(r);
        r->input.entities.enemies.ticks=f->game->ticks;
        r->input.entities.enemies.frame_counter=f->game->counter;
        for(i=0;i<combat->player_count;++i) {
            SsaPlayerGameplay *p=r->entities->players[i];
            r->input.entities.players[i].motion.ticks=f->game->ticks;
            r->input.entities.players[i].motion.player_count=(uint8_t)combat->player_count;
            r->input.entities.players[i].local_player=(uint8_t)f->local_index;
            p->hazard_game.frame_counter=f->game->counter;
            p->hazard_game.level_counter=r->input.entities.enemies.level_counter;
            p->hazard_game.local_flags=*f->player_flags[f->local_index];
        }
        return ssa_entity_runtime_step(r->entities,&r->input.entities,ops->entities);
    case SSA_FRAME_MOVERS:
        return ssa_level_runtime_movers_step(spawn->level,f->game->ticks,ops->movers);
    case SSA_FRAME_SCRIPTS:
        {
            int result;
            /* VM opcode9 and player death share input030042F0+14 (03004304). */
            spawn->level->script.game_state=r->life_game.input_state;
            result=ssa_script_spawn_scripts_step(spawn,f->game->ticks,r->input.script_fallback_z,
                ops->scripts,ops->movers);
            r->life_game.input_state=spawn->level->script.game_state;
            return result;
        }
    case SSA_FRAME_PICKUP_RESPAWN:
        return ssa_pickup_respawn_runtime_step(r->pickups,f->game->ticks,
            r->input.pickup_fallback_z,ops->pickups);
    case SSA_FRAME_ENTITY_ANIMATION:
        return ssa_actor_animation_runtime_step(r->animations,f->game->ticks);
    case SSA_FRAME_SPRITE_CONTROLLERS:
        {
            SsaSpriteDispatchOps adapters={sprite_handler,sprite_submit,call};
            if(f->local_index>=combat->player_count) return -1;
            shared(r);
            return ssa_sprite_dispatch_step(r->sprites,&adapters);
        }
    case SSA_FRAME_SPRITE_ANIMATION:
        for(i=0;i<r->sprites->active_count;++i) {
            unsigned slot=r->sprites->active[i],player;
            if(slot>=84 || !r->sprites->objects[slot])return -1;
            for(player=0;player<combat->player_count;++player)
                if(r->weapons[player] && r->weapons[player]->alternate_shotgun && r->weapons[player]->shotgun_animating &&
                   r->sprites->objects[slot]==&r->weapons[player]->object)break;
            if(player<combat->player_count){
                if(ssa_weapon_runtime_animate(r->weapons[player],f->game->flags,f->game->ticks)<0)return -1;
            }else if(ssa_weapon_animation_step(r->sprites->objects[slot],r->sprite_animations,
                        f->game->flags,f->game->ticks)<0)return -1;
        }
        return 0;
    default:
        {
            int result;
            r->external_active=1;
            result=ops->external(ops->context,r,which);
            r->external_active=0;
            return result;
        }
    }
}
int ssa_game_runtime_step(SsaGameRuntime *r,const SsaGameRuntimeInput *input,const SsaGameRuntimeOps *ops) {
    Call call={r,ops};SsaFrameOps adapters={stage,&call};int result;
    if(!idle(r) || r->entities->shared_cd_game!=&r->cd_game ||
       r->entities->shared_life_game!=&r->life_game || !input || !ops || !ops->entities || !ops->scripts ||
       !ops->scripts->particle || !ops->pickups || !ops->pickups->particle || !ops->external) return -1;
    r->input=*input;++r->step_serial;r->updating=1;
    result=ssa_frame_step(r->frame,&adapters);
    r->stage=(SsaFrameStage)0;r->updating=0;return result;
}
