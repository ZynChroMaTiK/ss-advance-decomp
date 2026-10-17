#ifndef SSA_COMBAT_EXPLOSION_H
#define SSA_COMBAT_EXPLOSION_H
#include "ssa/world.h"

typedef struct SsaCombatExplosion { unsigned updating; } SsaCombatExplosion;
typedef struct SsaCombatExplosionOps {
    int (*damage)(void *context, SsaEntity *target, uint32_t damage);
    void *context;
} SsaCombatExplosionOps;

/* Original 035A8, with 037F4 as a synchronous damage boundary. ordered follows
 * the original pool's active order, including external entities. At most 100
 * stable nonnull pointers. Callbacks may update body fields but must preserve
 * the active array/count and all entity storage until return. No nested call
 * with this workspace. Zero-initialize the workspace before first use.
 * radius_squared == 0 uses map reject bits and excludes types above D6;
 * otherwise distance is strictly less than the unsigned squared radius and
 * no visibility test is performed. include_players is a boolean gate only
 * for entities outside CD..D8. Attenuation retains original 32-bit wrap.
 * Return 0 on success, -1 on invalid input, callback failure or reentry.
 * Earlier effects remain applied after failure. map is only needed for the
 * zero-radius path; a valid empty active array needs no map or callback. */
int ssa_combat_explode(SsaCombatExplosion *explosion, const SsaMapAssets *map,
                       SsaEntity *source, SsaEntity *const *ordered, size_t count,
                       uint8_t game_mode, uint32_t radius_squared,
                       uint32_t damage, int include_players,
                       const SsaCombatExplosionOps *ops);
#endif
