#ifndef SSA_ACTOR_ANIMATION_H
#define SSA_ACTOR_ANIMATION_H
#include <stddef.h>
#include <stdint.h>
#include "ssa/entity_pool.h"

#define SSA_ACTOR_ANIMATION_COUNT 142u
#define SSA_ACTOR_ANIMATION_WORDS 772u
/* Entity +58..5F, separate from the 48-byte sprite/weapon animation state. */
typedef struct SsaActorAnimation {
    uint16_t animation, frame;
    uint8_t index, elapsed, period, done;
} SsaActorAnimation;
typedef struct SsaActorAnimations {
    uint16_t words[SSA_ACTOR_ANIMATION_WORDS];
    uint16_t offsets[SSA_ACTOR_ANIMATION_COUNT];
    unsigned loaded;
} SsaActorAnimations;
/* Copies sequence data; no ROM instructions execute. Returns 1/-1. */
int ssa_actor_animations_load(SsaActorAnimations *, const void *rom, size_t size);
/* 05394: accepts IDs <=8D or FFFF, truncates period to byte, resets index,
 * elapsed and done; does not change displayed frame. 1 applied/0 ignored/-1. */
int ssa_actor_animation_set(SsaActorAnimation *, uint16_t id, uint16_t period);
/* One entity from 00908. Flag800 skips every field. FFFF ID is unsafe in the
 * original unless skipped: native returns -1 without mutation. ticks<=255.
 * FFFF sequence marker loops with done=FF; FFFE holds with done=FE/period=FF.
 * FFFA..FFFD emits its byte and skips the marker. Returns 1/-1. */
int ssa_actor_animation_step(SsaActorAnimation *, const SsaActorAnimations *,
                              uint16_t body_flags, unsigned ticks);
/* Full forward active-index traversal of 00908. State/flags arrays are indexed
 * by stable slot. No callbacks or pool mutations. Validates pool bookkeeping;
 * a bad animation leaves all entries unchanged. Returns 1/-1. */
int ssa_actor_animation_pool_step(SsaActorAnimation states[SSA_ENTITY_POOL_CAPACITY],
    const uint16_t flags[SSA_ENTITY_POOL_CAPACITY], const SsaEntityPool *,
    const SsaActorAnimations *, unsigned ticks);
#endif
