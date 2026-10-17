#include "ssa/player_runtime.h"
#include <limits.h>
#include <string.h>

typedef struct RuntimeStep {
    SsaPlayerRuntime *runtime;
    const SsaPlayerRuntimeInput *input;
    const SsaPlayerRuntimeOps *ops;
} RuntimeStep;

static int32_t bits(uint32_t value)
{
    return value <= INT32_MAX ? (int32_t)value : -1 - (int32_t)~value;
}

static void synchronize(SsaPlayerRuntime *r)
{
    SsaPlayerSpawn *p = r->player;
    p->entity_health = p->inventory.health;
    p->vertical.flags = p->motion.movement.flags;
    r->frame.x = p->motion.x;
    r->frame.y = p->motion.y;
    r->frame.eye_z = r->camera.eye_z;
}

int ssa_player_runtime_init(SsaPlayerRuntime *r, SsaPlayerSpawn *player,
                             SsaWorld *world, SsaSlide *slide,
                             const SsaMathTables *tables)
{
    SsaMapCollision collision = {0};
    if (!r || r->player || r->view_collision.map || r->updating || !player ||
        !player->active || !world || world->query_active || !slide || !tables ||
        player->entity.owner != world || slide->map != world->collision.map ||
        player->motion.x != player->entity.body.x ||
        player->motion.y != player->entity.body.y)
        return -1;
    if (!ssa_map_collision_init(&collision, world->collision.map))
        return -1;
    memset(r, 0, sizeof(*r));
    r->player = player;
    r->world = world;
    r->slide = slide;
    r->tables = tables;
    r->view_collision = collision;
    r->frame.entity = &player->entity;
    r->frame.movement = &player->motion.movement;
    r->camera.eye_z = bits((uint32_t)player->entity.body.z +
                           (uint32_t)player->vertical.eye_offset);
    player->inventory.health = player->entity_health;
    ssa_jump_init(&r->jump);
    ssa_view_init(&r->view);
    synchronize(r);
    (void)ssa_view_pose(&r->view, &player->entity.body, &r->camera,
                       player->motion.movement.heading, tables,
                       &r->view_collision, &r->pose);
    return 0;
}

int ssa_player_runtime_free(SsaPlayerRuntime *r)
{
    if (!r || r->updating)
        return -1;
    ssa_map_collision_free(&r->view_collision);
    memset(r, 0, sizeof(*r));
    return 0;
}

int ssa_player_runtime_key(SsaPlayerRuntime *r, int key, int down, int repeat,
                            int controls_enabled)
{
    if (!r || !r->player || r->updating)
        return 0;
    return ssa_view_key(&r->view, key, down, repeat, controls_enabled);
}

void ssa_player_runtime_focus_lost(SsaPlayerRuntime *r)
{
    if (r && r->player && !r->updating)
        ssa_view_focus_lost(&r->view);
}

static int stage(void *context, enum SsaPlayerFrameStage which,
                 SsaPlayerFrame *frame, SsaPlayerFrameGame *game)
{
    RuntimeStep *step = context;
    SsaPlayerRuntime *r = step->runtime;
    SsaPlayerSpawn *p = r->player;
    int result;
    (void)frame;
    synchronize(r);
    if (which == SSA_PLAYER_FRAME_MOVEMENT) {
        SsaMotionFrame motion = step->input->motion;
        SsaMotionOptions options = {&r->jump, step->input->jump_down,
                                    step->input->controls_enabled, 1};
        motion.ticks = game->ticks;
        motion.player_count = game->player_count;
        motion.local_player = game->protected_player;
        if (!motion.player_count || motion.player_count > 4 ||
            motion.local_player >= motion.player_count ||
            motion.slot >= motion.player_count)
            return -1;
        result = ssa_player_motion_world_step_pc(&p->motion, &p->entity, &p->vertical,
                    r->world, r->slide, r->tables, &motion, &options);
    } else if (which == SSA_PLAYER_FRAME_CAMERA) {
        int sound;
        r->camera_frame.counter = step->input->camera_counter;
        r->camera_frame.is_local = (uint8_t)(p->entity.player_index == game->local_player);
        r->camera_frame.footsteps_muted = step->input->footsteps_muted;
        sound = ssa_camera_step(&p->entity.body, &p->vertical, &r->camera,
                                &r->camera_frame, r->tables);
        synchronize(r);
        result = sound ? step->ops->sound(step->ops->context, r, (unsigned)sound) : 0;
    } else {
        result = step->ops->gameplay(step->ops->context, r, which);
    }
    synchronize(r);
    return result < 0 ? -1 : 0;
}

