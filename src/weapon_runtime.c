#include "ssa/weapon_runtime.h"
#include "combat_runtime_internal.h"
#include "projectile_runtime_internal.h"
#include <string.h>

static int bound(const SsaWeaponRuntime *r) {
    return r && r->player && r->combat && r->player->runtime &&
        r->player->runtime->player && r->combat->spawn && r->rules &&
        r->inventory_rules && r->animations && r->animations->loaded &&
        r->trace.world==r->player->runtime->world &&
        r->trace.world==r->combat->spawn->world &&
        r->controller.source==&r->player->runtime->player->entity &&
        r->controller.control==&r->player->weapon &&
        r->controller.inventory==&r->player->runtime->player->inventory &&
        r->controller.object==&r->object;
}
static int idle(const SsaWeaponRuntime *r) {
    return bound(r) && !r->updating && !r->controller.updating && !r->player->updating &&
        !r->player->runtime->updating && !r->combat->updating && !r->combat->spawn->updating &&
        !r->trace.world->query_active && !r->trace.updating;
}
static void sync(SsaWeaponRuntime *r) {
    SsaPlayerGameplay *g=r->player;
    g->runtime->player->motion.movement.flags=g->weapon.keys;
    g->runtime->player->motion.movement.weapon_flags=g->weapon.flags;
    g->weapon_action=(uint32_t)r->object.y;
    g->weapon_view.x=r->object.x;g->weapon_view.y=r->object.y;
    g->weapon_view.flags=r->object.type;g->weapon_view.kind=r->object.layer;
    g->weapon_view.player_number=r->object.owner;
}
int ssa_weapon_runtime_respawn(SsaWeaponRuntime *r,uint16_t animation,uint16_t period) {
    SsaRespawnWeaponView *v;
    if(!bound(r) || r->updating || r->controller.updating ||
       (animation>=SSA_WEAPON_ANIMATION_COUNT && animation!=0xffff)) return -1;
    v=&r->player->weapon_view;
    r->object.x=v->x;r->object.y=v->y;r->object.type=v->flags;
    r->object.layer=v->kind;r->object.owner=v->player_number;
    if(ssa_weapon_animation_set(&r->object,animation,period)!=1) return -1;
    r->automatic_remaining=r->automatic_elapsed=r->automatic_weapon=0;
    r->automatic_phase=0;r->automatic_damage_phase=0;
    r->fire_pending_weapon=0;r->switch_wait=0;
    r->time_strike_empty_pending=0;
    r->player->weapon_action=(uint32_t)r->object.y;return 0;
}
int ssa_weapon_runtime_init(SsaWeaponRuntime *r,SsaPlayerGameplay *p,SsaCombatRuntime *c,
    const SsaInventoryRules *inv,const SsaWeaponRules *rules,const SsaWeaponAnimations *animations) {
    unsigned i;
    if(!r || r->player || r->updating || !p || p->updating || !p->runtime ||
       p->runtime->updating || !p->runtime->player || !c || !c->spawn || c->updating ||
       c->spawn->updating || p->runtime->world!=c->spawn->world || !inv || !rules ||
       !animations || !animations->loaded || p->weapon.current<1 || p->weapon.current>10 ||
       p->weapon.requested<1 || p->weapon.requested>10) return -1;
    for(i=0;i<c->player_count && c->players[i]!=p->runtime;++i) {}
    if(i==c->player_count) return -1;
    if(ssa_weapon_aim_init(&r->trace,p->runtime->world,p->runtime->tables)<0) return -1;
    if(ssa_map_collision_init(&r->interaction,p->runtime->world->collision.map)!=1) {
        ssa_weapon_aim_free(&r->trace);return -1;
    }
    r->player=p;r->combat=c;r->inventory_rules=inv;r->rules=rules;r->animations=animations;
    r->controller.inventory=&p->runtime->player->inventory;r->controller.control=&p->weapon;
    r->controller.source=&p->runtime->player->entity;r->controller.object=&r->object;
    ssa_weapon_object_init(&r->object);
    return ssa_weapon_runtime_respawn(r,(uint16_t)(p->weapon.current-1),3);
}
int ssa_weapon_runtime_free(SsaWeaponRuntime *r) {
    if(!idle(r) || (r->projectiles && r->projectiles->combat)) return -1;
    if(ssa_weapon_aim_free(&r->trace)<0) return -1;
    ssa_map_collision_free(&r->interaction);memset(r,0,sizeof(*r));return 0;
}
static int starting_object(void *context,unsigned player,SsaWeaponObject **out) {
    SsaWeaponRuntime *const *runtimes=context;
    *out=&runtimes[player]->object;
    return ssa_weapon_object_init(*out)==1?0:-1;
}
int ssa_weapon_runtimes_start(SsaWeaponRuntime *const *runtimes,SsaPlayerGameplay *const *players,
    SsaCombatRuntime *combat,const SsaInventoryRules *inv,const SsaWeaponRules *rules,
    const SsaWeaponAnimations *animations,const SsaWeaponLoadouts *loadouts,unsigned setting,uint8_t god_mode) {
    SsaWeaponLoadout state={0};SsaWeaponLoadoutPlayer load[4];SsaWeaponControl controls[4];
    SsaWeaponLoadoutOps ops={starting_object,(void *)runtimes};unsigned i,j,count;int loaded;
    if(!runtimes || !players || !combat || !combat->spawn || !combat->spawn->world || combat->updating ||
       combat->spawn->updating || combat->spawn->world->query_active || !inv || !rules ||
       !animations || !animations->loaded || !loadouts || !loadouts->loaded ||
       setting>SSA_WEAPON_LOADOUT_ROWS || !combat->player_count || combat->player_count>4) return -1;
    count=combat->player_count;
    for(i=0;i<count;++i) {
        SsaWeaponRuntime *r=runtimes[i];SsaPlayerGameplay *g=players[i];
        if(!r || r->player || r->updating || r->controller.updating || r->projectiles ||
           !g || g->updating || !g->runtime || g->runtime!=combat->players[i] ||
           g->runtime->updating || !g->runtime->player ||
           g->runtime->world!=combat->spawn->world || g->runtime->player->entity.owner!=combat->spawn->world ||
           g->runtime->player->entity.player_index!=i) return -1;
        for(j=0;j<i;++j)if(runtimes[j]==r || players[j]==g) return -1;
    }
    for(i=0;i<count;++i) {
        SsaPlayerGameplay *g=players[i];
        controls[i]=g->weapon;controls[i].keys=g->runtime->player->motion.movement.flags;
        load[i]=(SsaWeaponLoadoutPlayer){&g->runtime->player->inventory,&controls[i],
            NULL,runtimes[i]->controller.cooldown};
    }
    loaded=ssa_weapon_loadout_initialize(&state,load,count,loadouts,inv,setting,god_mode,&ops);
    for(i=0;i<count;++i)if(load[i].object)players[i]->weapon=controls[i];
    if(loaded!=1) return -1;
    for(i=0;i<count;++i) if(load[i].object) {
        SsaPlayerGameplay *g=players[i];SsaWeaponObject object=*load[i].object;
        g->weapon_view.x=object.x;g->weapon_view.y=object.y;g->weapon_view.flags=object.type;
        g->weapon_view.kind=object.layer;g->weapon_view.player_number=object.owner;
        g->weapon_action=(uint32_t)object.y;
        g->runtime->player->motion.movement.weapon_flags=g->weapon.flags;
        if(ssa_weapon_runtime_init(runtimes[i],g,combat,inv,rules,animations)<0) return -1;
        runtimes[i]->object=object;sync(runtimes[i]);
    }
    return 0;
}
int ssa_weapon_runtime_bind_projectiles(SsaWeaponRuntime *r,SsaProjectileRuntime *p) {
    unsigned index;
    if(!idle(r) || r->projectiles || !p || p->updating || p->combat!=r->combat ||
       (!r->credited_kills && r->combat->player_count>1)) return -1;
    index=r->controller.source->player_index;
    if(index>=r->combat->player_count || p->hit_counts[index] || p->credited_kills[index]) return -1;
    p->hit_counts[index]=r->hits;p->credited_kills[index]=r->credited_kills;
    r->projectiles=p;return 0;
}
typedef struct WeaponCall {
    SsaWeaponRuntime *runtime;SsaWeaponGame *game;const SsaWeaponRuntimeOps *ops;
    SsaCombatScope *scope;
    unsigned clear_marker;
} WeaponCall;
static int32_t attack_range(const SsaWeaponRuntime *r,unsigned weapon,int32_t range) {
    if(weapon==1 && r->chainsaw_range && r->rules->range[0])
        return (int32_t)((int64_t)range*r->chainsaw_range/r->rules->range[0]);
    return range;
}
static int aim_result(WeaponCall *c,const SsaWeaponController *s,SsaWeaponAimResult *out) {
    if(c->runtime->time_strike_requires_target && s->control->current==9) {
        int16_t health=0;SsaEntity *e=out->target;
        if(!e || e->type<0xcd || e->type>0xd8 || (e->body.flags&0x21) ||
           ssa_combat_scope_health(c->scope,e,&health)<0 || health<=0) {
            out->target=NULL;out->marker=0x69696969;
        }
        if(c->runtime->time_strike_empty_pending){out->target=NULL;out->marker=0x69696969;}
    }
    c->clear_marker=1;return 0;
}
static int aim(void *context,const SsaWeaponController *s,uint16_t heading,int32_t range,
    SsaWeaponAimResult *out) {
    WeaponCall *c=context;SsaWeaponAim *a=&c->runtime->trace;
    SsaPlayerRuntime *player=c->runtime->player->runtime;
    int unlimited=c->runtime->unlimited_ranged && s->control->current!=1;
    range=attack_range(c->runtime,s->control->current,range);
    if(player->free_aim){
        SsaWeaponHit hit;int32_t sine,cosine;
        ssa_sincos(player->tables,(uint16_t)player->pitch,&sine,&cosine);
        out->slope=(int32_t)((int64_t)sine*65536/cosine);
        if((unlimited?ssa_weapon_hit_query_unlimited(a,s->source,heading,
            player->camera.eye_z-s->source->body.z,out->slope,&hit,0):
            ssa_weapon_hit_query_free(a,s->source,heading,player->camera.eye_z-s->source->body.z,
            range,out->slope,&hit))<0)return -1;
        out->target=hit.target;out->marker=hit.target?out->slope:0x69696969;
        out->fraction=INT32_MAX;
        if(hit.impact){int32_t hs,hc;int64_t horizontal;
            ssa_sincos(player->tables,heading,&hs,&hc);
            horizontal=((int64_t)hit.x-s->source->body.x)*hs+((int64_t)hit.y-s->source->body.y)*hc;
            horizontal/=65536;
            if(horizontal>=0 && horizontal<=range)out->fraction=(int32_t)(horizontal*65536/range);}
        return aim_result(c,s,out);
    }
    a->marker=out->marker;a->traversal.blocking_fraction=out->fraction;
    if(ssa_weapon_aim_query(a,s->source,0,0,0x600000,heading,range,&out->slope)<0) return -1;
    out->target=a->target;out->marker=a->marker;out->fraction=a->traversal.blocking_fraction;
    if(unlimited && !out->target){SsaWeaponHit hit;
        if(ssa_weapon_hit_query_unlimited(a,s->source,heading,0x600000,0,&hit,0)<0)return -1;
        out->target=hit.target;out->marker=hit.target?0:0x69696969;
        /* A distant level target must not be mistaken for a close muzzle
         * obstruction. Keep original fractions for actual nearby contacts. */
        if(hit.target)out->fraction=INT32_MAX;
    }
    return aim_result(c,s,out);
}
static int event(void *context,const SsaWeaponController *s,SsaWeaponEvent *e) {
    WeaponCall *c=context;SsaWeaponRuntime *r=c->runtime;
    if(c->clear_marker) {r->trace.marker=0;c->clear_marker=0;}
    sync(r);
    if(r->projectiles && e->kind==SSA_WEAPON_PROJECTILE_CREATE){
        int prion=e->args[0]==0x69 && r->projectiles->ballistic_player_prion;
        if(prion){
            e->args[2]=0;
            /* Original no-target autoaim probes can leave CREATE heading
             * offset by512 even at level pitch. Launch on the actual sight
             * heading, retaining the controller's selected aiming slope. */
            e->args[4]=s->heading;
        }
        if(r->player->runtime->free_aim || prion){
            int32_t sine,cosine;SsaPlayerRuntime *player=r->player->runtime;
            ssa_sincos(player->tables,(uint16_t)player->pitch,&sine,&cosine);
            e->args[1]=(int32_t)((int64_t)e->args[1]*cosine/65536);
            e->args[3]=(int32_t)((int64_t)player->camera.eye_z-s->source->body.z+(int64_t)e->args[1]*e->args[5]/65536);
        }
    }
    if(r->projectiles && (e->kind==SSA_WEAPON_PROJECTILE_CREATE ||
                         e->kind==SSA_WEAPON_PROJECTILE_COMMIT ||
                         e->kind==SSA_WEAPON_EFFECT_ALLOCATE ||
                         e->kind==SSA_WEAPON_EFFECT_ANIMATE ||
                         e->kind==SSA_WEAPON_EFFECT_INSERT))
        return ssa_projectile_runtime_weapon(r->projectiles,c->scope,s->source,e);
    if(e->kind==SSA_WEAPON_HITSCAN) {
        SsaWeaponHit hit;int16_t health;
        e->args[4]=attack_range(r,s->control->current,e->args[4]);
        if(r->unlimited_ranged && s->control->current!=1){
            int free_aim=r->player->runtime->free_aim;
            uint16_t heading=free_aim?(uint16_t)e->args[3]:(uint16_t)((unsigned)r->player->runtime->player->motion.facing<<8);
            int32_t z=free_aim?r->player->runtime->camera.eye_z-s->source->body.z:e->args[2];
            /* Preserve original nearby autoaim/impact arithmetic; extend a
             * clear finite ray, never a ray already stopped by an obstacle. */
            if((free_aim?ssa_weapon_hit_query_free(&r->trace,s->source,heading,z,e->args[4],e->args[5],&hit):
                ssa_weapon_hit_query(&r->trace,s->source,(uint8_t)(heading>>8),
                    e->args[0],e->args[1],z,e->args[4],e->args[5],&hit))<0)return -1;
            if(!hit.impact && ssa_weapon_hit_query_unlimited(&r->trace,s->source,heading,z,e->args[5],&hit,0)<0)return -1;
        }else if(r->player->runtime->free_aim){
            if(ssa_weapon_hit_query_free(&r->trace,s->source,(uint16_t)e->args[3],
                r->player->runtime->camera.eye_z-s->source->body.z,e->args[4],e->args[5],&hit)<0)return -1;
        }else if(ssa_weapon_hit_query(&r->trace,s->source,r->player->runtime->player->motion.facing,
            e->args[0],e->args[1],e->args[2],e->args[4],e->args[5],&hit)<0) return -1;
        if(!hit.impact) return 0;
        if(hit.target) {
            if(!e->args[6]) return 0;
            if(ssa_combat_scope_health(c->scope,hit.target,&health)<0) return -1;
            if(!health) return 0;
        }
        if(c->ops->impact(c->ops->context,r,&hit)<0) return -1;
        return hit.target ? ssa_combat_scope_damage(c->scope,hit.target,s->source,NULL,(uint32_t)e->args[6]) : 0;
    }
    if(e->kind==SSA_WEAPON_EXPLOSION)
        return ssa_combat_scope_explode(c->scope,s->source,s->source,NULL,
            (uint32_t)e->args[0],(uint32_t)e->args[1],e->args[2]);
    return c->ops->event(c->ops->context,r,e);
}
static unsigned automatic_ticks(SsaWeaponRuntime *r,const SsaWeaponGame *g) {
    SsaWeaponController *s=&r->controller;unsigned w=s->control->current,ticks=g->ticks;
    int clocked=w==4 || w==5 || (w==1 && r->clocked_chainsaw);
    int chainsaw=w==1 && r->clocked_chainsaw==SSA_CHAINSAW_GBA_CADENCE;
    uint64_t clock=(uint64_t)16777216*(w==5 && r->double_minigun?2u:1u);
    unsigned refreshes=chainsaw?SSA_CHAINSAW_GBA_REFRESHES:r->automatic_period;
    if(chainsaw)clock*=SSA_CHAINSAW_GBA_INTERVALS;
    if(w!=r->automatic_weapon || !(s->held&1) || !s->source_health ||
       !s->inventory->ammo[w-1] || !(s->control->flags&0x400)){
        r->automatic_remaining=r->automatic_elapsed=0;r->automatic_weapon=w;r->automatic_phase=0;
        r->automatic_damage_phase=0;
    }
    if(r->automatic_gba_clock && clocked && (s->held&1) && s->source_health &&
       s->inventory->ammo[w-1] && !(s->control->flags&0x400))
        r->automatic_remaining=(unsigned)(((uint64_t)refreshes*280896*60+clock-1)/clock);
    if(!clocked || !(s->control->flags&0x400) || !(s->held&1) ||
       !s->source_health || !s->inventory->ammo[w-1] || !ticks)return 0;
    r->automatic_elapsed+=ticks;
    if(r->automatic_gba_clock){
        uint64_t period=(uint64_t)refreshes*280896*60;
        r->automatic_phase+=(uint64_t)ticks*clock;
        if(r->automatic_phase<period){
            r->automatic_remaining=(unsigned)((period-r->automatic_phase+clock-1)/clock);return 0;
        }
        r->automatic_phase%=period;r->automatic_elapsed=0;
        r->automatic_remaining=(unsigned)((period-r->automatic_phase+clock-1)/clock);
        if(chainsaw){
            r->automatic_damage_phase+=SSA_CHAINSAW_GBA_REFRESHES;
            ticks=r->automatic_damage_phase/SSA_CHAINSAW_GBA_INTERVALS;
            r->automatic_damage_phase%=SSA_CHAINSAW_GBA_INTERVALS;return ticks;
        }
        return r->automatic_period;
    }
    if(r->automatic_remaining>ticks){r->automatic_remaining-=ticks;return 0;}
    ticks=r->automatic_elapsed;r->automatic_elapsed=0;
    r->automatic_remaining=r->automatic_period;
    return ticks;
}
static int run(void *context,SsaCombatScope *scope) {
    WeaponCall *c=context;SsaWeaponRuntime *r=c->runtime;
    SsaWeaponController *s=&r->controller;SsaWeaponControl *w=s->control;
    SsaCombatDamage *d=r->combat->damage;SsaWeaponControllerOps ops={aim,event,c};
    uint8_t *previous_kills=d->credited_kills;
    SsaPlayerSpawn *p=r->player->runtime->player;size_t i;int result=-1;
    unsigned pending,held=s->held,coast=0;uint32_t previous_shots,was_firing;
    c->scope=scope;
    d->source=s->source;d->credit_player=s->source->player_index+1;
    d->shot_hit_count=&r->hits[w->current-1];
    d->credited_kills=r->credited_kills;
    w->keys=p->motion.movement.flags;w->flags=p->motion.movement.weapon_flags;
    s->source_health=p->inventory.health;s->heading=p->motion.movement.heading;
    c->game->local_flags=r->combat->players[d->local_player]->player->motion.movement.flags;
    if(w->keys&4) {
        if(ssa_weapon_controller_cooldowns(s,c->game->ticks)<0) goto done;
        if(!(w->flags&0x2000)) {
            const SsaMapAssets *m=r->interaction.map;
            if(ssa_player_interaction_query(&r->interaction,s->source,p->motion.facing,r->trace.math)<0) goto done;
            for(i=0;i<r->interaction.special_count;++i) {
                const uint8_t *line=m->data+m->lines+r->interaction.special_lines[i]*40;
                if((line[36]|(unsigned)line[37]<<8)==2) w->flags|=0x100;
            }
        }
        if(ssa_weapon_update(s->inventory,w,r->inventory_rules,c->game->players)<0)goto done;
        pending=r->fire_pending_weapon;
        if(pending && (pending!=w->requested || !s->inventory->ammo[pending-1] ||
           !s->source_health || (c->game->local_flags&8)))pending=r->fire_pending_weapon=0;
        if(pending!=9 || !r->time_strike_requires_target)r->time_strike_empty_pending=0;
        previous_shots=pending?c->game->shots[pending-1]:0;
        if(pending){
            /* A real release still acknowledges the original held-fire gate.
             * Keep only this buffered attack held through start/cadence waits. */
            if(!(s->held&1) && c->game->motion_index==s->source->player_index)
                c->game->flags&=(uint16_t)~0x400u;
            if(w->current==pending)s->held|=1;
            else s->held&=(uint16_t)~1u;
        }
        held=s->held;
        was_firing=w->flags&0x400;
        if(r->finish_fire_before_switch && w->requested!=w->current && (w->flags&0x400)){
            if(r->rules->continuous[w->current-1]){
                if(!r->switch_wait){
                    unsigned animation=r->object.animation;
                    /* Loop completion is sticky. Wait for the next boundary,
                     * rather than accepting a marker from an earlier cycle. */
                    if(animation<SSA_WEAPON_ANIMATION_COUNT &&
                       r->animations->words[r->animations->offsets[animation]+r->animations->lengths[animation]-1]==0xffff)
                        r->object.done=0;
                }
                if(!r->object.done)coast=1;
                else s->held&=(uint16_t)~1u; /* Normal stop/wind-down at the boundary. */
            }
            r->switch_wait=1;
        }else r->switch_wait=0;
        if(coast){
            /* Animation still ticks in the later global animation stage.
             * No more bullets/damage while finishing a queued switch. */
        }else if(r->time_strike_requires_target && w->current==9){
            if(ssa_weapon_controller_step_targeted(s,c->game,r->rules,&ops,
                r->player->runtime->free_aim!=0)<0)goto done;
        }else if(r->automatic_period && (w->current!=1 || r->clocked_chainsaw)){
            uint32_t frame=c->game->frame;unsigned ticks=automatic_ticks(r,c->game);int fired;
            /* ROM muzzle flashes occur on every fourth original frame, not
             * every admitted attack and not every fourth PC update. */
            if(r->automatic_gba_clock && (w->current==4 || w->current==5))
                c->game->frame=(uint32_t)((uint64_t)frame*16777216*(w->current==5 && r->double_minigun?2u:1u)/((uint64_t)r->automatic_period*280896*60));
            fired=ssa_weapon_controller_step_clocked(s,c->game,r->rules,&ops,ticks,r->player->runtime->free_aim!=0);
            c->game->frame=frame;
            if(fired<0)goto done;
        }else if((r->player->runtime->free_aim?
            ssa_weapon_controller_step_free(s,c->game,r->rules,&ops):
            ssa_weapon_controller_step(s,c->game,r->rules,&ops))<0)goto done;
        s->held=held;
        /* Discrete Time Strike completes itself after start. Consume its
         * buffered click at that boundary, including a visual-only attempt. */
        if(pending==9 && r->time_strike_requires_target && w->current==9 && !was_firing && (w->flags&0x400)){
            r->fire_pending_weapon=0;r->time_strike_empty_pending=0;
        }else if(pending==9 && r->time_strike_requires_target && w->current==9 && !s->aim.target){
            /* Latch only after all original adjacent-aim retries complete. */
            r->time_strike_empty_pending=1;
        }
        if(pending && c->game->shots[pending-1]!=previous_shots)r->fire_pending_weapon=0;
        if(c->clear_marker) {r->trace.marker=0;c->clear_marker=0;}
        if(!(r->finish_fire_before_switch && w->requested!=w->current && (w->flags&0x400))){
            r->switch_wait=0;
            if(ssa_weapon_controller_switch(s,c->game->ticks)<0) goto done;
        }
    }
    result=0;
done:
    s->held=held;sync(r);d->credit_player=0;d->shot_hit_count=NULL;d->credited_kills=previous_kills;
    return result;
}
int ssa_weapon_runtime_step(SsaWeaponRuntime *r,SsaWeaponGame *game,uint16_t held,
    uint16_t pressed,const SsaWeaponRuntimeOps *ops) {
    WeaponCall c={r,game,ops,NULL,0};int result;
    if(!idle(r) || r->automatic_period>255 || r->automatic_gba_clock>1 || r->double_minigun>1 ||
       r->clocked_chainsaw>SSA_CHAINSAW_GBA_CADENCE || r->chainsaw_range>32767 ||
       (r->clocked_chainsaw==SSA_CHAINSAW_GBA_CADENCE && !r->automatic_gba_clock) ||
       (r->chainsaw_range && !r->rules->range[0]) || r->buffer_fire>1 || r->fire_pending_weapon>10 ||
       r->finish_fire_before_switch>1 || r->switch_wait>1 || r->time_strike_requires_target>1 || r->time_strike_empty_pending>1 ||
       !game || game->players!=r->combat->player_count || !ops ||
       !ops->combat || !ops->combat->event || !ops->event || !ops->impact ||
       r->player->weapon.current<1 || r->player->weapon.current>10 ||
       r->object.owner!=r->controller.source->player_index+1 ||
       !r->combat->damage || (!r->credited_kills && game->players>1) ||
       r->combat->damage->local_player>=r->combat->player_count) return -1;
    /* F150 runs before122AC: paused objects retain visibility; input-state3
     * objects still refresh it. Display-local index differs from motion index. */
    if(game->flags&1){r->fire_pending_weapon=r->time_strike_empty_pending=0;return 0;}
    if(r->combat->players[r->combat->damage->local_player]->player->motion.movement.flags&8)
        r->object.flags&=~1u;
    else r->object.flags|=1;
    if(r->player->life_game.input_state==3){r->fire_pending_weapon=r->time_strike_empty_pending=0;return 0;}
    if(!r->controller.inventory->health || !(r->player->runtime->player->motion.movement.flags&4))
        r->fire_pending_weapon=r->time_strike_empty_pending=0;
    else if(r->buffer_fire && (pressed&1) && r->player->weapon.requested>=1 && r->player->weapon.requested<=10)
        r->fire_pending_weapon=r->player->weapon.requested;
    r->updating=1;r->player->updating=1;
    r->controller.held=held;r->controller.pressed=pressed;
    result=ssa_combat_scope_run(r->combat,ops->combat,run,&c);
    r->player->updating=0;r->updating=0;return result;
}
int ssa_weapon_runtime_animate(SsaWeaponRuntime *r,uint16_t flags,unsigned ticks) {
    if(!idle(r)) return -1;
    return ssa_weapon_animation_step(&r->object,r->animations,flags,ticks)<0 ? -1 : 0;
}
