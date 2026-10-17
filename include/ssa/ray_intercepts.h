#ifndef SSA_RAY_INTERCEPTS_H
#define SSA_RAY_INTERCEPTS_H
#include "ssa/map_assets.h"
#include "ssa/ray_traversal.h"

/* Independent wall-visit storage for 0800975C/08009A88. Map layout and storage
 * must remain stable until free. No entity intercepts are collected. */
typedef struct SsaRayWallCollector {
    const SsaMapAssets *map;
    uint8_t *visited;
    size_t capacity;
    unsigned updating;
} SsaRayWallCollector;
/* Initialize zeroed storage once; free before reuse. All return 1/-1. */
int ssa_ray_walls_init(SsaRayWallCollector *collector, const SsaMapAssets *map);
int ssa_ray_walls_free(SsaRayWallCollector *collector);
/* Clear visited lines before each traversal. Does not clear traversal hits. */
int ssa_ray_walls_begin(SsaRayWallCollector *collector);
/* Signature directly matches the traversal's cell callback. Out-of-map cells
 * succeed without effects. Duplicate walls are visited once until begin().
 * Errors retain earlier visited flags and appended hits. */
int ssa_ray_walls_cell(void *collector, SsaRayTraversal *traversal,
                       int32_t cell_x, int32_t cell_y);
/* Standalone 09A88 collection plus the iterator's visit deduplication. */
int ssa_ray_walls_line(SsaRayWallCollector *collector,
                       SsaRayTraversal *traversal, unsigned line);
#endif
