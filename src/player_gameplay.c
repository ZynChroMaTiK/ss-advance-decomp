#include "ssa/player_gameplay.h"
#include <string.h>

typedef struct GameplayStep {
    SsaPlayerGameplay *g;
    const SsaPlayerRuntimeInput *input;
    const SsaPlayerGameplayOps *ops;
} GameplayStep;

int ssa_player_gameplay_init(SsaPlayerGameplay *g, SsaPlayerRuntime *r,
                              const SsaSpawnPlan *plan) {
    SsaMapCollision scratch = {0};
    if (!g || g->runtime || g->interaction.map || g->updating || !r ||
        !r->player || !r->world || r->updating || r->world->query_active ||
        r->player->entity.owner != r->world || !plan ||
        plan->map != r->world->collision.map) return -1;
    if (!ssa_map_collision_init(&scratch, plan->map)) return -1;
    memset(g, 0, sizeof(*g));
    g->runtime = r; g->plan = plan; g->interaction = scratch;
    g->lifecycle.player = r->player; g->lifecycle.frame = &r->frame;
    g->lifecycle.camera = &r->camera; g->lifecycle.weapon = &g->weapon;
    g->lifecycle.weapon_view = &g->weapon_view;
    g->weapon.flags = r->player->motion.movement.weapon_flags;
    return 0;
}

int ssa_player_gameplay_free(SsaPlayerGameplay *g) {
    if (!g || g->updating || (g->runtime && g->runtime->updating)) return -1;
    ssa_map_collision_free(&g->interaction);
    memset(g, 0, sizeof(*g));
    return 0;
}

static int life_event(void *context, SsaPlayerLifecycle *life,
                      SsaPlayerLifecycleGame *game, enum SsaPlayerLifecycleEvent kind,
                      int32_t a, int32_t b, int32_t c, int32_t d) {
    GameplayStep *s = context;
    SsaPlayerRuntime *r = s->g->runtime;
    int result;
    (void)life; (void)game;
    switch (kind) {
    case SSA_LIFECYCLE_UNLINK:
        return ssa_world_remove(r->world, &r->player->entity) ? 0 : -1;
    case SSA_LIFECYCLE_INSERT:
        result = ssa_world_insert(r->world, &r->player->entity) ? 0 : -1;
        if (!result) {
            /* Native-only state must not carry a stale pre-respawn bob or
             * synthesize a fresh jump from an already held button. */
            r->camera_frame.previous_bob = 0;
            r->jump.was_down = (uint8_t)!!s->input->jump_down;
        }
        return result;
    case SSA_LIFECYCLE_ANIMATION:
        r->player->animation = (uint16_t)a;
        r->player->animation_ticks = (uint8_t)b;
        return s->ops->animation(s->ops->context, r, (unsigned)a, (unsigned)b);
    case SSA_LIFECYCLE_SOUND:
        return s->ops->sound(s->ops->context, r, (unsigned)a);
    default:
        return s->ops->event(s->ops->context, s->g, kind, a, b, c, d);
    }
}

static int select_spawn(void *context, SsaPlayerLifecycle *life,
                        SsaPlayerLifecycleGame *game, SsaSpawnMarker *out) {
    GameplayStep *s = context;
    SsaRespawnPosition positions[4];
    unsigned i;
    if (!game->player_count || game->player_count > 4 ||
        game->player_count != s->input->motion.player_count) return -1;
    for (i = 0; i < game->player_count; ++i) {
        const SsaEntity *e;
        if (i == s->input->motion.slot) {
            /* Original clears this player's coordinates before selection. */
            positions[i].x = life->player->motion.x;
            positions[i].y = life->player->motion.y;
            continue;
        }
        if (!s->input->motion.players || !(e = s->input->motion.players[i])) return -1;
        positions[i].x = e->body.x; positions[i].y = e->body.y;
    }
    return ssa_respawn_select(out, s->g->plan->player_starts,
        s->g->plan->player_count, positions, game->player_count,
        s->g->runtime->tables) == 1 ? 0 : -1;
}

static int line_event(void *context, SsaPlayerHazards *hazards,
                      unsigned kind, uint16_t line) {
    GameplayStep *s = context;
    (void)hazards;
    return s->ops->line(s->ops->context, s->g, kind, line);
}

static int gameplay(void *context, SsaPlayerRuntime *r, enum SsaPlayerFrameStage stage) {
    GameplayStep *s = context;
    SsaPlayerGameplay *g = s->g;
    if (stage == SSA_PLAYER_FRAME_DEATH) {
        SsaPlayerLifecycleOps ops = {life_event, select_spawn, s};
        int result;
        g->life_game.ticks = r->game.ticks;
        g->life_game.player_count = r->game.player_count;
        g->life_game.local_player = r->game.local_player;
        result = ssa_player_death_step(&g->lifecycle, &g->life_game, &ops);
        r->game.ticks = g->life_game.ticks;
        r->game.player_count = g->life_game.player_count;
        r->game.local_player = g->life_game.local_player;
        return result;
    } else {
        SsaPlayerHazards h = {&r->player->entity, &r->player->motion.movement,
                              &r->player->inventory, g->weapon_action,
                              r->player->motion.facing};
        SsaPlayerHazardOps ops = {line_event, s};
        if (stage != SSA_PLAYER_FRAME_HAZARDS) return -1;
        if (g->display_flags)
            g->hazard_game.local_flags = *g->display_flags;
        else if (r->game.local_player == r->player->entity.player_index)
            g->hazard_game.local_flags = r->player->motion.movement.flags;
        return g->immediate_use ?
            ssa_player_hazards_step_immediate_use(&h,&g->interaction,r->tables,&g->hazard_game,&ops) :
            ssa_player_hazards_step(&h,&g->interaction,r->tables,&g->hazard_game,&ops);
    }
}
static int sector(void *context, SsaPlayerRuntime *r, int16_t id) {
    GameplayStep *s = context;
    return s->ops->sector(s->ops->context, r, id);
}
static int animation(void *context, SsaPlayerRuntime *r, unsigned id, unsigned ticks) {
    GameplayStep *s = context;
    return s->ops->animation(s->ops->context, r, id, ticks);
}
static int sound(void *context, SsaPlayerRuntime *r, unsigned id) {
    GameplayStep *s = context;
    return s->ops->sound(s->ops->context, r, id);
}

int ssa_player_gameplay_step(SsaPlayerGameplay *g, const SsaPlayerRuntimeInput *input,
                              const SsaPlayerGameplayOps *ops) {
    GameplayStep context = {g, input, ops};
    SsaPlayerRuntimeOps runtime_ops = {gameplay, sector, animation, sound, &context};
    SsaPlayerRuntime *r;
    int result;
    if (!g || g->updating || g->immediate_use>1 || !(r = g->runtime) || r->updating || !r->player ||
        !r->world || !g->plan || g->plan->map != r->world->collision.map ||
        g->interaction.map != g->plan->map || !input || !ops || !ops->event ||
        !ops->line || !ops->sector || !ops->animation || !ops->sound ||
        g->lifecycle.player != r->player || g->lifecycle.frame != &r->frame ||
        g->lifecycle.camera != &r->camera || g->lifecycle.weapon != &g->weapon ||
        g->lifecycle.weapon_view != &g->weapon_view) return -1;
    g->updating = 1;
    g->weapon.flags = r->player->motion.movement.weapon_flags;
    result = ssa_player_runtime_step(r, input, &runtime_ops);
    g->weapon.keys = r->player->motion.movement.flags;
    g->weapon.flags = r->player->motion.movement.weapon_flags;
    g->updating = 0;
    return result;
}
