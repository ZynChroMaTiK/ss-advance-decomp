#include "ssa/combat_rewards.h"

int ssa_combat_reward(uint16_t target_type, uint32_t *score) {
    if (!score) return -1;
    if (target_type >= 0xcdu && target_type <= 0xd6u) *score += 2000u;
    return 0;
}
