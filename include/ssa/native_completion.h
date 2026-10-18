#ifndef SSA_NATIVE_COMPLETION_H
#define SSA_NATIVE_COMPLETION_H
#include "ssa/completion.h"
#include "ssa/native_statistics.h"

typedef struct SsaCompletionRecord {
    SsaWeaponProgress weapons;
    SsaStatisticsTotals totals;
    SsaStatisticsRows checkpoint;
    unsigned next_level;
} SsaCompletionRecord;
/* Snapshot a completed living single-player game into the original tally.
 * Source game must stay frozen until record. It is never mutated by this API.
 * Presentation/music/input/audio completion are explicit completion ops.
 * This is not an automatic replacement for native_game_tick or the PC shell. */
int ssa_native_completion_init(SsaCompletion *,const SsaNativeGame *);
/* Original completion data order1231C ->1E48 ->1CF4, then next level. Supplies
 * bonus-adjusted score and the original setting-byte shot/hit reset. Totals
 * seed is unchanged; output is value-only, for the caller's campaign lifetime.
 * First success marks tally.recorded; duplicate or early records reject.
 * The source game is unchanged, including live score/counters. No disk save,
 * player restoration, unlocks, next-level admission or ending routing implied.
 * Returns1/-1. Invalid/error/busy leaves output and recorded unchanged. */
int ssa_native_completion_record(SsaCompletionRecord *,SsaCompletion *,
    const SsaNativeGame *,const SsaStatisticsTotals *,uint8_t setting);
#endif
