#ifndef SSA_MAP_ACTORS_H
#define SSA_MAP_ACTORS_H
#include "ssa/spawn.h"
#include "ssa/actor_animation.h"

/* Original01984 map construction and00908 animation state, before AI activation.
 * Owns stable map objects; borrows world/map/animation tables until free. These
 * are not gameplay-ready enemies: original initial dimensions/health/flags
 * remain unchanged, and no attack/death/pickup/script effects are invented. */
typedef struct SsaMapActors {
    SsaSpawnPlan plan;
    SsaEntityPool pool;
    SsaActorAnimation animations[SSA_ENTITY_POOL_CAPACITY];
    SsaWorld *world;
    const SsaActorAnimations *data;
    unsigned loaded,updating;
} SsaMapActors;

/* Zero-init. World must be empty, use the supplied map, and have1..100 slots.
 * Map objects are allocated and inserted before separate player initialization,
 * in the original type/file order. Success1/failure-1, atomic ownership on error.
 * All objects retain source thing/variant/facing/trigger/animation metadata. */
int ssa_map_actors_init(SsaMapActors *,SsaWorld *,const SsaSpawnDefinitions *,
    const SsaActorAnimations *,unsigned level);
/* Whole00908 clock for this map-object subset in original allocation order;
 * players/scripted entities have their separate animation owners. No AI update.
 * Original bodyflag800 skip and frame markers preserved. Success1/-1; invalid
 * animation/ticks leave all states unchanged. No nested update/world query. */
int ssa_map_actors_step(SsaMapActors *,unsigned ticks);
/* Detach ONLY this owner’s objects, then release storage. Other world bodies
 * remain registered. Free before world/map/data; no world query/reentry. */
int ssa_map_actors_free(SsaMapActors *);
/* Stable borrowed object/state lookup. NULL on invalid index/owner. */
const SsaSpawnObject *ssa_map_actor_object(const SsaMapActors *,unsigned index);
const SsaActorAnimation *ssa_map_actor_animation(const SsaMapActors *,unsigned index);
#endif
