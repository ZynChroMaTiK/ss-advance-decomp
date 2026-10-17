#include "ssa/enemy_cd.h"

typedef struct Call { SsaEnemyCd *cd; const SsaEnemyCdOps *ops; } Call;
static int emit(Call *c, SsaEnemyCdEvent event, SsaEntity *other,
                 int32_t a, int32_t b, int32_t d)
{
    return c->ops->event(c->ops->context, c->cd, event, other, a, b, d) < 0 ? -1 : 0;
}
static int lifecycle(void *context, const SsaEnemyActivation *view,
                      SsaEnemyLifecycleEvent event)
{
    Call *c = context;
    int result;
    (void)view;
    result = emit(c, event == SSA_ENEMY_LIFECYCLE_GROUP_RELEASE ?
        SSA_ENEMY_CD_GROUP_RELEASE : SSA_ENEMY_CD_DESTROY, NULL, 0, 0, 0);
    if (!result && event == SSA_ENEMY_LIFECYCLE_DESTROY) c->cd->destroy_requested = 1;
    return result;
}
static int animation(Call *c, unsigned id, unsigned period)
{
    c->cd->animation_done = 0;
    return emit(c, SSA_ENEMY_CD_ANIMATION, NULL, (int32_t)id, (int32_t)period, 0);
}
static int active(Call *c, SsaEnemyCdGame *g, const SsaMathTables *math,
                   unsigned ticks, int32_t speed, const SsaEnemyLifecycleOps *life)
{
    SsaEnemyCd *cd = c->cd;
    SsaEnemyActivation *a = cd->activation;
    SsaEnemyNavigation *n = cd->navigation;
    SsaEntity *e = a->entity, *target;
    SsaEntity player_pose = {0};
    SsaEnemyMotion motion = {e, n->speed, a->facing, n->desired_facing, 0};
    int32_t distance;
    int explode = 0;
    if (ssa_enemy_navigation_step(n, g->player, math, ticks, speed, c->ops->navigation) < 0)
        return -1;
    if (!n->move_result && n->phase == 2 && g->blocking_entity &&
        (g->blocking_entity->body.flags & 0x10) &&
        emit(c, SSA_ENEMY_CD_CONTACT_DAMAGE, g->blocking_entity, 20, 0, 0)) return -1;
    if (ssa_enemy_drain_step(a, cd->drain_period, &cd->drain_countdown, ticks)) return -1;
    if (e->body.flags & 1) {
        player_pose.body.x = g->player_x;
        player_pose.body.y = g->player_y;
        if (ssa_enemy_face(&motion, &player_pose, math)) return -1;
        n->desired_facing = motion.desired_facing;
        a->facing = motion.desired_facing;
        if (a->health <= 0) explode = 1;
        else {
            a->state = 1;
            e->body.flags &= UINT16_C(0xfffe);
            if (animation(c, 8, 6)) return -1;
        }
    }
    if (!explode) {
        target = (e->body.flags & 0x400) && n->target ? n->target : g->player;
        if (ssa_enemy_distance(e, target, math, &distance)) return -1;
        explode = (uint32_t)distance <= 127u;
    }
    if (explode) {
        a->state = 2;
        e->body.flags |= 0x20;
        if (animation(c, 0x1c, 6) || ssa_enemy_death_begin(a, life) ||
            emit(c, SSA_ENEMY_CD_SOUND, NULL, 0x27, 0, 0) ||
            emit(c, SSA_ENEMY_CD_EXPLOSION, NULL, 0x20000, 30, 1)) return -1;
    }
    return 0;
}
static int sound_postlude(Call *c, SsaEnemyCdGame *g)
{
    SsaEnemyCd *cd = c->cd;
    SsaEnemyNavigation *n = cd->navigation;
    SsaEntity *e = cd->activation->entity;
    unsigned phase = cd->destroy_requested ? 0 : n->phase;
    unsigned state = cd->destroy_requested ? 0 : cd->activation->state;
    int32_t distance = n->distance < 0x8000 ? n->distance : (int32_t)n->distance - 65536;
    if (phase == 2 && distance <= 1023 && distance && !g->loop_source && state != 2) {
        g->loop_source = e;
        return emit(c, SSA_ENEMY_CD_SOUND, NULL, 0x3a, 0, 0);
    }
    if (g->loop_source == e && (phase != 2 || distance > 1024 || state == 2)) {
        g->loop_source = NULL;
        return emit(c, SSA_ENEMY_CD_STOP_SOUND, NULL, 0x3a, 0, 0);
    }
    return 0;
}
int ssa_enemy_cd_step(SsaEnemyCd *cd, SsaEnemyCdGame *g, const SsaMathTables *math,
                       unsigned ticks, int32_t speed, const SsaEnemyCdOps *ops)
{
    Call call = {cd, ops};
    SsaEnemyLifecycleOps life = {lifecycle, &call};
    int result = 0;
    if (!cd || cd->updating || cd->destroy_requested || !cd->activation ||
        !cd->navigation || cd->activation->updating || cd->navigation->updating ||
        !cd->activation->entity || cd->navigation->entity != cd->activation->entity ||
        cd->activation->entity->type != 0xcd || !g || !g->activation || !g->player ||
        !math || ticks > 255 || !ops || !ops->event ||
        (cd->activation->entity->owner && cd->activation->entity->owner->query_active)) return -1;
    cd->updating = 1;
    switch (cd->activation->state) {
    case 0:
        result = ssa_enemy_activation_step(cd->activation, g->activation, NULL, ticks);
        break;
    case 1:
        result = active(&call, g, math, ticks, speed, &life);
        break;
    case 2:
        result = ssa_enemy_corpse_step(cd->activation, cd->animation_done, &life, ticks);
        break;
    default: break;
    }
    if (result >= 0) result = sound_postlude(&call, g);
    cd->updating = 0;
    return result < 0 ? -1 : cd->destroy_requested ? 1 : 0;
}
