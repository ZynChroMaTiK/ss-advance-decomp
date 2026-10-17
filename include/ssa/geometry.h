#ifndef SSA_GEOMETRY_H
#define SSA_GEOMETRY_H
#include "ssa/map_assets.h"
#include "ssa/physics.h"

/* All positions/bounds are signed 16.16; line directions/origins are map units. */
typedef struct SsaBounds { int32_t top, bottom, left, right; } SsaBounds;
int ssa_bsp_side(int32_t x, int32_t y, int32_t origin_x, int32_t origin_y,
                 int32_t dx, int32_t dy);
int ssa_line_side(int32_t x, int32_t y, int16_t origin_x, int16_t origin_y,
                  int16_t dx, int16_t dy);
int ssa_box_line_side(const SsaBounds *box, int16_t x, int16_t y,
                      int16_t dx, int16_t dy, uint16_t slope);
int ssa_map_leaf(const SsaMapAssets *map, int32_t x, int32_t y);
int ssa_map_sector(const SsaMapAssets *map, int32_t x, int32_t y);
typedef struct SsaPlayerStart {
    int32_t x, y, z;
    uint16_t heading;
    int16_t floor, ceiling;
} SsaPlayerStart;
/* First type-1 thing, original heading and BSP floor/ceiling. This recovers
 * position only; entity spawning, health, weapons and game state are external.
 * Returns 1 on success, 0 if absent/invalid; out is unchanged on failure. */
int ssa_map_player_start(const SsaMapAssets *map, SsaPlayerStart *out);

/* Map must remain loaded and its structural metadata/layout unchanged for the
 * lifetime of this workspace. Sector heights may change. No ownership of
 * map is transferred. Zero-initialize before init; free before reinitializing.
 * This query covers static walls/sector heights, not dynamic entity collision.
 * The special-line list is an ordered contact result, not event execution. */
typedef struct SsaMapCollision {
    const SsaMapAssets *map;
    uint8_t *visited;
    uint16_t *special_lines;
    size_t special_count;
    size_t line_capacity;
    int32_t floor, ceiling, low_floor;
    int32_t ceiling_line, blocking_line;
} SsaMapCollision;
int ssa_map_collision_init(SsaMapCollision *collision, const SsaMapAssets *map);
void ssa_map_collision_free(SsaMapCollision *collision);
/* Return 1 for clear, 0 for blocked, -1 for malformed/unusable geometry.
 * On 0, partial heights/contact list match original traversal ordering. */
int ssa_map_query(SsaMapCollision *collision, int32_t old_x, int32_t old_y,
                  int32_t x, int32_t y, int32_t radius);
/* Same query with an optional broadphase visitor between destination sector
 * setup and wall traversal (original entity-before-wall ordering). Visitor
 * returns 1 to continue, 0 to block, -1 on error; it must not reenter collision. */
int ssa_map_query_visit(SsaMapCollision *collision, int32_t old_x, int32_t old_y,
                        int32_t x, int32_t y, int32_t radius,
                        int (*visit)(void *, const SsaBounds *), void *context);
/* Clearance/commit phase only. Call immediately after a successful query on
 * this workspace. Does not change spatial links; callers own those updates. */
int ssa_map_commit_move(const SsaMapCollision *collision, SsaBody *body,
                        int32_t x, int32_t y);
/* Static-map part of 0x080043D0: query, clearance, 48-unit step, position commit.
 * Does not perform original spatial entity-list unlink/link side effects.
 * On failure position/heights stay unchanged; flag 4 can modify velocity_z. */
int ssa_map_try_move(SsaMapCollision *collision, SsaBody *body,
                     int32_t radius, int32_t x, int32_t y);
#endif
