#ifndef SSA_WEAPON_HIT_H
#define SSA_WEAPON_HIT_H
#include "ssa/weapon_aim.h"

typedef struct SsaWeaponHit {
    SsaEntity *target;
    int32_t x,y,z;
    unsigned impact;
} SsaWeaponHit;
/* Geometry of A444/9DE4. Use the SAME trace context as the preceding aim:
 * top/bottom slopes and opening retain original shared-global semantics.
 * Facing is the original body byte, not the camera heading. Returns1/-1.
 * A target blocks even when health or damage is zero. After this query releases
 * the world lock, the caller checks target health: nonzero damage and health
 * emit 9D4C impact BEFORE 37F4 damage. Wall impacts always emit 9D4C with NULL
 * target. Effects may not mutate geometry or entity ownership. */
int ssa_weapon_hit_query(SsaWeaponAim *, SsaEntity *, uint8_t facing,
    int32_t start_x_distance,int32_t start_y_distance,int32_t z_offset,
    int32_t range,int32_t slope,SsaWeaponHit *);
/* PC free aim: full yaw precision, explicit eye height, exact vertical ray,
 * including sector floors/ceilings. Does not perform original autoaim. */
int ssa_weapon_hit_query_free(SsaWeaponAim *,SsaEntity *,uint16_t heading,
    int32_t z_offset,int32_t range,int32_t slope,SsaWeaponHit *);
/* Presentation-only exact contact, without the original impact-particle
 * backoff. Use dedicated query storage; no damage or gameplay callbacks. */
int ssa_weapon_hit_query_camera(SsaWeaponAim *,SsaEntity *,uint16_t heading,
    int32_t z_offset,int32_t range,int32_t slope,SsaWeaponHit *);
/* PC ray with no weapon-distance cap. Walk short, bounded segments until the
 * first obstruction or the world boundary. exact selects camera contact rather
 * than particle backoff. Uses original source identity throughout the trace. */
int ssa_weapon_hit_query_unlimited(SsaWeaponAim *,SsaEntity *,uint16_t heading,
    int32_t z_offset,int32_t slope,SsaWeaponHit *,int exact);
#endif
