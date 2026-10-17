#include "ssa/player_lifecycle.h"
#include <string.h>

static void sync(SsaPlayerLifecycle *s)
{
    SsaPlayerSpawn *p = s->player;
    p->entity_health = p->inventory.health;
    p->vertical.flags = p->motion.movement.flags;
    s->weapon->keys = p->motion.movement.flags;
    p->motion.movement.weapon_flags = s->weapon->flags;
    s->frame->x = p->motion.x;
    s->frame->y = p->motion.y;
    s->frame->eye_z = s->camera->eye_z;
}

static int valid(const SsaPlayerLifecycle *s, const SsaPlayerLifecycleGame *g,
                 const SsaPlayerLifecycleOps *ops)
{
    return s && s->player && s->frame && s->camera && s->weapon &&
        s->weapon_view && g && ops && ops->event && ops->select_spawn &&
        s->frame->entity == &s->player->entity &&
        s->frame->movement == &s->player->motion.movement &&
        g->player_count >= 1 && g->player_count <= 4 &&
        g->local_player < g->player_count &&
        s->player->entity.player_index < g->player_count;
}

static int event(SsaPlayerLifecycle *s, SsaPlayerLifecycleGame *g,
                 const SsaPlayerLifecycleOps *ops,
                 enum SsaPlayerLifecycleEvent kind,
                 int32_t a, int32_t b, int32_t c, int32_t d)
{
    int result;
    sync(s);
    result = ops->event(ops->context, s, g, kind, a, b, c, d);
    if (!valid(s, g, ops)) return -1;
    sync(s);
    return result < 0 ? -1 : 0;
}

static int respawn(SsaPlayerLifecycle *s, SsaPlayerLifecycleGame *g,
                    const SsaPlayerLifecycleOps *ops)
{
    SsaPlayerSpawn *p = s->player;
    SsaEntity *e = &p->entity;
    SsaInventory saved_inventory = p->inventory;
    SsaWeaponControl saved_weapon = *s->weapon;
    SsaSpawnMarker marker = {0};
    uint8_t index = e->player_index;
    uint8_t facing = p->motion.facing;
    int16_t health = p->inventory.health;
    int result;
    memset(&p->motion, 0, sizeof(p->motion));
    p->motion.facing = facing; /* body +3F is not part of the cleared player */
    memset(&p->vertical, 0, sizeof(p->vertical));
    memset(&p->inventory, 0, sizeof(p->inventory));
    p->inventory.health = health;
    memset(s->camera, 0, sizeof(*s->camera));
    memset(s->weapon, 0, sizeof(*s->weapon));
    s->death_ticks = 0;
    if (event(s, g, ops, SSA_LIFECYCLE_UNLINK, 0, 0, 0, 0) < 0) return -1;
    result = ops->select_spawn(ops->context, s, g, &marker);
    if (!valid(s, g, ops)) return -1;
    sync(s);
    if (result < 0) return -1;
    p->motion.movement.heading = (uint16_t)((uint32_t)marker.heading << 8);
    p->motion.x = (int32_t)marker.x * 65536;
    p->motion.y = (int32_t)marker.y * 65536;
    /* The marker Z is overwritten by the body's old floor before insertion. */
    p->motion.z = (int32_t)e->body.floor * 65536;
    e->body.z = p->motion.z;
    p->motion.movement.flags |= 4;
    p->vertical.eye_offset = 96 * INT32_C(65536);
    e->type = (uint16_t)(0xc9 + index);
    e->player_index = index;
    e->radius = 32 * INT32_C(65536);
    e->body.height = 100 * INT32_C(65536);
    e->body.flags = 0x98;
    p->inventory.health = 100;
    s->frame->timer = 60;
    e->body.x = p->motion.x;
    e->body.y = p->motion.y;
    e->body.velocity_x = p->motion.movement.velocity_x;
    e->body.velocity_y = p->motion.movement.velocity_y;
    e->body.velocity_z = p->motion.velocity_z;
    p->motion.facing = (uint8_t)(p->motion.movement.heading >> 8);
    if (event(s, g, ops, SSA_LIFECYCLE_EFFECT, 5,
              e->body.x, e->body.y, e->body.z) < 0 ||
        event(s, g, ops, SSA_LIFECYCLE_ANIMATION, 0x65, 4, 0, 0) < 0 ||
        event(s, g, ops, SSA_LIFECYCLE_INSERT, 0, 0, 0, 0) < 0) return -1;
    /* Only player +28..A3 is restored, not health or +C4/+C6 stats. */
    saved_inventory.health = p->inventory.health;
    saved_inventory.stat_c4 = p->inventory.stat_c4;
    saved_inventory.stat_c6 = p->inventory.stat_c6;
    p->inventory = saved_inventory;
    *s->weapon = saved_weapon;
    s->weapon->flags = 0;
    p->inventory.weapons = 1;
    s->weapon_view->x = 0x7800;
    s->weapon_view->y = 0xa000;
    s->weapon_view->flags = 2;
    s->weapon_view->kind = 14;
    s->weapon_view->player_number = (uint8_t)(e->player_index + 1);
    p->inventory.pending_count = 0;
    p->inventory.ammo[0] = 999;
    s->weapon->requested = 1;
    s->weapon->current = 1;
    return event(s, g, ops, SSA_LIFECYCLE_WEAPON_ANIMATION, 0, 3, 0, 0);
}

