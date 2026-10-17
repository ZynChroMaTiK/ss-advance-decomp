#ifndef SSA_SPAWN_H
#define SSA_SPAWN_H
#include "ssa/world.h"

typedef struct SsaSpawnDefinition {
    uint16_t type, variant, animation, animation_ticks;
} SsaSpawnDefinition;
typedef struct SsaSpawnDefinitions { SsaSpawnDefinition thing[64]; } SsaSpawnDefinitions;
typedef struct SsaSpawnObject {
    SsaEntity entity;
    uint16_t thing_index, animation, trigger;
    uint8_t variant, facing, animation_ticks;
} SsaSpawnObject;
/* Original marker representation is whole units and an 8-bit heading. */
typedef struct SsaSpawnMarker { int16_t x, y, z; uint16_t heading; } SsaSpawnMarker;
typedef struct SsaSpawnPlan {
    const SsaMapAssets *map;
    SsaSpawnObject *objects;
    SsaSpawnMarker *player_starts, *teleport_starts;
    size_t object_count, player_count, teleport_count, dropped_count;
} SsaSpawnPlan;

/* Reads the 64-entry type translation table from the supplied AENP ROM. */
int ssa_spawn_definitions_load(SsaSpawnDefinitions *, const uint8_t *rom, size_t size);
/* Whole 08001984 construction phase. entity_limit models available entries in
 * the original 100-slot pool. Level is 0..15; players is 1..4. Zero-init plan.
 * Owns stable object/marker storage; map must outlive it. The original phase
 * leaves dimensions, flags and health zero. Enemy AI activation is NOT
 * implemented here; ordinary pickups retain zero dimensions in the original.
 * This is not a gameplay-ready level initializer. */
int ssa_spawn_plan_load(SsaSpawnPlan *, const SsaMapAssets *, const SsaSpawnDefinitions *,
                         unsigned level, unsigned players, size_t entity_limit);
/* Inserts in original allocation order. World must use the same map and have
 * enough capacity. Failure leaves no newly inserted plan objects behind. */
int ssa_spawn_plan_insert(SsaSpawnPlan *, SsaWorld *);
/* Refuses while any object is registered; detach/free world first. Never free
 * from a world query callback, even after removing its encountered object. */
int ssa_spawn_plan_free(SsaSpawnPlan *);
#endif
