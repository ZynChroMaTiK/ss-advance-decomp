#ifndef SSA_PICKUP_RESPAWN_H
#define SSA_PICKUP_RESPAWN_H
#include "ssa/spawn.h"
#include "ssa/level_script.h"
typedef struct SsaPickupRespawnData {uint16_t schedule[4][34];unsigned loaded;} SsaPickupRespawnData;
typedef struct SsaPickupRespawn {uint16_t elapsed,sequence,marker;unsigned updating;} SsaPickupRespawn;
typedef struct SsaPickupRespawnOps {
    int (*occupied)(void *,const SsaSpawnMarker *,int *);
    int (*allocate)(void *,SsaLevelScriptEntity **);
    int (*animation)(void *,SsaLevelScriptEntity *,unsigned,unsigned);
    int (*insert)(void *,SsaLevelScriptEntity *);
    int (*particle)(void *,unsigned,int32_t,int32_t,int32_t);
    void *context;
} SsaPickupRespawnOps;
/* Four original data schedules for levels 12..15. Returns 1/-1; atomic failure. */
int ssa_pickup_respawn_data_load(SsaPickupRespawnData *,const void *,size_t);
/* 05330: any active entity whose signed whole X and whole Y equal the marker
 * prevents respawn. Ignores fractional XY, type, Z and flags. Returns 1/0/-1. */
int ssa_pickup_respawn_occupied(const SsaSpawnMarker *,SsaEntity *const *,size_t);
/* Whole 1B854. Run after movers/VM, before 0908 and weapon controllers. Only
 * player_count!=1 and levels 12..15 operate. Strict elapsed>threshold; elapsed
 * wraps to 16 bits. Sequence and marker advance BEFORE occupancy/allocation. Stable
 * caller-owned markers and allocated entity; callbacks cannot reenter/mutate
 * state or replace/free storage. Insert supplies floor; final Z uses that floor.
 * NULL allocation still emits particle 5 with explicit fallback_z (original
 * reads address 8). Errors retain earlier changes. Returns 0/-1;
 * ticks/player_count<=255. */
int ssa_pickup_respawn_step(SsaPickupRespawn *,const SsaPickupRespawnData *,
    unsigned player_count,unsigned level,unsigned ticks,const SsaSpawnMarker *,
    size_t marker_count,int32_t fallback_z,const SsaPickupRespawnOps *);
#endif
