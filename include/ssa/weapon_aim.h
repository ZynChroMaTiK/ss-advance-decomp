#ifndef SSA_WEAPON_AIM_H
#define SSA_WEAPON_AIM_H
#include "ssa/enemy_ray.h"
#include "ssa/world.h"

/* Native A4F4/A09C, including flags=3 entity and wall tracing. Dedicated
 * storage; stable world/map/math until free, no ownership of entities. */
typedef struct SsaWeaponAim {
    SsaWorld *world;
    const SsaMathTables *math;
    SsaRayWallCollector walls;
    SsaRayTraversal traversal;
    SsaRayOpening opening;
    SsaEntity **entities;
    size_t entity_count, entity_capacity;
    SsaEntity *source, *target;
    int32_t marker, top_slope, bottom_slope, shoot_z, range;
    int32_t start_x, start_y, end_x, end_y;
    unsigned updating;
} SsaWeaponAim;
/* Zero initialize. Init/free return 1/-1. Do not copy a live instance. */
int ssa_weapon_aim_init(SsaWeaponAim *, SsaWorld *, const SsaMathTables *);
int ssa_weapon_aim_free(SsaWeaponAim *);
/* Shared flags3 A240 collector used by aim and hitscan. Does not reset opening,
 * target, marker, slopes or other aim globals. line IDs below map.line_count
 * are walls; other IDs index entities[id-line_count], valid during callback.
 * Callback must not mutate/reenter world or query workspace. Return 1 continue,
 * 0 block, -1 error. Trace returns same convention; prior effects persist.
 * World query lock is held only for the call; perform gameplay effects after.
 * Repeated cells preserve original repeated entity intercepts, with wall stamps.
 */
int ssa_weapon_aim_trace(SsaWeaponAim *, int32_t x, int32_t y,
    int32_t end_x, int32_t end_y, SsaRayPredicate, void *context);
/* Query returns 1/-1, writing slope=0 for no target. Start distances and range
 * are fixed16.16; the original truncates each to whole units before trig.
 * marker and traversal.blocking_fraction retain stale values when no matching
 * original write occurs. Caller may seed them before a set of aim retries.
 * Original debug start_y is end_y, although the actual trace starts elsewhere.
 * World lists must not change during the synchronous call. No callbacks.
 * Failure retains earlier query effects but restores world query lock. */
int ssa_weapon_aim_query(SsaWeaponAim *, SsaEntity *source,
    int32_t start_x_distance, int32_t start_y_distance, int32_t z_offset,
    uint16_t heading, int32_t range, int32_t *slope);
#endif
