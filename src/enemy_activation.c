#include "ssa/enemy_activation.h"
#include "ssa/entity_timer.h"

typedef struct Dimensions { int16_t height, radius, health; } Dimensions;
static const Dimensions dimensions[12] = {
    {120, 32, 30}, {140, 64, 100}, {152, 48, 50}, {90, 25, 40},
    {140, 50, 100}, {170, 70, 80}, {210, 48, 100}, {180, 56, 130},
    {66, 32, 60}, {138, 90, 150}, {186, 70, 3000}, {186, 70, 3000}
};

unsigned ssa_enemy_initial_health(unsigned type)
{return type>=0xcd && type<=0xd8?(unsigned)dimensions[type-0xcd].health:0;}

static int event(SsaEnemyActivation *view, SsaEnemyActivationGame *game,
                 const SsaEnemyActivationOps *ops, SsaEnemyActivationEvent kind,
                 int32_t a, int32_t b, int32_t c, int32_t d)
{
    return ops->event(ops->context, view, game, kind, a, b, c, d) < 0 ? -1 : 0;
}

int ssa_enemy_activation_step(SsaEnemyActivation *view,
                              SsaEnemyActivationGame *game,
                              const SsaEnemyActivationOps *ops, unsigned ticks)
{
    SsaEntity *entity;
    unsigned index;
    int result = 1;
    if (!view || !game || !view->entity || view->updating || view->state ||
        ticks > 255) return -1;
    entity = view->entity;
    if (entity->type < 0xcd || entity->type > 0xd8 ||
        (entity->owner && entity->owner->query_active)) return -1;
    index = (unsigned)entity->type - 0xcd;
    if ((index == 1 || index >= 10) && (!ops || !ops->event)) return -1;
    view->updating = 1;
    if (!ssa_entity_timer_step(&entity->body.flags, &view->timer, ticks)) {
        view->updating = 0;
        return 0;
    }
    game->spawned[index] += UINT32_C(1);
    if (index < 10) view->state = 1;
    entity->body.flags |= 2;
    entity->body.height = (int32_t)dimensions[index].height * INT32_C(65536);
    entity->radius = (int32_t)dimensions[index].radius * INT32_C(65536);
    view->health = dimensions[index].health;
    if (index == 3) entity->body.hover_offset = 48;
    if (index == 1) {
        if (event(view, game, ops, SSA_ENEMY_ACTIVATION_SOUND, 0x4d, 0, 0, 0))
            result = -1;
    } else if (index >= 10) {
        view->timer = 0;
        view->saved_facing = view->facing;
        game->boss_phase = 0;
        game->boss = entity;
        view->boss_ticks = 5;
        view->state = 1;
        if (event(view, game, ops, SSA_ENEMY_ACTIVATION_ANIMATION,
                  index == 10 ? 0x58 : 0x52, 10, 0, 0)) {
            result = -1;
        } else {
            entity->body.flags &= UINT16_C(0xefff);
            if (event(view, game, ops, SSA_ENEMY_ACTIVATION_MUSIC, 0x13, 0, 0, 0) ||
                event(view, game, ops, SSA_ENEMY_ACTIVATION_NOTICE,
                      index == 10 ? 0x72 : 0x73, 0, 48, 0)) result = -1;
        }
    }
    view->updating = 0;
    return result;
}
