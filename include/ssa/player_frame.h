#ifndef SSA_PLAYER_FRAME_H
#define SSA_PLAYER_FRAME_H
#include "ssa/world.h"

/* Host view of the fields used by 08005CCC. This is not a complete player.
 * entity and movement must remain valid and unchanged as pointers throughout
 * callbacks. Sync cached position/eye and movement fields in stage adapters. */
typedef struct SsaPlayerFrame {
    SsaEntity *entity;
    SsaMovement *movement;
    int32_t x, y, eye_z;
    uint16_t timer;       /* entity +4E; shared by the two original countdowns */
    uint8_t animation_state; /* entity +3E, 0 idle / 1 moving */
} SsaPlayerFrame;

typedef struct SsaPlayerLocalView {
    int32_t x, y, z;
    uint16_t heading;
    int16_t sector;
} SsaPlayerLocalView;

typedef struct SsaPlayerFrameGame {
    SsaPlayerLocalView view;
    uint8_t ticks, player_count, protected_player, local_player;
} SsaPlayerFrameGame;

enum SsaPlayerFrameStage {
    SSA_PLAYER_FRAME_DEATH = 0, /* 08006154 */
    SSA_PLAYER_FRAME_MOVEMENT,  /* 080064B8 */
    SSA_PLAYER_FRAME_CAMERA,    /* 080062D4 */
    SSA_PLAYER_FRAME_HAZARDS    /* 08005680 */
};

typedef struct SsaPlayerFrameOps {
    int (*stage)(void *, enum SsaPlayerFrameStage, SsaPlayerFrame *,
                 SsaPlayerFrameGame *);
    int (*sector)(void *, int16_t sector);
    int (*animation)(void *, SsaPlayerFrame *, unsigned animation,
                     unsigned ticks);
    void *context;
} SsaPlayerFrameOps;

/* Original 08005C7C / 08005CA4 animation predicates. No mutation. */
int ssa_player_starts_moving(const SsaMovement *);
int ssa_player_stops_moving(const SsaMovement *);

/* Entire 08005CCC wrapper, with explicit stage/script/animation boundaries.
 * All callbacks are required; they return nonnegative success or negative
 * failure. -1 rejects invalid arguments or aborts on callback failure, leaving
 * earlier effects in place; 0 completes. Callbacks must not reenter this call,
 * replace/free state, or change entity/movement pointers. Stages may update
 * state and game values. Local-view copying occurs before hazards/sector calls.
 * This wrapper alone does not implement death, combat, scripts, or a game loop.
 * Protected-player and local-player indices are distinct original globals. */
int ssa_player_frame_step(SsaPlayerFrame *, SsaPlayerFrameGame *,
                           const SsaPlayerFrameOps *);
#endif
