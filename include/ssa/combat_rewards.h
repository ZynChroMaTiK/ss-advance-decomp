#ifndef SSA_COMBAT_REWARDS_H
#define SSA_COMBAT_REWARDS_H
#include <stdint.h>

/* Original 080125E0: CD..D6 add 2000 to GAME+88, with unsigned wrap.
 * The caller decides whether a kill earns credit. No entities are created,
 * released or otherwise mutated. Returns 0, or -1 for a NULL score. */
int ssa_combat_reward(uint16_t target_type, uint32_t *score);
#endif
