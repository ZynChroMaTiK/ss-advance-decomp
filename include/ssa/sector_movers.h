#ifndef SSA_SECTOR_MOVERS_H
#define SSA_SECTOR_MOVERS_H
#include "ssa/map_assets.h"

/* Original records are retained verbatim: sector +0, lower +8, upper +10,
 * speed +12, flags +14, copied sector tag +16; other words remain opaque. */
typedef struct SsaSectorMovers {
    SsaMapAssets *map;
    uint8_t records[25][20];
    uint32_t hazard_sectors[8];
    unsigned count, hazard_count, updating;
} SsaSectorMovers;
typedef struct SsaSectorMoverOps {
    int (*sound)(void *context, unsigned id);
    void *context;
} SsaSectorMoverOps;
typedef enum SsaSectorMoverCommand {
    SSA_SECTOR_MOVER_OPEN, SSA_SECTOR_MOVER_CLOSE,
    SSA_SECTOR_MOVER_LOCK, SSA_SECTOR_MOVER_UNLOCK
} SsaSectorMoverCommand;

/* Init loads the 500-byte level table and changes map heights. It excludes the
 * original initializer's final switch initialization (0801C3B4). out must be
 * zeroed or idle; map storage/layout must remain stable for its lifetime.
 * All mutators return 1 on success, 0 on invalid input/callback failure. Sound
 * is optional, negative callback return fails. Failure can leave partial
 * effects. Callbacks must not change this state/map or reenter its APIs. */
int ssa_sector_movers_init(SsaSectorMovers *out, SsaMapAssets *map,
                           const uint8_t *rom, size_t rom_size, unsigned level);
int ssa_sector_movers_command(SsaSectorMovers *state, unsigned index,
                              SsaSectorMoverCommand command,
                              const SsaSectorMoverOps *ops);
int ssa_sector_movers_activate_line(SsaSectorMovers *state, unsigned line,
                                    const SsaSectorMoverOps *ops);
int ssa_sector_movers_step(SsaSectorMovers *state, unsigned ticks,
                           const SsaSectorMoverOps *ops);
/* Active closing-door hazard list. Duplicate entries and swap-removal are
 * intentional. Query returns -1 for invalid state, otherwise 0/1. */
int ssa_sector_movers_hazard_contains(const SsaSectorMovers *state,
                                      uint32_t sector);
#endif
