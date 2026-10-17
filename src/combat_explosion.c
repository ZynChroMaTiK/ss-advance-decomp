#include "ssa/combat_explosion.h"
#include "ssa/enemy_targeting.h"
#include <limits.h>

static int32_t bits(uint32_t n) {
    return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)~n;
}
static int32_t whole_difference(int32_t a, int32_t b) {
    uint32_t n = ((uint32_t)a - (uint32_t)b) >> 16;
    return n <= INT16_MAX ? (int32_t)n : (int32_t)n - 65536;
}
static uint32_t attenuation(uint32_t squared, uint32_t radius, uint32_t damage) {
    uint32_t fraction = ((squared << 12) / radius) << 4;
    int64_t product = (int64_t)bits(damage << 16) * bits(fraction);
    int64_t shifted = product / 65536;
    if (product < 0 && product % 65536) --shifted;
    return damage - ((uint32_t)shifted >> 16);
}
int ssa_combat_explode(SsaCombatExplosion *explosion, const SsaMapAssets *map,
                       SsaEntity *source, SsaEntity *const *ordered, size_t count,
                       uint8_t game_mode, uint32_t radius_squared,
                       uint32_t damage, int include_players,
                       const SsaCombatExplosionOps *ops) {
    size_t i;
    int result = -1;
    if (!explosion || explosion->updating || !source || count > 100 ||
        (count && (!ordered || !ops || !ops->damage))) return -1;
    for (i = 0; i < count; ++i) if (!ordered[i]) return -1;
    explosion->updating = 1;
    for (i = 0; i < count; ++i) {
        SsaEntity *target = ordered[i];
        uint32_t applied = damage;
        uint16_t flags;
        if (target == source) continue;
        flags = target->body.flags;
        if ((target->type < 0xcd || target->type > 0xd8) &&
            (!(flags & 0x10) || !include_players)) continue;
        if (flags & 0x20) continue;
        if (!radius_squared) {
            int rejected = ssa_sector_reject(map, source->sector, target->sector);
            if (rejected < 0) goto done;
            if (rejected || target->type > 0xd6) continue;
        } else {
            int32_t x, y;
            uint32_t squared;
            if ((flags & 0x10) && game_mode == 1 && source->type == 0x67)
                damage >>= 2;
            x = whole_difference(target->body.x, source->body.x);
            y = whole_difference(target->body.y, source->body.y);
            squared = (uint32_t)(x * x) + (uint32_t)(y * y);
            if (squared >= radius_squared) continue;
            applied = attenuation(squared, radius_squared, damage);
        }
        if (ops->damage(ops->context, target, applied) < 0) goto done;
    }
    result = 0;
done:
    explosion->updating = 0;
    return result;
}
