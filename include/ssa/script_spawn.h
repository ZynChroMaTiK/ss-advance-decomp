#ifndef SSA_SCRIPT_SPAWN_H
#define SSA_SCRIPT_SPAWN_H
#include "ssa/entity_pool.h"
#include "ssa/enemy_activation.h"
#include "ssa/level_runtime.h"
#include "ssa/spawn.h"

typedef struct SsaScriptActor {
    /* Body and activation are canonical after insertion; script retains VM
     * spawn coordinates and tag. Read fields, use APIs to change ownership. */
    SsaEntity entity;
    SsaLevelScriptEntity script;
    SsaEnemyActivation activation;
    uint16_t animation;
    uint8_t animation_ticks, animation_frame, animation_counter, animation_flags;
    uint8_t owned, group_released;
    uint16_t parameter34; /* Shared entity +34; periodic health-drain period. */
    uint16_t parameter36; /* Shared entity +36; periodic health-drain countdown. */
    uint16_t animation_display_frame; /* Entity +5A, advanced by 08000908. */
} SsaScriptActor;

typedef struct SsaScriptSpawn {
    SsaLevelRuntime *level;
    SsaWorld *world;
    SsaEntityPool pool;
    SsaScriptActor actors[SSA_ENTITY_POOL_CAPACITY];
    SsaEnemyActivationGame game;
    unsigned reserved, pending_slot, updating, failed;
} SsaScriptSpawn;

typedef struct SsaScriptSpawnOps {
    int (*particle)(void *, const SsaScriptSpawn *, unsigned,
                    int32_t, int32_t, int32_t);
    int (*event)(void *, const SsaScriptSpawn *, unsigned slot,
                 SsaEnemyActivationEvent, int32_t, int32_t, int32_t, int32_t);
    void *context;
} SsaScriptSpawnOps;

/* Zero-init before init. All storage, level, map and world must remain stable.
 * Existing world entities initially reserve pool slots. Map objects can then be
 * adopted before runtime borrowers bind; other reserved bodies remain external.
 * The reserved count describes the initial prefix, not a minimum live count.
 * This binds entity ownership, not the full
 * original entity dispatcher. No callbacks may mutate/reenter bound state.
 * All functions reject world queries and level/bridge reentry.
 * Init/free return 0/-1. Free unlinks and clears owned actors, including failed
 * partial spawns, without sending defeat notifications. Free before level/world.
 */
int ssa_script_spawn_init(SsaScriptSpawn *, SsaLevelRuntime *, SsaWorld *);
/* Adopt the map-object prefix inserted before players, preserving world list
 * and pool order. Plan remains valid metadata but no longer owns live bodies.
 * Call exactly once, immediately after init and before binding other runtimes.
 * Returns 0/-1. All inputs are checked before ownership changes. */
int ssa_script_spawn_adopt_plan(SsaScriptSpawn *, SsaSpawnPlan *);
/* Canonical occupied slot: owned actor takes precedence over initial external
 * prefix binding. NULL for an inactive or invalid slot. */
SsaEntity *ssa_script_spawn_entity(SsaScriptSpawn *, SsaEntity *const *, unsigned);
int ssa_script_spawn_free(SsaScriptSpawn *);
/* Run once after entity updates. Native allocation, animation metadata and
 * world insertion replace VM callbacks. Particle callback is required and sees
 * the final floor Z. Mover sound is optional. Returns 0/-1, retaining earlier
 * effects on failure. A failed inserted/allocated actor remains owned until
 * explicit release/free. Pool/world exhaustion is normal VM allocation failure.
 */
int ssa_script_spawn_step(SsaScriptSpawn *, unsigned ticks, int32_t fallback_z,
                          const SsaScriptSpawnOps *, const SsaSectorMoverOps *);
/* VM stage alone, after ssa_level_runtime_movers_step. Do not also call the
 * combined step in that frame. Identical allocation/insertion/error contracts. */
int ssa_script_spawn_scripts_step(SsaScriptSpawn *, unsigned ticks, int32_t fallback_z,
                                  const SsaScriptSpawnOps *, const SsaSectorMoverOps *);
/* State-zero enemies CD..D8 only: 1 activated, 0 waiting, -1 invalid/error.
 * CE and bosses require event callback; boss animation also updates metadata.
 * Scripted spawn countdown is 60 then 30 ticks. Does not run subsequent AI.
 */
int ssa_script_spawn_activate(SsaScriptSpawn *, unsigned slot, unsigned ticks,
                              const SsaScriptSpawnOps *);
/* Defeat sends group release once (0 success/-1 invalid); it does not destroy
 * the actor or simulate combat. Release unlinks, returns slot and clears payload
 * (0/-1), without notifying the script. Call defeat explicitly on actual death.
 * External reserved bodies cannot be released through this API.
 */
int ssa_script_spawn_defeat(SsaScriptSpawn *, unsigned slot);
int ssa_script_spawn_release(SsaScriptSpawn *, unsigned slot);
#endif
