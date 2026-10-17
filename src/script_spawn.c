#include "ssa/script_spawn.h"
#include <string.h>

static int ready(const SsaScriptSpawn *s)
{
    return s && s->level && s->level->map && s->world &&
        s->world->cells && s->world->sectors && !s->updating &&
        !s->level->updating && !s->level->script.updating &&
        !s->level->movers.updating && !s->level->switches.updating &&
        !s->world->query_active && s->world->collision.map == s->level->map;
}

static SsaScriptActor *actor(SsaScriptSpawn *s, unsigned slot)
{
    if (slot >= SSA_ENTITY_POOL_CAPACITY || !s->pool.occupied[slot] ||
        !s->actors[slot].owned || s->actors[slot].activation.updating) return NULL;
    return &s->actors[slot];
}

static void animation_set(SsaScriptActor *a, unsigned id, unsigned ticks)
{
    id = (uint16_t)id;
    if (id <= 0x8d || id == 0xffff) {
        a->animation = (uint16_t)id;
        a->animation_ticks = (uint8_t)ticks;
        a->animation_frame = a->animation_counter = a->animation_flags = 0;
    }
}

/* Only used before insertion: registered X/Y must be changed through world. */
static void project(SsaScriptActor *a)
{
    a->entity.body.x = a->script.x;
    a->entity.body.y = a->script.y;
    a->entity.body.z = a->script.z;
    a->entity.body.flags = a->script.flags;
    a->entity.type = a->script.type;
    a->activation.entity = &a->entity;
    a->activation.timer = a->script.timer;
    a->activation.state = a->script.variant;
    a->activation.facing = a->script.facing;
}

int ssa_script_spawn_init(SsaScriptSpawn *s, SsaLevelRuntime *l, SsaWorld *w)
{
    unsigned i, slot;
    if (!s || s->level || s->world || s->updating || !l || !w || !l->map ||
        !w->cells || !w->sectors || w->collision.map != l->map ||
        w->entity_count > SSA_ENTITY_POOL_CAPACITY || l->updating ||
        l->script.updating || l->movers.updating || l->switches.updating ||
        w->query_active) return -1;
    memset(s, 0, sizeof(*s));
    s->level = l;
    s->world = w;
    s->pending_slot = SSA_ENTITY_POOL_CAPACITY;
    ssa_entity_pool_init(&s->pool);
    s->reserved = (unsigned)w->entity_count;
    for (i = 0; i < s->reserved; ++i) ssa_entity_pool_allocate(&s->pool, &slot);
    return 0;
}

SsaEntity *ssa_script_spawn_entity(SsaScriptSpawn *s,SsaEntity *const *reserved,unsigned slot)
{
    if(!s || slot>=SSA_ENTITY_POOL_CAPACITY || !s->pool.occupied[slot]) return NULL;
    if(s->actors[slot].owned) return &s->actors[slot].entity;
    return slot<s->reserved && reserved ? reserved[slot] : NULL;
}

int ssa_script_spawn_adopt_plan(SsaScriptSpawn *s,SsaSpawnPlan *plan)
{
    size_t i;
    if(!ready(s) || !plan || plan->map!=s->level->map ||
       plan->object_count>s->reserved || (plan->object_count && !plan->objects) ||
       s->pool.active_count!=s->reserved || s->world->entity_count!=s->reserved) return -1;
    for(i=0;i<SSA_ENTITY_POOL_CAPACITY;++i) if(s->actors[i].owned) return -1;
    for(i=0;i<plan->object_count;++i)
        if(!s->pool.occupied[i] || plan->objects[i].entity.owner!=s->world ||
           plan->objects[i].entity.sector<0 ||
           (unsigned)plan->objects[i].entity.sector>=plan->map->sector_count) return -1;
    for(i=0;i<plan->object_count;++i) {
        SsaSpawnObject *p=&plan->objects[i];
        SsaScriptActor *a=&s->actors[i];
        if(!ssa_world_replace(s->world,&p->entity,&a->entity)) return -1;
        a->owned=1;a->activation.entity=&a->entity;
        a->activation.state=p->variant;a->activation.facing=p->facing;
        a->script.x=a->entity.body.x;a->script.y=a->entity.body.y;a->script.z=a->entity.body.z;
        a->script.flags=a->entity.body.flags;a->script.type=a->entity.type;
        a->script.variant=p->variant;a->script.facing=p->facing;a->script.trigger=p->trigger;
        a->animation=p->animation;a->animation_ticks=p->animation_ticks;
    }
    return 0;
}

typedef struct SpawnCall {
    SsaScriptSpawn *s;
    const SsaScriptSpawnOps *ops;
    unsigned slot;
} SpawnCall;

static SsaLevelScriptEntity *allocate(void *context)
{
    SpawnCall *c = context;
    SsaScriptSpawn *s = c->s;
    SsaScriptActor *a;
    unsigned slot;
    int result;
    s->pending_slot = SSA_ENTITY_POOL_CAPACITY;
    if (s->world->entity_count >= s->world->capacity) return NULL;
    result = ssa_entity_pool_allocate(&s->pool, &slot);
    if (result < 0) s->failed = 1;
    if (result != 1) return NULL;
    a = &s->actors[slot];
    a->owned = 1;
    a->activation.entity = &a->entity;
    s->pending_slot = slot;
    return &a->script;
}

