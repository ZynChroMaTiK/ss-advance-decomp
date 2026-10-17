#ifndef SSA_HORIZONTAL_H
#define SSA_HORIZONTAL_H
#include "ssa/physics.h"
typedef struct SsaHorizontalOps {
    int (*try_move)(void *, SsaBody *, int32_t x, int32_t y);
    int (*blocked_by_entity)(void *);
    int (*slide)(void *, SsaBody *);
    void *context;
} SsaHorizontalOps;
/* 08004BDC control flow, provided world movement and wall sliding operations.
 * try_move returns 1 success/0 blocked/-1 invalid. blocked_by_entity returns
 * 1 entity/0 wall/-1 invalid. slide returns -1 on error, nonnegative otherwise.
 * Original initial clamped velocity is reused throughout the tick batch.
 * Return -1 for invalid integration; 0 otherwise. Zero ticks still clamp
 * velocity. Remote multiplayer player bodies are not integrated locally. */
int ssa_horizontal_step(SsaBody *body, uint8_t player_index, uint8_t ticks,
                        uint8_t player_count, uint8_t local_player,
                        const SsaHorizontalOps *ops);
#endif
