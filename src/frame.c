#include "ssa/frame.h"

int ssa_frame_step(SsaFrame *f, const SsaFrameOps *ops)
{
    int result = -1;
    if (!f || f->updating || !ops || !ops->stage) return -1;
    f->updating = 1;
#define STAGE(s) do { if (ops->stage(ops->context, f, (s)) < 0) goto done; } while (0)
    STAGE(SSA_FRAME_POLL);
    f->input = f->processed_input;
    if (!f->game) goto done;
    f->game->counter++;
    STAGE(SSA_FRAME_DISTRIBUTE);
    STAGE(SSA_FRAME_CONTROL);
    if (!f->game) goto done;
    if (!(f->game->flags & 1u)) {
        f->game->elapsed += f->game->ticks;
        if (f->game->elapsed > UINT32_C(0x57E04))
            f->game->elapsed = UINT32_C(0x57E04);
        STAGE(SSA_FRAME_ENTITIES);
        STAGE(SSA_FRAME_MOVERS);
        STAGE(SSA_FRAME_SCRIPTS);
        STAGE(SSA_FRAME_PICKUP_RESPAWN);
        STAGE(SSA_FRAME_ENTITY_ANIMATION);
    }
    STAGE(SSA_FRAME_SPRITE_CONTROLLERS);
    STAGE(SSA_FRAME_SPRITE_ANIMATION);
    if (f->local_index >= 4 || !f->player_flags[f->local_index]) goto done;
    if (!(*f->player_flags[f->local_index] & 8u)) {
        STAGE(SSA_FRAME_PREPARE_SPRITES);
        STAGE(SSA_FRAME_BUILD_OAM);
    }
    f->presentation_latch = 16;
    result = 0;
done:
    f->updating = 0;
    return result;
#undef STAGE
}
