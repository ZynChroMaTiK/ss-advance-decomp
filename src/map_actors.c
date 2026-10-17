#include "ssa/map_actors.h"
#include <string.h>

static int ready(const SsaMapActors *a)
{
    return a && a->loaded==1 && a->world && a->world->cells && a->world->sectors &&
        a->plan.map==a->world->collision.map && a->data && a->data->loaded==1 &&
        a->plan.objects && a->plan.object_count<=SSA_ENTITY_POOL_CAPACITY &&
        a->pool.active_count==a->plan.object_count;
}
int ssa_map_actors_init(SsaMapActors *out,SsaWorld *world,const SsaSpawnDefinitions *defs,
    const SsaActorAnimations *data,unsigned level)
{
    SsaMapActors a={0};unsigned i,slot;
    if (!out || out->loaded || out->updating || out->plan.map || out->plan.objects ||
        out->plan.player_starts || out->plan.teleport_starts ||
        out->world || out->data || !world || !world->cells || !world->sectors ||
        world->query_active || world->entity_count || !world->capacity ||
        world->capacity>SSA_ENTITY_POOL_CAPACITY || !data || data->loaded!=1 ||
        !ssa_spawn_plan_load(&a.plan,world->collision.map,defs,level,1,world->capacity))
        return -1;
    a.world=world;a.data=data;
    if (ssa_entity_pool_init(&a.pool)<0) goto fail;
    for (i=0;i<a.plan.object_count;++i) {
        if (ssa_entity_pool_allocate(&a.pool,&slot)!=1 || slot!=i) goto fail;
        if (ssa_actor_animation_set(&a.animations[i],a.plan.objects[i].animation,
             a.plan.objects[i].animation_ticks)<0) goto fail;
    }
    if (!ssa_spawn_plan_insert(&a.plan,world)) goto fail;
    a.loaded=1;*out=a;return 1;
fail:
    ssa_spawn_plan_free(&a.plan);return -1;
}
int ssa_map_actors_step(SsaMapActors *a,unsigned ticks)
{
    uint16_t flags[SSA_ENTITY_POOL_CAPACITY]={0};unsigned i;int result;
    if (!ready(a) || a->updating || a->world->query_active || ticks>255) return -1;
    for (i=0;i<a->plan.object_count;++i) {
        if(a->plan.objects[i].entity.owner!=a->world)return -1;
        flags[i]=a->plan.objects[i].entity.body.flags;
    }
    a->updating=1;
    result=ssa_actor_animation_pool_step(a->animations,flags,&a->pool,a->data,ticks);
    a->updating=0;return result<0?-1:1;
}
int ssa_map_actors_free(SsaMapActors *a)
{
    unsigned i;
    if (!ready(a) || a->updating || a->world->query_active) return -1;
    for(i=0;i<a->plan.object_count;++i)
        if(a->plan.objects[i].entity.owner!=a->world)return -1;
    for(i=0;i<a->plan.object_count;++i)
        if(!ssa_world_remove(a->world,&a->plan.objects[i].entity))return -1;
    if(!ssa_spawn_plan_free(&a->plan))return -1;
    memset(a,0,sizeof(*a));return 1;
}
const SsaSpawnObject *ssa_map_actor_object(const SsaMapActors *a,unsigned i)
{
    if(!ready(a) || i>=a->plan.object_count || a->plan.objects[i].entity.owner!=a->world)
        return NULL;
    return &a->plan.objects[i];
}
const SsaActorAnimation *ssa_map_actor_animation(const SsaMapActors *a,unsigned i)
{return ssa_map_actor_object(a,i)?&a->animations[i]:NULL;}
