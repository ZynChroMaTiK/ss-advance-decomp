#ifndef SSA_FRAME_H
#define SSA_FRAME_H
#include <stdint.h>

/* Host projections of the fields directly accessed by 0800244C. */
typedef struct SsaFrameGame {
    uint32_t counter;             /* GAME+00, increments even while paused */
    uint16_t flags;               /* GAME+04, bit 0 gates simulation */
    uint8_t ticks;                /* GAME+07 */
    uint32_t elapsed;             /* GAME+0C, wrap add then unsigned clamp */
} SsaFrameGame;

typedef enum SsaFrameStage {
    SSA_FRAME_POLL = 0x08003028,
    SSA_FRAME_DISTRIBUTE = 0x08005898,
    SSA_FRAME_CONTROL = 0x080021E4,
    SSA_FRAME_ENTITIES = 0x08004E04,
    SSA_FRAME_MOVERS = 0x0801C13C,
    SSA_FRAME_SCRIPTS = 0x0801C6DC,
    SSA_FRAME_PICKUP_RESPAWN = 0x0801B854,
    SSA_FRAME_ENTITY_ANIMATION = 0x08000908,
    SSA_FRAME_SPRITE_CONTROLLERS = 0x0800F85C,
    SSA_FRAME_SPRITE_ANIMATION = 0x0800E460,
    SSA_FRAME_PREPARE_SPRITES = 0x0800DBDC,
    SSA_FRAME_BUILD_OAM = 0x0800FCE4,
    /* Historical names retained for API compatibility. DBDC prepares sprite
     * visibility/cache; FCE4 emits hardware OAM. Neither is the world renderer. */
    SSA_FRAME_RENDER_WORLD = SSA_FRAME_PREPARE_SPRITES,
    SSA_FRAME_RENDER_SPRITES = SSA_FRAME_BUILD_OAM
} SsaFrameStage;

typedef struct SsaFrame {
    SsaFrameGame *game;
    uint32_t processed_input;     /* input030042F0+04, set by POLL */
    uint32_t input;               /*03003F84, copied immediately after POLL */
    uint16_t local_index;         /*0300464E, display-local, not GAME+68 */
    const uint16_t *player_flags[4]; /* player+BC; read after sprite animation */
    uint16_t presentation_latch;  /* [03003F00]+B02, final write is 16 */
    unsigned updating;
} SsaFrame;

typedef struct SsaFrameOps {
    int (*stage)(void *context, SsaFrame *frame, SsaFrameStage stage);
    void *context;
} SsaFrameOps;

/* Complete 0244C orchestration; every reached stage is mandatory. No callee is
 * silently replaced by a no-op. Callback returns <0 on failure, otherwise success.
 * Callbacks may update projections, including game/player pointer bindings; all
 * pointed storage must remain alive and valid. They must not change updating or
 * the ops storage. No nested step. Game is validated at its two use sites (after
 * POLL and CONTROL), player index/pointer only after SPRITE_ANIMATION. The pause
 * decision is made once; later flag changes do not truncate the selected stages.
 * Zero-initialize before first use. Returns 0 complete or -1 invalid/failure.
 * Failures retain prior effects, clear the guard, and do not write the final
 * presentation latch. The ROM has no defined return value or exit-level branch.
 */
int ssa_frame_step(SsaFrame *frame, const SsaFrameOps *ops);
#endif
