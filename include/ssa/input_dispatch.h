#ifndef SSA_INPUT_DISPATCH_H
#define SSA_INPUT_DISPATCH_H
#include <stdint.h>

typedef struct SsaInputDispatchPlayer {
    const uint16_t *flags; /* Player +BC: canonical movement flags. */
    uint32_t *keys;        /* Player +A4: held low half, pressed high half. */
} SsaInputDispatchPlayer;

typedef struct SsaInputDispatch {
    SsaInputDispatchPlayer players[4];
    uint16_t game_flags;   /* GAME +04, bit100 bypasses network distribution. */
    uint8_t player_count;  /* GAME +10; the local branch still scans four slots. */
    uint8_t motion_index;  /* GAME +68, NOT the display-local index0300464E. */
    uint8_t exit_flag;     /* GAME +7D, nonzero bypasses network distribution. */
} SsaInputDispatch;

enum SsaInputDispatchResult {
    SSA_INPUT_DISPATCH_OK = 0,
    SSA_INPUT_DISPATCH_INVALID = -1,
    SSA_INPUT_DISPATCH_NETWORK_REQUIRED = -2
};

/* Exact local/bypass branch of 08005898 (05C26..05C5A). It is selected when
 * player_count<=1, game_flags&100, or exit_flag!=0. Otherwise return NETWORK_REQUIRED
 * before any side effect; no network transport or packet handling is emulated.
 * packed_keys is03003F84: the PROCESSED poll output copied by0244C, not raw_keys.
 * Write it only to slots with BC bit4 and slot==motion_index; leave all other
 * keys unchanged. No edge synthesis, input-mode suppression or counter updates.
 * Four stable, distinct player projections are required for the local branch,
 * including unused player slots. Null pointers fail atomically. */
int ssa_input_dispatch_local(const SsaInputDispatch *,uint32_t packed_keys);
#endif