static int death(SsaPlayerLifecycle *s, SsaPlayerLifecycleGame *g,
                  const SsaPlayerLifecycleOps *ops)
{
    SsaPlayerSpawn *p = s->player;
    uint16_t timer;
    if (!(p->motion.movement.flags & 0x10)) {
        if (p->inventory.health == 0) {
            p->motion.movement.flags |= 0x10;
            p->entity.body.flags |= 0x20;
            if (event(s, g, ops, SSA_LIFECYCLE_ANIMATION, 0x68, 8, 0, 0) < 0)
                return -1;
            if (g->continue_flag == 1 && g->player_count == 1) {
                g->continue_flag = 0;
                s->death_ticks = 500;
            } else {
                s->death_ticks = 0;
                if (p->entity.player_index == g->local_player) {
                    if (event(s, g, ops, SSA_LIFECYCLE_NOTICE, 0xb5, 0, 0x34, 0) < 0)
                        return -1;
                    g->message_ticks = 600;
                }
                if (event(s, g, ops, SSA_LIFECYCLE_SOUND, 17, 0, 0, 0) < 0)
                    return -1;
            }
        }
        if (!(p->motion.movement.flags & 0x10)) return 0;
    }
    if (p->vertical.eye_offset > 4 * INT32_C(65536))
        p->vertical.eye_velocity = -(int32_t)g->ticks * 131072;
    else
        p->entity.body.flags |= 0x40;
    timer = (uint16_t)(s->death_ticks + g->ticks);
    s->death_ticks = timer;
    if (timer > 590) {
        if (g->continue_flag == 0 && g->player_count == 1) {
            if (event(s, g, ops, SSA_LIFECYCLE_MODE, 10, 0, 0, 0) < 0 ||
                event(s, g, ops, SSA_LIFECYCLE_NOTICE, 0xb4, 0, 0x40, 0) < 0)
                return -1;
            g->message_ticks = 600;
            s->death_ticks = 0;
        }
    } else if ((uint16_t)(timer - 301) <= 98) {
        s->death_ticks = 0;
        if (g->player_count > 1) {
            g->message_ticks = 0;
            if (g->exit_flag) g->input_state = 3;
            else return respawn(s, g, ops);
        } else g->input_state = 4;
    }
    return 0;
}

static int run(SsaPlayerLifecycle *s, SsaPlayerLifecycleGame *g,
                 const SsaPlayerLifecycleOps *ops, int reset)
{
    int result;
    if (!valid(s, g, ops) || s->updating ||
        s->weapon->flags != s->player->motion.movement.weapon_flags) return -1;
    s->updating = 1;
    sync(s);
    result = reset ? respawn(s, g, ops) : death(s, g, ops);
    sync(s);
    s->updating = 0;
    return result;
}
int ssa_player_death_step(SsaPlayerLifecycle *s, SsaPlayerLifecycleGame *g,
                            const SsaPlayerLifecycleOps *ops)
{ return run(s, g, ops, 0); }
int ssa_player_respawn(SsaPlayerLifecycle *s, SsaPlayerLifecycleGame *g,
                         const SsaPlayerLifecycleOps *ops)
{ return run(s, g, ops, 1); }
