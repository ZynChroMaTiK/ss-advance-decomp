#ifndef SSA_PLAYER_SPAWN_H
#define SSA_PLAYER_SPAWN_H
#include "ssa/spawn.h"
#include "ssa/inventory.h"
#include "ssa/player_motion.h"

/* Host state initialized by 08005E4C, not a GBA RAM overlay. Inventory is
 * deliberately empty: weapon setup is a later original initialization phase.
 * Keep all four records at stable addresses while their bodies are registered. */
typedef struct SsaPlayerSpawn {
    SsaEntity entity;
    SsaPlayerMotion motion;
    SsaPlayerVertical vertical;
    SsaInventory inventory;
    int16_t entity_health;
    uint16_t animation;
    uint8_t animation_ticks, active;
} SsaPlayerSpawn;

/* Original player construction and world insertion. Zero-initialize players[4]
 * before first use. Rejects registered output, active world queries, insufficient
 * world capacity and missing multiplayer markers without modifying outputs.
 * Single-player uses the first map thing of type 1; absence is a successful
 * empty initialization, matching the original. Multiplayer uses the first
 * count plan markers in their existing order. count=1..4, local<count.
 * Returns 1 success, 0 invalid/allocation/geometry failure. On failure no new
 * bodies remain registered; geometry failure can clear output records.
 * Caller supplies plan and world with the same validated, stable map. */
int ssa_players_spawn(SsaPlayerSpawn players[4], SsaWorld *, const SsaSpawnPlan *,
                       unsigned count, unsigned local);
#endif
