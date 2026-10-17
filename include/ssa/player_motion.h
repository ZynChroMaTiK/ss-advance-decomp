#ifndef SSA_PLAYER_MOTION_H
#define SSA_PLAYER_MOTION_H
#include "ssa/world.h"

typedef struct SsaPlayerMotion {
    SsaMovement movement;
    int32_t x, y, z, velocity_z;
    uint8_t facing;
} SsaPlayerMotion;
typedef struct SsaMotionFrame {
    const SsaEntity *const *players;
    uint8_t ticks, player_count, local_player, slot;
} SsaMotionFrame;
typedef struct SsaMotionOps {
    int (*horizontal)(void *, SsaEntity *); /* 0 success, -1 failure */
    void *context;
} SsaMotionOps;
/* NULL options selects original behavior. Native options are explicit gameplay
 * changes, not ROM matches. A non-NULL jump uses tick substeps; retain_heading
 * preserves the low eight heading bits discarded by the original body copy. */
typedef struct SsaMotionOptions {
    SsaJump *jump;
    uint8_t jump_down, controls_enabled, retain_heading;
} SsaMotionOptions;

/* Whole 080064B8, with a horizontal-physics boundary. Multiplayer push uses
 * player array order: last player within the original distance threshold wins.
 * players/slot must identify the current entity when local pushing is enabled.
 * Count is bounded to original four-player storage. All structures stay alive.
 * Player movement.flags is authoritative; vertical.flags is synchronized.
 * Returns 0/-1. Errors may follow controls/body/callback mutations; no rollback. */
int ssa_player_motion_step(SsaPlayerMotion *, SsaEntity *, SsaPlayerVertical *,
                            const SsaMathTables *, const SsaMotionFrame *,
                            const SsaMotionOps *, const SsaMotionOptions *);
/* Connects the entire routine to native dynamic collision and wall sliding.
 * Entity must be registered with matching player X/Y before entering, since
 * copying different coordinates would invalidate the world spatial links. */
int ssa_player_motion_world_step(SsaPlayerMotion *, SsaEntity *, SsaPlayerVertical *,
                                  SsaWorld *, SsaSlide *, const SsaMathTables *,
                                  const SsaMotionFrame *, const SsaMotionOptions *);
/* PC runtime entry: wide enemy broadphase and escape from existing enemy
 * overlap, including scripted spawns. Original world_step above is unchanged. */
int ssa_player_motion_world_step_pc(SsaPlayerMotion *,SsaEntity *,SsaPlayerVertical *,
                                  SsaWorld *,SsaSlide *,const SsaMathTables *,
                                  const SsaMotionFrame *,const SsaMotionOptions *);
#endif
