#ifndef SSA_STATISTICS_H
#define SSA_STATISTICS_H
#include "ssa/weapon_progress.h"

typedef struct SsaEnemyStatistics {
    uint32_t spawned, hits, kills; /* GAME+130/+134/+138, stride12. */
} SsaEnemyStatistics;
typedef struct SsaStatisticsRows {
    SsaWeaponProgressStats weapons[10];
    SsaEnemyStatistics enemies[12];
} SsaStatisticsRows;
typedef struct SsaLevelStatistics {
    SsaStatisticsRows rows;
    uint32_t elapsed, score, pickups;
    int16_t health, armor6, armor4;
    uint8_t level;
} SsaLevelStatistics;
typedef struct SsaStatisticsTotals {
    uint32_t score;              /* GAME+2F0: overwritten, not added. */
    int32_t health, armor6, armor4;
    uint32_t level, elapsed, spawned, kills, hits, weapon_hits, pickups;
} SsaStatisticsTotals;
/* Original1E48. Add elapsed, all ten weapon HIT counters, all twelve enemy
 * rows and accepted pickups with unsigned32 wrap. Overwrite score, signed
 * health/armor and byte level. Source unchanged. Returns1/-1 (null); null
 * rejects before writes. No idempotence: caller commits once per completion.
 * Objects must be distinct and remain stable throughout these calls. */
int ssa_statistics_complete(SsaStatisticsTotals *,const SsaLevelStatistics *);
/* Original1CF4 saves only the weapon/enemy rows. Original1D9C restores those
 * rows and clears current pickups; elapsed, score, health/armor, level and
 * campaign totals are untouched. Neither helper performs player restoration. */
int ssa_statistics_checkpoint_capture(SsaStatisticsRows *,const SsaLevelStatistics *);
int ssa_statistics_checkpoint_restore(SsaLevelStatistics *,const SsaStatisticsRows *);
#endif
