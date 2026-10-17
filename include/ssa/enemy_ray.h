#ifndef SSA_ENEMY_RAY_H
#define SSA_ENEMY_RAY_H
#include "ssa/ray_intercepts.h"

typedef struct SsaRayOpening {
    int32_t top, bottom, low_floor, range;
} SsaRayOpening;
/* 9C98. A one-sided line clears only range; other fields retain their values.
 * Sector heights are read live. Returns 1/-1; invalid records leave state intact. */
int ssa_ray_opening(const SsaMapAssets *, unsigned line, SsaRayOpening *);
/* A040 wall branch. One-sided walls block without changing opening. This is
 * the navigation floor-step test, not a body-height or general sight test.
 * Return 1 clear, 0 blocked, -1 invalid. */
int ssa_enemy_ray_wall(const SsaMapAssets *, unsigned line, SsaRayOpening *);

typedef struct SsaEnemyRay {
    SsaRayWallCollector walls;
    SsaRayTraversal traversal;
    SsaRayOpening opening;
    int32_t origin_x, origin_y;
    unsigned updating;
} SsaEnemyRay;
/* Dedicated scratch, independent of world movement/blocker and camera queries.
 * Zero-init; stable map/storage until free; heights may change between calls.
 * init/free return 1/-1. Query returns 1 clear, 0 blocked, -1 error. */
int ssa_enemy_ray_init(SsaEnemyRay *, const SsaMapAssets *);
int ssa_enemy_ray_free(SsaEnemyRay *);
int ssa_enemy_ray_query(SsaEnemyRay *, int32_t x, int32_t y, int32_t end_x, int32_t end_y);
#endif
