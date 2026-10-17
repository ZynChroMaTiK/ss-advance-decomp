#include "ssa/combat_damage.h"
#include <string.h>

static int16_t signed16(uint16_t bits)
{
    int16_t out;
    memcpy(&out, &bits, sizeof(out));
    return out;
}
static void armor(int16_t *value, uint32_t *damage)
{
    if (*value) {
        /* ASR(damage <<17,16) takes the signed low halfword of damage*2. */
        *value = signed16((uint16_t)((uint16_t)*value -
                         (uint32_t)(int32_t)signed16((uint16_t)(*damage << 1))));
        *damage >>= 1;
        if (*value < 0) {
            *damage -= (uint32_t)(int32_t)(*value / 2);
            *value = 0;
        }
    }
}
int ssa_combat_damage(SsaCombatDamage *g, SsaCombatDamageTarget *v,
                       uint32_t damage, const SsaCombatDamageOps *ops)
{
    SsaEntity *e;
    const SsaEntity *source;
    int result = 0;
    unsigned index;
    if (!g || g->updating || !ops || !ops->event) return -1;
    if (!v) return 0;
    if (!(e = v->entity) || !v->health) return -1;
    if ((e->body.flags & 0x10) && (!v->timer || !v->inventory)) return -1;
    if ((e->body.flags & 0x10) && !(e->body.flags & 1) &&
        g->player_count <= 1 && !g->source) return -1;
    if (g->player_count > 1 && g->credit_player && !g->credited_kills) return -1;
    g->updating = 1;
    if (g->player_count > 1 && g->winner) goto done;
    if (g->player_count == 1 && g->shot_hit_count) ++*g->shot_hit_count;
    if (e->body.flags & 1) goto done;
    source = g->source;
    if (e->body.flags & 0x10) {
        if (g->player_count > 1) {
            if (source) {
                if (!(source->body.flags & 0x10)) source = g->source_owner;
                if (!source || !(source->body.flags & 0x10) ||
                    source->player_index != g->local_player) goto done;
                result = ops->event(ops->context, v, SSA_COMBAT_DAMAGE_NETWORK,
                                    e->player_index, (uint16_t)damage);
                goto done;
            }
        } else {
            index = (uint16_t)(source->type - 0xcd);
            if (index > 9) {
                source = g->source_owner;
                index = source ? (uint16_t)(source->type - 0xcd) : 12;
            }
            if (index < 12) ++g->enemy_hits[index];
            if (g->god_mode) damage = 0;
            if (!g->difficulty) damage >>= 1;
            else if (g->difficulty == 1) damage = (damage / 3) << 1;
            else if (g->difficulty == 2) damage += damage >> 1;
        }
        armor(&v->inventory->stat_c6, &damage);
        armor(&v->inventory->stat_c4, &damage);
        if (g->player_count == 1) *v->timer = 30;
        if (!(g->local_flags & 8) && g->local_player == e->player_index &&
            damage && !g->hurt_flash) {
            g->hurt_flash = 9;
            result = ops->event(ops->context, v, SSA_COMBAT_DAMAGE_SOUND,
                                (g->frame_counter & 1) + 15, 0);
            if (result < 0) goto done;
        }
    } else {
        if (!g->difficulty) damage += damage >> 1;
        else if (g->difficulty == 2) damage = (damage / 3) << 1;
    }
    *v->health = signed16((uint16_t)((uint16_t)*v->health - damage));
    if (*v->health <= 0) {
        *v->health = 0;
        index = (uint16_t)(e->type - 0xcd);
        if (index < 12) ++g->enemy_kills[index];
        if (g->credit_player) {
            if (g->player_count > 1) {
                ++*g->credited_kills;
                if (*g->credited_kills == g->kill_limit)
                    g->winner = (uint8_t)g->credit_player;
            } else {
                result = ops->event(ops->context, v, SSA_COMBAT_DAMAGE_REWARD, 0, 0);
                if (result < 0) goto done;
            }
            if (e->body.flags & 0x10) {
                *v->timer = 3000;
                e->body.flags |= 1;
            }
        }
    }
    if (g->player_count == 1) e->body.flags |= 1;
    e->body.flags &= UINT16_C(0xf7ff);
done:
    g->updating = 0;
    return result < 0 ? -1 : 0;
}
