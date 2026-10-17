#include "ssa/pickup_respawn_runtime.h"
#include <string.h>
static int idle(const SsaPickupRespawnRuntime *r) {
    SsaCombatRuntime *c=r?r->combat:NULL;SsaScriptSpawn *s=c?c->spawn:NULL;unsigned i;
    if(!r || !c || !s || !s->world || !s->level || !r->plan || !r->data || !r->data->loaded ||
       r->updating || r->state.updating || c->updating || !c->damage || c->damage->updating ||
       c->explosion.updating || s->updating || s->world->query_active || s->level->updating ||
       s->level->script.updating || s->level->movers.updating || s->level->switches.updating ||
       r->plan->map!=s->world->collision.map || r->plan->map!=s->level->map ||
       !c->player_count || c->player_count>4 || c->reserved_count!=s->reserved || s->reserved>100 ||
       s->pool.active_count>100 || s->pool.active_count!=s->world->entity_count) return 0;
    for(i=0;i<c->player_count;++i)if(!c->players[i] || c->players[i]->updating) return 0;
    return 1;
}
int ssa_pickup_respawn_runtime_init(SsaPickupRespawnRuntime *r,SsaCombatRuntime *c,
    const SsaSpawnPlan *plan,const SsaPickupRespawnData *data) {
    if(!r || r->combat || r->updating || !c || !plan || !data)return -1;
    r->combat=c;r->plan=plan;r->data=data;r->pending_slot=100;
    if(!idle(r)) {memset(r,0,sizeof(*r));return -1;}return 0;
}
int ssa_pickup_respawn_runtime_free(SsaPickupRespawnRuntime *r) {
    if(!idle(r))return -1;
    memset(r,0,sizeof(*r));return 0;
}
typedef struct Call {SsaPickupRespawnRuntime *r;const SsaPickupRespawnRuntimeOps *ops;} Call;
static int occupied(void *context,const SsaSpawnMarker *marker,int *out) {
    Call *c=context;SsaCombatRuntime *combat=c->r->combat;SsaScriptSpawn *s=combat->spawn;
    SsaEntity *entities[100];unsigned i,slot;int result;
    for(i=0;i<s->pool.active_count;++i) {
        slot=s->pool.active_slots[i];if(slot>=100 || !s->pool.occupied[slot])return -1;
        entities[i]=ssa_script_spawn_entity(s,combat->reserved,slot);
        if(!entities[i] || entities[i]->owner!=s->world)return -1;
    }
    result=ssa_pickup_respawn_occupied(marker,entities,s->pool.active_count);
    if(result<0)return -1;
    *out=result;return 0;
}
static int allocate(void *context,SsaLevelScriptEntity **out) {
    Call *c=context;SsaScriptSpawn *s=c->r->combat->spawn;SsaScriptActor *a;unsigned slot;int result;
    *out=NULL;if(s->world->entity_count>=s->world->capacity)return 0;
    result=ssa_entity_pool_allocate(&s->pool,&slot);if(result!=1)return result<0?-1:0;
    if(s->actors[slot].owned)return -1;
    a=&s->actors[slot];memset(a,0,sizeof(*a));a->owned=1;a->activation.entity=&a->entity;
    c->r->pending_slot=slot;*out=&a->script;return 0;
}
static SsaScriptActor *project(Call *c,SsaLevelScriptEntity *e) {
    SsaScriptActor *a;SsaScriptSpawn *s=c->r->combat->spawn;
    if(c->r->pending_slot>=100)return NULL;
    a=&s->actors[c->r->pending_slot];
    if(!a->owned || e!=&a->script)return NULL;
    a->entity.body.x=e->x;a->entity.body.y=e->y;a->entity.body.z=e->z;
    a->entity.type=e->type;a->entity.body.flags=e->flags;
    a->activation.timer=e->timer;a->activation.state=e->variant;a->activation.facing=e->facing;return a;
}
static int animation(void *context,SsaLevelScriptEntity *e,unsigned id,unsigned ticks) {
    Call *c=context;SsaScriptActor *a=project(c,e);if(!a)return -1;
    if(id<=0x8d || id==0xffff) {
        a->animation=(uint16_t)id;a->animation_ticks=(uint8_t)ticks;
        a->animation_frame=a->animation_counter=a->animation_flags=0;
    }
    return 0;
}
static int insert(void *context,SsaLevelScriptEntity *e) {
    Call *c=context;SsaScriptActor *a=project(c,e);
    if(!a || !ssa_world_insert(c->r->combat->spawn->world,&a->entity))return -1;
    e->floor=(uint16_t)a->entity.body.floor;return 0;
}
static int particle(void *context,unsigned id,int32_t x,int32_t y,int32_t z) {
    Call *c=context;
    if(c->r->pending_slot<100) {
        SsaScriptActor *a=&c->r->combat->spawn->actors[c->r->pending_slot];a->entity.body.z=a->script.z;
    }
    return c->ops->particle(c->ops->context,c->r,id,x,y,z);
}
int ssa_pickup_respawn_runtime_step(SsaPickupRespawnRuntime *r,unsigned ticks,int32_t fallback,
    const SsaPickupRespawnRuntimeOps *ops) {
    Call c={r,ops};SsaPickupRespawnOps adapters={occupied,allocate,animation,insert,particle,&c};
    unsigned i;int result;SsaCombatRuntime *combat;
    if(!idle(r) || ticks>255 || !ops || !ops->particle)return -1;
    combat=r->combat;r->updating=1;r->pending_slot=100;combat->updating=1;combat->spawn->updating=1;
    for(i=0;i<combat->player_count;++i)combat->players[i]->updating=1;
    result=ssa_pickup_respawn_step(&r->state,r->data,combat->player_count,combat->spawn->level->level,ticks,
        r->plan->teleport_starts,r->plan->teleport_count,fallback,&adapters);
    for(i=0;i<combat->player_count;++i)combat->players[i]->updating=0;
    combat->spawn->updating=0;combat->updating=0;r->updating=0;return result;
}
