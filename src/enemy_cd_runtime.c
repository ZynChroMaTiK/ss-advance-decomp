#include "ssa/enemy_cd_runtime.h"
#include <string.h>

static int ready(const SsaScriptSpawn *s)
{
    return s && s->level && s->level->map && s->world && s->world->cells &&
        !s->updating && !s->level->updating && !s->level->script.updating &&
        !s->level->movers.updating && !s->level->switches.updating &&
        !s->world->query_active && s->world->collision.map == s->level->map;
}
int ssa_enemy_cd_runtime_init(SsaEnemyCdRuntime *r, SsaScriptSpawn *s, unsigned slot,
                               const SsaMathTables *math, SsaEnemyCdGame *g, uint32_t *count)
{
    SsaScriptActor *a;
    if (!r || r->active || r->updating || !ready(s) || slot >= 100 || !math ||
        !g || g->activation != &s->game || !g->player || !count) return -1;
    a = &s->actors[slot];
    if (!s->pool.occupied[slot] || !a->owned || a->entity.owner != s->world ||
        a->entity.type != 0xcd || a->activation.state || a->activation.updating) return -1;
    memset(r, 0, sizeof(*r));
    r->spawn = s; r->math = math; r->game = g; r->walk_count = count;
    r->slot = slot; r->active = 1;
    r->navigation.entity = &a->entity;
    r->navigation.heading = a->activation.saved_facing;
    r->controller.activation = &a->activation;
    r->controller.navigation = &r->navigation;
    r->controller.animation_done = a->animation_flags;
    r->controller.drain_period = a->parameter34;
    r->controller.drain_countdown = a->parameter36;
    return 0;
}
typedef struct RuntimeCall {
    SsaEnemyCdRuntime *r;
    const SsaEnemyCdFrame *frame;
    const SsaEnemyCdRuntimeOps *ops;
    SsaPlayerHazardGame hazards;
} RuntimeCall;
static int visibility(void *context, int player, int enemy, int *blocked)
{
    RuntimeCall *c = context;
    return c->ops->queries->visibility(c->ops->queries->context, player, enemy, blocked);
}
static int nearest(void *context, SsaEntity *source, SsaEntity **out)
{
    RuntimeCall *c = context;
    return c->ops->queries->nearest(c->ops->queries->context, source, out);
}
static int path(void *context, const SsaEnemyNavigation *n, int32_t x0, int32_t y0,
                 int32_t x1, int32_t y1, int32_t radius, unsigned special,
                 unsigned mode, int *mask, int32_t *dx, int32_t *dy)
{
    RuntimeCall *c = context;
    return c->ops->queries->path(c->ops->queries->context, n, x0, y0, x1, y1,
        radius, special, mode, mask, dx, dy);
}
static SsaEnemyMotion motion(SsaEnemyCdRuntime *r)
{
    SsaEnemyMotion m = {r->navigation.entity, r->navigation.speed,
        r->controller.activation->facing, r->navigation.desired_facing, 0};
    return m;
}
static int turn(void *context, SsaEnemyNavigation *n, int32_t rate, int32_t *delta)
{
    RuntimeCall *c = context;
    SsaEnemyMotion m = motion(c->r);
    (void)n;
    if (rate != 65536 || ssa_enemy_turn(&m, c->frame->ticks, delta)) return -1;
    c->r->controller.activation->facing = m.facing;
    return 0;
}
static int move(void *context, SsaEnemyNavigation *n, int32_t *result)
{
    RuntimeCall *c = context;
    SsaEnemyMotion m = motion(c->r);
    int moved;
    (void)n;
    moved = c->r->crowd_slide?
        ssa_enemy_walk_world_sliding(&m,c->r->math,c->frame->ticks,
            c->r->walk_count,c->r->spawn->world,&c->r->crowd):
        ssa_enemy_walk_world(&m,c->r->math,c->frame->ticks,c->r->walk_count,c->r->spawn->world);
    c->r->game->blocking_entity = c->r->spawn->world->blocking_entity;
    if (moved < 0) return -1;
    *result = moved;
    return 0;
}
static int finish(void *context, SsaEnemyNavigation *n)
{
    RuntimeCall *c = context;
    return ssa_enemy_hazards(n->entity, &c->r->controller.activation->health,
        c->r->spawn->level->map, &c->hazards);
}
static int event(void *context, const SsaEnemyCd *cd, SsaEnemyCdEvent kind,
                  SsaEntity *other, int32_t a, int32_t b, int32_t d)
{
    RuntimeCall *c = context;
    SsaScriptActor *actor = &c->r->spawn->actors[c->r->slot];
    actor->parameter34 = cd->drain_period;
    actor->parameter36 = cd->drain_countdown;
    if (kind == SSA_ENEMY_CD_GROUP_RELEASE) {
        if (actor->group_released || !ssa_level_script_release(&c->r->spawn->level->script,
            actor->script.trigger)) return -1;
        actor->group_released = 1;
        return 0;
    }
    if (kind == SSA_ENEMY_CD_DESTROY) return 0; /* deferred until controller returns */
    if (kind == SSA_ENEMY_CD_ANIMATION) {
        actor->animation = (uint16_t)a;
        actor->animation_ticks = (uint8_t)b;
        actor->animation_frame = actor->animation_counter = actor->animation_flags = 0;
    }
    return c->ops->event(c->ops->context, cd, kind, other, a, b, d);
}
int ssa_enemy_cd_runtime_step(SsaEnemyCdRuntime *r, const SsaEnemyCdFrame *frame,
                               const SsaEnemyCdRuntimeOps *ops)
{
    RuntimeCall c = {r, frame, ops, {0}};
    SsaEnemyNavigationOps nav = {visibility, nearest, path, turn, move, finish, &c};
    SsaEnemyCdOps adapters = {&nav, event, &c};
    SsaScriptActor *a;
    unsigned i;
    int result;
    if (!r || !r->active || r->updating || r->crowd_slide>1 || !ready(r->spawn) || r->slot >= 100 ||
        !frame || frame->ticks > 255 || !ops || !ops->event || !ops->queries ||
        !ops->queries->visibility || !ops->queries->nearest || !ops->queries->path ||
        !r->game || r->game->activation != &r->spawn->game || !r->game->player ||
        !r->walk_count || !r->math || r->spawn->level->movers.hazard_count > 8) return -1;
    a = &r->spawn->actors[r->slot];
    if (!a->owned || !r->spawn->pool.occupied[r->slot] ||
        r->controller.updating || r->navigation.updating || a->activation.updating ||
        a->entity.owner != r->spawn->world || r->controller.activation != &a->activation ||
        r->controller.navigation != &r->navigation || r->navigation.entity != &a->entity) return -1;
    c.hazards.frame_counter = frame->frame_counter;
    c.hazards.level_counter = frame->level_counter;
    c.hazards.hazard_count = r->spawn->level->movers.hazard_count;
    for (i = 0; i < c.hazards.hazard_count; ++i)
        r->spawn->level->hazard_sectors[i] = (int32_t)r->spawn->level->movers.hazard_sectors[i];
    c.hazards.hazard_sectors = r->spawn->level->hazard_sectors;
    r->updating = r->spawn->updating = 1;
    /* 12638 reads the shared latest-query blocker even when navigation skips
     * movement (for example, its visibility early exit). */
    r->game->blocking_entity = r->spawn->world->blocking_entity;
    r->controller.animation_done = a->animation_flags;
    /* 10A20 type69 sets original +34 directly. Preserve the separate +36
     * countdown: conversion does not reset it or stop enemy navigation. */
    r->controller.drain_period = a->parameter34;
    r->controller.drain_countdown = a->parameter36;
    result = ssa_enemy_cd_step(&r->controller, r->game, r->math,
        frame->ticks, frame->speed_factor, &adapters);
    a->script.flags = a->entity.body.flags;
    a->script.timer = a->activation.timer;
    a->script.variant = a->activation.state;
    a->script.facing = a->activation.facing;
    a->animation_flags = r->controller.animation_done;
    a->parameter34 = r->controller.drain_period;
    a->parameter36 = r->controller.drain_countdown;
    r->spawn->updating = 0;
    if (r->controller.destroy_requested) {
        if (ssa_script_spawn_release(r->spawn, r->slot)) result = -1;
        else r->active = 0;
    }
    r->updating = 0;
    return result;
}
