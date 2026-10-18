#ifndef SSA_NATIVE_STATISTICS_H
#define SSA_NATIVE_STATISTICS_H
#include "ssa/native_game.h"
#include "ssa/statistics.h"
/* Read-only snapshot at an idle native single-player boundary, including
 * accepted pickups and canonical player health/protection. Returns1/-1;
 * invalid/busy/error leaves output unchanged. Usable during play or after a
 * completion. Does not commit totals, restore a checkpoint or unlock levels.
 * Output must be distinct from game storage. No pointers escape the snapshot. */
int ssa_native_game_statistics(SsaLevelStatistics *,const SsaNativeGame *);
#endif
