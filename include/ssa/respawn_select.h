#ifndef SSA_RESPAWN_SELECT_H
#define SSA_RESPAWN_SELECT_H
#include "ssa/spawn.h"

typedef struct SsaRespawnPosition { int32_t x, y; } SsaRespawnPosition;
/* Original 08001B18: maximize nearest-player distance, capped at 10000.
 * Players at exactly (0,0) are skipped, ties retain the first marker, and
 * the output Z deliberately copies marker Y. Distances preserve ARM wrapping.
 * Returns 1 selected, 0 no positive score, -1 invalid. Output unchanged on
 * 0/-1. players_count may be 0..4; tables must be loaded from the ROM.
 * Output may alias a marker; other storage must remain stable during the call. */
int ssa_respawn_select(SsaSpawnMarker *out, const SsaSpawnMarker *markers,
                       size_t count, const SsaRespawnPosition *players,
                       unsigned player_count, const SsaMathTables *tables);
#endif
