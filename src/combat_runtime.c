#include "ssa/combat_runtime.h"
#include "combat_runtime_internal.h"
#include <string.h>

static int ready(const SsaCombatRuntime *r) {
    const SsaScriptSpawn *s=r ? r->spawn : NULL;
    unsigned i;
    if (!r || r->updating || r->explosion.updating || !s || !s->world || !s->level ||
        !s->level->map || s->updating || !s->world->cells || !s->world->sectors ||
        s->world->query_active || s->level->updating || s->level->script.updating ||
        s->level->movers.updating || s->level->switches.updating ||
        s->world->collision.map!=s->level->map || r->reserved_count!=s->reserved ||
        s->reserved>100 || !r->damage || r->damage->updating || !r->score ||
        !r->player_count || r->player_count>4 || r->damage->player_count!=r->player_count ||
        r->damage->local_player>=r->player_count) return 0;
    for (i=0;i<r->player_count;++i) {
        const SsaPlayerRuntime *p=r->players[i];
        if (!p || p->updating || !p->player || !p->player->active ||
            p->world!=s->world || p->player->entity.owner!=s->world ||
            p->player->entity.player_index!=i || !(p->player->entity.body.flags&0x10)) return 0;
    }
    return 1;
}
static int ordered(SsaCombatRuntime *r,SsaEntity **out) {
    SsaScriptSpawn *s=r->spawn;
    unsigned i,j,slot,n=s->pool.active_count;
    if (n>100 || n!=s->world->entity_count) return -1;
    for (i=0;i<n;++i) {
        slot=s->pool.active_slots[i];
        if (slot>=100 || !s->pool.occupied[slot] || s->pool.positions[slot]!=i) return -1;
        out[i]=ssa_script_spawn_entity(s,r->reserved,slot);
        if (!out[i] || out[i]->owner!=s->world) return -1;
        for (j=0;j<i;++j) if (out[j]==out[i]) return -1;
    }
    return 0;
}
static int project(SsaCombatRuntime *r,SsaEntity *e,SsaCombatDamageTarget *v) {
    unsigned i;
    if (!e || e->owner!=r->spawn->world) return -1;
    for (i=0;i<r->player_count;++i) {
        SsaPlayerRuntime *p=r->players[i];
        if (e==&p->player->entity) {
            *v=(SsaCombatDamageTarget){e,&p->player->inventory.health,&p->frame.timer,&p->player->inventory};
            return 0;
        }
    }
    for (i=0;i<100;++i) {
        SsaScriptActor *a=&r->spawn->actors[i];
        if (e==&a->entity && a->owned && r->spawn->pool.occupied[i] && !(e->body.flags&0x10)) {
            *v=(SsaCombatDamageTarget){e,&a->activation.health,&a->activation.timer,NULL};
            return 0;
        }
    }
    return -1;
}
static void synchronize(SsaCombatRuntime *r) {
    unsigned i;
    for (i=0;i<r->player_count;++i)
        r->players[i]->player->entity_health=r->players[i]->player->inventory.health;
    for (i=0;i<100;++i) if (r->spawn->actors[i].owned) {
        SsaScriptActor *a=&r->spawn->actors[i];
        a->script.flags=a->entity.body.flags; a->script.timer=a->activation.timer;
    }
}
static void lock(SsaCombatRuntime *r) {
    unsigned i; r->updating=1;
    for (i=0;i<r->player_count;++i) r->players[i]->updating=1;
}
static void unlock(SsaCombatRuntime *r) {
    unsigned i; synchronize(r);
    for (i=0;i<r->player_count;++i) r->players[i]->updating=0;
    r->updating=0;
}
typedef struct DamageCall {
    SsaCombatRuntime *runtime;
    const SsaCombatRuntimeOps *ops;
    const SsaEntity *source,*owner;
} DamageCall;
static int effect(void *context,const SsaCombatDamageTarget *v,SsaCombatDamageEvent kind,
                    uint32_t a,uint32_t b) {
    DamageCall *c=context;
    synchronize(c->runtime);
    if (kind==SSA_COMBAT_DAMAGE_REWARD) return ssa_combat_reward(v->entity->type,c->runtime->score);
    return c->ops->event(c->ops->context,c->runtime,v,kind,a,b);
}
static int damage(void *context,SsaEntity *e,uint32_t amount) {
    DamageCall *c=context;
    SsaCombatRuntime *r=c->runtime;
    SsaCombatDamage *g=r->damage;
    SsaCombatDamageTarget v;
    SsaCombatDamageOps ops={effect,c};
    const SsaEntity *old_source=g->source,*old_owner=g->source_owner;
    uint16_t old_flags=g->local_flags;
    int result;
    if (e && project(r,e,&v)) return -1;
    if (e==c->owner && ssa_combat_boss_projectile(r,c->source,c->owner)) return 0;
    g->source=c->source; g->source_owner=c->owner;
    g->local_flags=r->players[g->local_player]->player->motion.movement.flags;
    result=ssa_combat_damage(g,e ? &v : NULL,amount,&ops);
    g->source=old_source; g->source_owner=old_owner; g->local_flags=old_flags;
    synchronize(r); return result;
}
static int explode(DamageCall *c,SsaEntity *origin,uint32_t radius,uint32_t amount,int players) {
    SsaEntity *entities[100];
    SsaCombatRuntime *r=c->runtime;
    SsaCombatExplosionOps ops={damage,c};
    if (!origin || origin->owner!=r->spawn->world || ordered(r,entities)) return -1;
    return ssa_combat_explode(&r->explosion,r->spawn->level->map,origin,entities,
        r->spawn->pool.active_count,r->damage->player_count,radius,amount,players,&ops);
}
int ssa_combat_runtime_init(SsaCombatRuntime *r,SsaScriptSpawn *s,
    SsaEntity *const *reserved,size_t count,SsaPlayerRuntime *const *players,
    unsigned player_count,SsaCombatDamage *g,uint32_t *score) {
    SsaEntity *entities[100];
    unsigned i,j;
    if (!r || r->spawn || r->updating || r->explosion.updating || !s || count>100 ||
        count!=s->reserved || (count && !reserved) || !players || !player_count || player_count>4) return -1;
    memset(r,0,sizeof(*r));r->spawn=s;r->damage=g;r->score=score;
    r->reserved_count=(unsigned)count;r->player_count=player_count;
    for (i=0;i<count;++i) r->reserved[i]=reserved[i];
    for (i=0;i<player_count;++i) r->players[i]=players[i];
    if (!ready(r) || ordered(r,entities)) goto fail;
    for (i=0;i<player_count;++i) {
        for (j=0;j<count && reserved[j]!=&players[i]->player->entity;++j) {}
        if (j==count) goto fail;
    }
    return 0;
fail:
    memset(r,0,sizeof(*r));return -1;
}
int ssa_combat_runtime_free(SsaCombatRuntime *r) {
    if (!r || (r->spawn && !ready(r)) || r->updating) return -1;
    memset(r,0,sizeof(*r));return 0;
}
int ssa_combat_runtime_damage(SsaCombatRuntime *r,SsaEntity *target,
    const SsaEntity *source,const SsaEntity *owner,uint32_t amount,const SsaCombatRuntimeOps *ops) {
    DamageCall c={r,ops,source,owner};
    int result;
    if (!ready(r) || !ops || !ops->event) return -1;
    lock(r);r->spawn->updating=1;
    result=damage(&c,target,amount);
    r->spawn->updating=0;unlock(r);return result;
}
int ssa_combat_runtime_explode(SsaCombatRuntime *r,SsaEntity *origin,
    const SsaEntity *source,const SsaEntity *owner,uint32_t radius,uint32_t amount,int players,
    const SsaCombatRuntimeOps *ops) {
    DamageCall c={r,ops,source,owner};
    int result;
    if (!ready(r) || !ops || !ops->event) return -1;
    lock(r);r->spawn->updating=1;
    result=explode(&c,origin,radius,amount,players);
    r->spawn->updating=0;unlock(r);return result;
}
typedef struct CdCall { SsaCombatRuntime *runtime; const SsaCombatCdOps *ops; } CdCall;
static int cd_event(void *context,const SsaEnemyCd *cd,SsaEnemyCdEvent kind,SsaEntity *other,
                    int32_t a,int32_t b,int32_t d) {
    CdCall *c=context;
    SsaEntity *source=cd->activation->entity;
    DamageCall hit={c->runtime,c->ops->combat,source,NULL};
    if (kind==SSA_ENEMY_CD_CONTACT_DAMAGE) return damage(&hit,other,(uint32_t)a);
    if (kind==SSA_ENEMY_CD_EXPLOSION) return explode(&hit,source,(uint32_t)a,(uint32_t)b,d);
    return c->ops->presentation(c->ops->context,cd,kind,other,a,b,d);
}
int ssa_combat_cd_step(SsaCombatRuntime *r,SsaEnemyCdRuntime *cd,const SsaEnemyCdFrame *frame,
    const SsaEnemyNavigationOps *queries,const SsaCombatCdOps *ops) {
    CdCall c={r,ops};
    SsaEnemyCdRuntimeOps bridge={queries,cd_event,&c};
    SsaEnemyCdFrame adjusted;
    int result;
    if (!ready(r) || !cd || cd->spawn!=r->spawn || !ops || !ops->combat ||
        !ops->combat->event || !ops->presentation) return -1;
    lock(r);
    if(r->prion_balance && frame && cd->navigation.entity && (cd->navigation.entity->body.flags&0x400) &&
       cd->navigation.target && cd->navigation.target->type>=0xcd && cd->navigation.target->type<=0xd8 &&
       !(cd->navigation.target->body.flags&(0x10|0x20))) {
        adjusted=*frame;adjusted.speed_factor/=2;frame=&adjusted;
    }
    result=ssa_enemy_cd_runtime_step(cd,frame,&bridge);
    unlock(r);return result;
}