static SsaScriptActor *pending(SpawnCall *c, SsaLevelScriptEntity *e)
{
    SsaScriptActor *a = actor(c->s, c->s->pending_slot);
    return a && &a->script == e ? a : NULL;
}

static int animation(void *context, SsaLevelScriptEntity *e, unsigned id, unsigned ticks)
{
    SpawnCall *c = context;
    SsaScriptActor *a = pending(c, e);
    if (!a) return -1;
    project(a);
    animation_set(a, id, ticks);
    return 0;
}

static int insert(void *context, SsaLevelScriptEntity *e)
{
    SpawnCall *c = context;
    SsaScriptActor *a = pending(c, e);
    if (!a) return -1;
    project(a);
    if (!ssa_world_insert(c->s->world, &a->entity)) return -1;
    e->floor = (uint16_t)a->entity.body.floor;
    return 0;
}

static void flush(SsaScriptSpawn *s)
{
    SsaScriptActor *a = actor(s, s->pending_slot);
    if (a) a->entity.body.z = a->script.z;
    s->pending_slot = SSA_ENTITY_POOL_CAPACITY;
}

static int particle(void *context, unsigned id, int32_t x, int32_t y, int32_t z)
{
    SpawnCall *c = context;
    flush(c->s);
    if (c->s->failed) return -1;
    return c->ops->particle(c->ops->context, c->s, id, x, y, z);
}

int ssa_script_spawn_step(SsaScriptSpawn *s, unsigned ticks, int32_t fallback_z,
                          const SsaScriptSpawnOps *ops, const SsaSectorMoverOps *sound)
{
    SpawnCall c = {s, ops, 0};
    SsaLevelScriptOps adapters = {allocate, animation, insert, particle, NULL, &c};
    int result;
    if (!ready(s) || ticks > 255 || !ops || !ops->particle) return -1;
    s->updating = 1;
    s->failed = 0;
    result = ssa_level_runtime_step(s->level, ticks, fallback_z, &adapters, sound);
    flush(s);
    s->updating = 0;
    return result;
}
int ssa_script_spawn_scripts_step(SsaScriptSpawn *s,unsigned ticks,int32_t fallback_z,
                                  const SsaScriptSpawnOps *ops,const SsaSectorMoverOps *sound)
{
    SpawnCall c={s,ops,0};
    SsaLevelScriptOps adapters={allocate,animation,insert,particle,NULL,&c};
    int result;
    if (!ready(s) || ticks>255 || !ops || !ops->particle) return -1;
    s->updating=1; s->failed=0;
    result=ssa_level_runtime_scripts_step(s->level,ticks,fallback_z,&adapters,sound);
    flush(s); s->updating=0;
    return result;
}

static int activation_event(void *context, const SsaEnemyActivation *view,
                            const SsaEnemyActivationGame *game,
                            SsaEnemyActivationEvent event,
                            int32_t a, int32_t b, int32_t c, int32_t d)
{
    SpawnCall *call = context;
    (void)view;
    (void)game;
    if (event == SSA_ENEMY_ACTIVATION_ANIMATION)
        animation_set(&call->s->actors[call->slot], (unsigned)a, (unsigned)b);
    return call->ops->event(call->ops->context, call->s, call->slot, event, a, b, c, d);
}

int ssa_script_spawn_activate(SsaScriptSpawn *s, unsigned slot, unsigned ticks,
                              const SsaScriptSpawnOps *ops)
{
    SsaScriptActor *a;
    SpawnCall c = {s, ops, slot};
    SsaEnemyActivationOps adapters = {activation_event, &c};
    int result;
    if (!ready(s) || !(a = actor(s, slot)) || a->entity.owner != s->world ||
        ticks > 255) return -1;
    s->updating = 1;
    result = ssa_enemy_activation_step(&a->activation, &s->game,
        ops && ops->event ? &adapters : NULL, ticks);
    a->script.flags = a->entity.body.flags;
    a->script.timer = a->activation.timer;
    a->script.variant = a->activation.state;
    a->script.facing = a->activation.facing;
    s->updating = 0;
    return result;
}

int ssa_script_spawn_defeat(SsaScriptSpawn *s, unsigned slot)
{
    SsaScriptActor *a;
    if (!ready(s) || !(a = actor(s, slot)) || a->group_released ||
        a->entity.owner != s->world) return -1;
    if (!ssa_level_script_release(&s->level->script, a->script.trigger)) return -1;
    a->group_released = 1;
    return 0;
}

int ssa_script_spawn_release(SsaScriptSpawn *s, unsigned slot)
{
    SsaScriptActor *a;
    if (!ready(s) || !(a = actor(s, slot)) ||
        (a->entity.owner && a->entity.owner != s->world)) return -1;
    if (a->entity.owner && !ssa_world_remove(s->world, &a->entity)) return -1;
    if (ssa_entity_pool_release(&s->pool, slot) != 1) return -1;
    if (s->game.boss == &a->entity) s->game.boss = NULL;
    memset(a, 0, sizeof(*a));
    return 0;
}

int ssa_script_spawn_free(SsaScriptSpawn *s)
{
    unsigned i;
    if (!ready(s)) return -1;
    for (i = 0; i < SSA_ENTITY_POOL_CAPACITY; ++i)
        if (s->actors[i].owned && ssa_script_spawn_release(s, i)) return -1;
    memset(s, 0, sizeof(*s));
    return 0;
}
