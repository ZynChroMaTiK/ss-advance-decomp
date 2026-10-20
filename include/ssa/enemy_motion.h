#ifndef SSA_ENEMY_MOTION_H
#define SSA_ENEMY_MOTION_H
#include "ssa/player_hazards.h"
typedef struct SsaEnemyMotion {
    SsaEntity *entity;
    int32_t speed;
    uint8_t facing, desired_facing;
    unsigned updating;
} SsaEnemyMotion;
typedef struct SsaEnemyMotionOps {
    /* Original 043D0 boundary. Return 1 moved, 0 blocked, -1 error.
     * May move this entity through world API; must not replace/free/reenter. */
    int (*move)(void *, SsaEnemyMotion *, int32_t x, int32_t y);
    void *context;
} SsaEnemyMotionOps;
typedef struct SsaEnemyCrowd {
    const SsaEntity *blocker; /* Direction preference only; never dereferenced. */
    SsaEntity *contact; /* First enemy hit in this move, even after a sidestep. */
    int side;
    unsigned axis,updating;
} SsaEnemyCrowd;
/* 0456C: desired heading from wrapped whole-unit self-minus-target XY. */
int ssa_enemy_face(SsaEnemyMotion *, const SsaEntity *target, const SsaMathTables *);
/* 04614: returns pre-turn delta via output; equality still decrements facing.
 * Native return 0/-1; ticks must fit original unsigned byte. */
int ssa_enemy_turn(SsaEnemyMotion *, unsigned ticks, int32_t *delta);
/* 045C4 (also 045EC): table-based distance, including signed wrap at 32768. */
int ssa_enemy_distance(const SsaEntity *, const SsaEntity *, const SsaMathTables *, int32_t *distance);
/* 04498: increment counter, attempt XY move, run vertical even when blocked.
 * Returns move result 0/1 or -1. Native callback error stops before vertical.
 * Reject player bodies: this routine is for enemy entities only. */
int ssa_enemy_walk(SsaEnemyMotion *, const SsaMathTables *, unsigned ticks,
                     uint32_t *walk_count, const SsaEnemyMotionOps *);
int ssa_enemy_walk_world(SsaEnemyMotion *, const SsaMathTables *, unsigned ticks,
                           uint32_t *walk_count, SsaWorld *);
/* PC extension: tangent sidesteps around enemies, with a persistent side to
 * avoid oscillating back into the crowd. Substeps are at most4 units per axis.
 * All commits use world collision; players/walls remain solid. Zero-init one
 * crowd state per enemy lifetime. Return1 moved,0 blocked,-1 error. */
int ssa_enemy_crowd_move(SsaEnemyCrowd *,SsaWorld *,SsaEntity *,int32_t x,int32_t y);
int ssa_enemy_walk_world_sliding(SsaEnemyMotion *,const SsaMathTables *,unsigned ticks,
    uint32_t *walk_count,SsaWorld *,SsaEnemyCrowd *);
/* 0354C: lethal floor material / moving-sector sample, only health changes. */
int ssa_enemy_hazards(const SsaEntity *, int16_t *health, const SsaMapAssets *,
                        const SsaPlayerHazardGame *);
#endif