static int sector(void *context, int16_t id)
{
    RuntimeStep *step = context;
    int result = step->ops->sector(step->ops->context, step->runtime, id);
    synchronize(step->runtime);
    return result < 0 ? -1 : 0;
}

static int animation(void *context, SsaPlayerFrame *frame, unsigned id,
                     unsigned ticks)
{
    RuntimeStep *step = context;
    int result;
    (void)frame;
    step->runtime->player->animation = (uint16_t)id;
    step->runtime->player->animation_ticks = (uint8_t)ticks;
    result = step->ops->animation(step->ops->context, step->runtime, id, ticks);
    synchronize(step->runtime);
    return result < 0 ? -1 : 0;
}

int ssa_player_runtime_step(SsaPlayerRuntime *r, const SsaPlayerRuntimeInput *input,
                             const SsaPlayerRuntimeOps *ops)
{
    RuntimeStep context = {r, input, ops};
    SsaPlayerFrameOps frame_ops = {stage, sector, animation, &context};
    SsaPlayerSpawn *p;
    unsigned i;
    int result;
    if (!r || r->updating || !(p = r->player) || !p->active || !r->world ||
        r->world->query_active || p->entity.owner != r->world || !r->slide ||
        r->slide->map != r->world->collision.map || !r->tables ||
        r->view_collision.map != r->world->collision.map || !input || !ops ||
        !ops->gameplay || !ops->sector || !ops->animation || !ops->sound ||
        !input->motion.player_count || input->motion.player_count > 4 ||
        input->motion.local_player >= input->motion.player_count ||
        input->local_player >= input->motion.player_count ||
        input->motion.slot >= input->motion.player_count ||
        r->frame.entity != &p->entity || r->frame.movement != &p->motion.movement ||
        p->motion.x != p->entity.body.x || p->motion.y != p->entity.body.y)
        return -1;
    if (input->motion.player_count > 1) {
        if (!input->motion.players || input->motion.players[input->motion.slot] != &p->entity)
            return -1;
        for (i = 0; i < input->motion.player_count; ++i)
            if (!input->motion.players[i])
                return -1;
    }
    r->updating = 1;
    r->game.ticks = input->motion.ticks;
    r->game.player_count = input->motion.player_count;
    r->game.local_player = input->local_player;
    r->game.protected_player = input->motion.local_player;
    p->motion.movement.keys = input->controls_enabled ? input->keys : 0;
    if (input->controls_enabled) {
        int64_t pitch=(int64_t)r->pitch+input->pitch_delta;
        p->motion.movement.heading = (uint16_t)((uint32_t)p->motion.movement.heading +
                                               (uint32_t)input->yaw_delta);
        r->pitch=pitch>14564?14564:pitch< -14564?-14564:(int32_t)pitch;
        if(input->pitch_delta)r->free_aim=1;
    }
    synchronize(r);
    result = ssa_player_frame_step(&r->frame, &r->game, &frame_ops);
    synchronize(r);
    if (!result && !ssa_view_pose_pitch(&r->view, &p->entity.body, &r->camera,
            p->motion.movement.heading,r->pitch,r->tables,&r->view_collision,&r->pose))
        result = -1;
    r->updating = 0;
    return result;
}
