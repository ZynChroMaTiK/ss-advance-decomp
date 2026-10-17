#ifndef SSA_PROJECTILE_H
#define SSA_PROJECTILE_H
#include "ssa/world.h"
/* Host projection, not a ROM overlay. Entity and source addresses stay stable. */
typedef struct SsaProjectile {
    SsaEntity *entity, *source;
    int32_t speed;
    int16_t health;
    uint16_t timer, parameter34, animation;
    uint8_t state, facing, owner, weapon, animation_done, animation_period;
    unsigned updating;
} SsaProjectile;
#endif
