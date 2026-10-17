#ifndef SSA_ENEMY_QUERIES_H
#define SSA_ENEMY_QUERIES_H
#include "ssa/enemy_ray.h"
#include "ssa/enemy_targeting.h"
#include "ssa/enemy_navigation.h"
#include "ssa/script_spawn.h"

typedef struct SsaEnemyQueries {
    SsaScriptSpawn *spawn;
    const SsaMathTables *math;
    SsaEntity *reserved[SSA_ENTITY_POOL_CAPACITY];
    unsigned reserved_count, updating;
    SsaEnemyPath path;
    SsaEnemyRay ray;
    SsaEnemyNavigationOps ops;
} SsaEnemyQueries;
/* Zero-init, stable storage; do not copy (ops.context refers to this object).
 * reserved_entities supplies the initial slot order when script_spawn_init
 * ran. The pointer array is copied. Adopted actors use canonical owned storage
 * and can be released/reused; remaining external bodies stay registered.
 * Call before binding controllers; free before script_spawn/world teardown.
 * ops provides native visibility/nearest/path for enemy_cd_runtime; its other
 * callbacks are NULL because that runtime supplies native movement/hazards.
 * Queries may run during a controller update, never during world/level queries.
 * They do not change world.blocking_entity or any entity. Returns 0/-1. */
int ssa_enemy_queries_init(SsaEnemyQueries *, SsaScriptSpawn *, const SsaMathTables *,
                            SsaEntity *const *reserved_entities, size_t count);
int ssa_enemy_queries_free(SsaEnemyQueries *);
#endif