struct SsaCombatScope { SsaCombatRuntime *runtime; const SsaCombatRuntimeOps *ops; };
int ssa_combat_scope_run(SsaCombatRuntime *r,const SsaCombatRuntimeOps *ops,
    int (*run)(void *,SsaCombatScope *),void *context) {
    SsaCombatScope scope={r,ops};int result;
    if(!ready(r) || !ops || !ops->event || !run) return -1;
    lock(r);r->spawn->updating=1;
    result=run(context,&scope);
    r->spawn->updating=0;unlock(r);return result;
}
static int scope_ready(const SsaCombatScope *s) {
    return s && s->runtime && s->runtime->updating && s->runtime->spawn->updating &&
        !s->runtime->spawn->world->query_active && !s->runtime->damage->updating &&
        !s->runtime->explosion.updating;
}
int ssa_combat_scope_health(SsaCombatScope *s,SsaEntity *e,int16_t *health) {
    SsaCombatDamageTarget v;
    if(!scope_ready(s) || !health || project(s->runtime,e,&v)) return -1;
    *health=*v.health;return 0;
}
int ssa_combat_scope_damage(SsaCombatScope *s,SsaEntity *e,const SsaEntity *source,
    const SsaEntity *owner,uint32_t amount) {
    DamageCall c;
    if(!scope_ready(s)) return -1;
    c=(DamageCall){s->runtime,s->ops,source,owner};return damage(&c,e,amount);
}
int ssa_combat_scope_melee(SsaCombatScope *s,SsaEntity *e,const SsaEntity *source,uint32_t amount) {
    return ssa_combat_scope_damage(s,e,source,NULL,amount);
}
int ssa_combat_scope_explode(SsaCombatScope *s,SsaEntity *e,const SsaEntity *source,
    const SsaEntity *owner,uint32_t radius,uint32_t amount,int players) {
    DamageCall c;
    if(!scope_ready(s)) return -1;
    c=(DamageCall){s->runtime,s->ops,source,owner};return explode(&c,e,radius,amount,players);
}
