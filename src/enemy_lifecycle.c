#include "ssa/enemy_lifecycle.h"
#include <string.h>

static int valid(const SsaEnemyActivation *v)
{
    return v && v->entity && !v->updating &&
        (!v->entity->owner || !v->entity->owner->query_active);
}
static int16_t signed16(uint16_t bits)
{
    int16_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
static int32_t signed32(uint32_t bits)
{
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
int ssa_enemy_death_begin(SsaEnemyActivation *v, const SsaEnemyLifecycleOps *ops)
{
    int result;
    if (!valid(v) || !ops || !ops->event) return -1;
    v->updating = 1;
    v->health = 0;
    v->entity->body.flags &= UINT16_C(0xfffd);
    result = ops->event(ops->context, v, SSA_ENEMY_LIFECYCLE_GROUP_RELEASE);
    if (result >= 0)
        v->timer = v->entity->type == 0xd7 || v->entity->type == 0xd8 ? 720 : 360;
    v->updating = 0;
    return result < 0 ? -1 : 0;
}
int ssa_enemy_corpse_step(SsaEnemyActivation *v, unsigned animation_done,
                          const SsaEnemyLifecycleOps *ops, unsigned ticks)
{
    SsaBody *body;
    int32_t floor;
    int result = 0;
    if (!valid(v) || !ops || !ops->event || ticks > 255 ||
        animation_done > 255) return -1;
    v->updating = 1;
    body = &v->entity->body;
    body->z = signed32((uint32_t)body->z - ((uint32_t)ticks << 16));
    floor = (int32_t)body->floor * INT32_C(65536);
    if (body->z < floor) body->z = floor;
    if (animation_done)
        body->flags = (uint16_t)((body->flags & UINT16_C(0xfffe)) | 0x20);
    v->timer = (uint16_t)(v->timer - ticks);
    if (signed16(v->timer) <= 119) body->flags |= 0x40;
    if (signed16(v->timer) < 0) {
        v->timer = 0;
        body->flags &= UINT16_C(0xffbf);
        result = ops->event(ops->context, v, SSA_ENEMY_LIFECYCLE_DESTROY) < 0 ? -1 : 1;
    }
    v->updating = 0;
    return result;
}
int ssa_enemy_drain_step(SsaEnemyActivation *v, uint16_t period,
                         uint16_t *countdown, unsigned ticks)
{
    if (!valid(v) || !countdown || ticks > 255) return -1;
    if (!period) return 0;
    *countdown = (uint16_t)(*countdown - ticks);
    if (signed16(*countdown) < 0) {
        v->health = signed16((uint16_t)((uint16_t)v->health - 1));
        if (v->health <= 0) {
            *countdown = 0;
            v->entity->body.flags |= 1;
        } else *countdown = period;
    }
    return 0;
}
