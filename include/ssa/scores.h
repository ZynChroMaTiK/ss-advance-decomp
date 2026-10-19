#ifndef SSA_SCORES_H
#define SSA_SCORES_H
#include <stddef.h>
#include <stdint.h>
enum {SSA_SCORE_COUNT=18,SSA_SCORE_NAME=12,SSA_SCORES_BYTES=320};
typedef struct SsaScoreEntry {uint32_t score;char name[SSA_SCORE_NAME];} SsaScoreEntry;
typedef struct SsaScores {SsaScoreEntry entries[SSA_SCORE_COUNT];char name[SSA_SCORE_NAME+1];} SsaScores;
int ssa_scores_init(SsaScores *,const uint8_t *rom,size_t);
int ssa_scores_valid(const SsaScores *);
/* Original16F70: unsigned scores, earlier insertion on a tie.18=no place. */
int ssa_scores_rank(const SsaScores *,uint32_t score);
/* Full insertion, used when merging a submitted record with the latest file.
 * The twelve-byte input uses underscores for blank slots.18 leaves data intact. */
int ssa_scores_insert(SsaScores *,uint32_t score,const char name[SSA_SCORE_NAME]);
/* Versioned little-endian format, printable names, sorted entries and CRC32.
 * Failed decode leaves the destination unchanged. No progress/ROM is stored. */
int ssa_scores_encode(const SsaScores *,uint8_t *,size_t);
int ssa_scores_decode(SsaScores *,const uint8_t *,size_t);
#endif
