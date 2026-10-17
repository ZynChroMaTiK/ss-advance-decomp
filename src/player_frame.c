#include "ssa/player_frame.h"

static int signed_timer(uint16_t value)
{
    return value <= UINT16_C(32767) ? (int)value : (int)value - 65536;
}

int ssa_player_starts_moving(const SsaMovement *movement)
{
    return movement && (movement->keys & UINT32_C(0x3c0)) != 0 &&
           (movement->flags & UINT16_C(0x10)) == 0;
}

int ssa_player_stops_moving(const SsaMovement *movement)
{
    return movement && (movement->keys & UINT32_C(0x3c0)) == 0 &&
           (movement->flags & UINT16_C(0x10)) == 0;
}

int ssa_player_frame_step(SsaPlayerFrame *player, SsaPlayerFrameGame *game,
                           const SsaPlayerFrameOps *ops)
{
    SsaEntity *entity;
    enum SsaPlayerFrameStage stage;
    if (!player || !player->entity || !player->movement || !game || !ops ||
        !ops->stage || !ops->sector || !ops->animation)
        return -1;
    entity = player->entity;
    if (game->player_count > 1 && (entity->body.flags & UINT16_C(0x80))) {
        player->timer = (uint16_t)(player->timer - game->ticks);
        if (signed_timer(player->timer) <= 0) {
            if (game->protected_player != entity->player_index)
                entity->body.flags &= UINT16_C(0xff7f);
            entity->body.flags |= UINT16_C(2);
        }
    }
    for (stage = SSA_PLAYER_FRAME_DEATH; stage <= SSA_PLAYER_FRAME_CAMERA;
         stage = (enum SsaPlayerFrameStage)(stage + 1)) {
        if (ops->stage(ops->context, stage, player, game) < 0)
            return -1;
    }
    if (entity->player_index == game->local_player) {
        game->view.x = player->x;
        game->view.y = player->y;
        game->view.z = player->eye_z;
        game->view.heading = player->movement->heading;
        game->view.sector = (int16_t)entity->sector;
    }
    if (ops->stage(ops->context, SSA_PLAYER_FRAME_HAZARDS, player, game) < 0 ||
        ops->sector(ops->context, (int16_t)entity->sector) < 0)
        return -1;
    if (signed_timer(player->timer) > 0) {
        player->timer = (uint16_t)(player->timer - game->ticks);
        if (signed_timer(player->timer) <= 0) {
            player->timer = 0;
            entity->body.flags &= UINT16_C(0xfffe);
        }
    }
    if (player->animation_state == 0 &&
        ssa_player_starts_moving(player->movement)) {
        player->animation_state = 1;
        if (ops->animation(ops->context, player, 0x66, 4) < 0)
            return -1;
    } else if (player->animation_state == 1 &&
               ssa_player_stops_moving(player->movement)) {
        player->animation_state = 0;
        if (ops->animation(ops->context, player, 0x65, 4) < 0)
            return -1;
    }
    return 0;
}
