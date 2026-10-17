#include "ssa/projectile_controller.h"

static int emit(SsaProjectileController *c, const SsaProjectileControllerOps *o,
                SsaProjectileEvent event, int32_t a, int32_t b, int32_t d)
{
    return o->event(o->context, c, event, a, b, d) < 0 ? -1 : 0;
}
static int destroy(SsaProjectileController *c, const SsaProjectileControllerOps *o)
{
    if (emit(c, o, SSA_PROJECTILE_DESTROY, 0, 0, 0)) return -1;
    c->destroy_requested = 1;
    return 1;
}
int ssa_projectile_controller_step(SsaProjectileController *c,
    SsaProjectileControllerGame *g, const SsaProjectileControllerOps *o)
{
    SsaProjectile *p;
    unsigned animation, damage, particle, type;
    int result = 0;
    if (!c || c->updating || c->destroy_requested || !c->projectile ||
        c->projectile->updating || !c->projectile->entity || !g || !o ||
        !o->motion || !o->event) return -1;
    p = c->projectile;
    if (p->entity->owner && p->entity->owner->query_active) return -1;
    type = p->entity->type;
    switch (type) {
    case 0x65: animation = 0x0f; damage = 100; particle = 4; break;
    case 0x66: animation = 0x44; damage = 5; particle = 4; break;
    case 0x67: animation = 0x5a; damage = 100; particle = 4; break;
    case 0x69: animation = 0x5f; damage = 0; particle = 0; break;
    case 0x6b: animation = 0x87; damage = 5; particle = 15; break;
    default: return -1;
    }
    c->updating = 1;
    if (p->state > 1) goto done;
    if (!p->state) {
        p->animation = (uint16_t)animation;
        p->animation_period = 5;
        p->animation_done = 0;
        if (emit(c, o, SSA_PROJECTILE_ANIMATION, (int32_t)animation, 5, 0)) {
            result = -1; goto done;
        }
        p->state = 1;
        if (type == 0x67) p->health = 160;
    }
    if ((type != 0x65 && type != 0x67) || p->speed) {
        result = o->motion(o->context, p);
        if (result < 0) { result = -1; goto done; }
        if (result) { result = 0; goto done; }
    }
    if (type != 0x69) {
        g->credit = p->owner;
        g->hit_weapon = p->weapon;
        result = emit(c, o, SSA_PROJECTILE_EXPLOSION, 0x20000, (int32_t)damage, 1);
        g->credit = g->hit_weapon = 0;
        if (result) goto done;
        result = emit(c, o, SSA_PROJECTILE_PARTICLE, (int32_t)particle, 0, 0);
        if (result) goto done;
    }
    result = destroy(c, o);
done:
    c->updating = 0;
    return result;
}
