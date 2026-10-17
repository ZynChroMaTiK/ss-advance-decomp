#ifndef SSA_PICKUP_RESPAWN_RUNTIME_H
#define SSA_PICKUP_RESPAWN_RUNTIME_H
#include "ssa/pickup_respawn.h"
#include "ssa/combat_runtime.h"
typedef struct SsaPickupRespawnRuntime {
    SsaCombatRuntime *combat;
    const SsaSpawnPlan *plan;
    const SsaPickupRespawnData *data;
    SsaPickupRespawn state;
    unsigned pending_slot,updating;
} SsaPickupRespawnRuntime;
typedef struct SsaPickupRespawnRuntimeOps {
    /* Observe only. World/pool/player mutation or reentry is prohibited. */
    int (*particle)(void *,const SsaPickupRespawnRuntime *,unsigned,int32_t,int32_t,int32_t);
    void *context;
} SsaPickupRespawnRuntimeOps;
/* Zero-init; borrows stable combat/plan/data. Uses D9 markers and the shared
 * 100-slot pool. Newly allocated E1 actors belong to script_spawn; release them
 * through script_spawn on pickup/destruction. Free this adapter before its
 * dependencies; free does not remove spawned actors. Returns 0/-1. */
int ssa_pickup_respawn_runtime_init(SsaPickupRespawnRuntime *,SsaCombatRuntime *,
    const SsaSpawnPlan *,const SsaPickupRespawnData *);
int ssa_pickup_respawn_runtime_free(SsaPickupRespawnRuntime *);
/* Level from spawn.level; player count from combat. Run after movers/VM.
 * Native occupancy, allocation, animation metadata and insertion/floor Z.
 * Only particle 5 is external; fallback_z represents the original address 8
 * read on pool exhaustion. Retains allocated/inserted actors and advanced
 * indices on failure. Returns 0/-1. */
int ssa_pickup_respawn_runtime_step(SsaPickupRespawnRuntime *,unsigned ticks,
    int32_t fallback_z,const SsaPickupRespawnRuntimeOps *);
#endif
