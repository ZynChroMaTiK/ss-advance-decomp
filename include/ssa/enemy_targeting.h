#ifndef SSA_ENEMY_TARGETING_H
#define SSA_ENEMY_TARGETING_H
#include "ssa/world.h"

/* Original 036C4 / 03758. ordered is the active-pool iteration order, not
 * spatial-list order. At most 100 stable, nonnull pointers; source need not
 * belong to that array. Strict distance ties retain the first candidate.
 * Returns 0 and stores NULL when none qualify; -1 leaves out unchanged.
 * These read-only queries do not invoke callbacks or require world ownership. */
int ssa_enemy_nearest(const SsaEntity *source, SsaEntity *const *ordered,
                      size_t count, SsaEntity **out);
int ssa_pickup_nearest(const SsaEntity *source, SsaEntity *const *ordered,
                       size_t count, SsaEntity **out);
/* Original B18C reject bit, preserving the actual bit mask (not a boolean).
 * Zero means the sector table does not reject visibility, not that a ray is
 * clear. Returns -1 for invalid sectors or a truncated reject table. */
int ssa_sector_reject(const SsaMapAssets *map, int source_sector, int target_sector);
#endif
