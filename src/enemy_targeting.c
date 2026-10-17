#include "ssa/enemy_targeting.h"
#include <limits.h>

static int32_t signed_bits(uint32_t n) {
    return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)~n;
}
static int32_t whole_difference(int32_t a, int32_t b) {
    uint32_t n = ((uint32_t)a - (uint32_t)b) >> 16;
    return n <= INT16_MAX ? (int32_t)n : (int32_t)n - 65536;
}
static int nearest(const SsaEntity *source, SsaEntity *const *ordered,
                   size_t count, SsaEntity **out, int pickup) {
    SsaEntity *best = NULL;
    int32_t distance = INT32_MAX;
    size_t i;
    if (!source || !out || count > 100 || (count && !ordered)) return -1;
    for (i = 0; i < count; ++i) if (!ordered[i]) return -1;
    for (i = 0; i < count; ++i) {
        SsaEntity *e = ordered[i];
        int32_t x, y, squared;
        if (e == source) continue;
        if (pickup) {
            if (e->type != 0xe1) continue;
        } else if (e->type < 0xcd || e->type > 0xd6 ||
                   (e->body.flags & 0x20)) continue;
        x = whole_difference(e->body.x, source->body.x);
        y = whole_difference(e->body.y, source->body.y);
        squared = signed_bits((uint32_t)(x * x) + (uint32_t)(y * y));
        if (squared < distance) { distance = squared; best = e; }
    }
    *out = best;
    return 0;
}
int ssa_enemy_nearest(const SsaEntity *source, SsaEntity *const *ordered,
                      size_t count, SsaEntity **out) {
    return nearest(source, ordered, count, out, 0);
}
int ssa_pickup_nearest(const SsaEntity *source, SsaEntity *const *ordered,
                       size_t count, SsaEntity **out) {
    return nearest(source, ordered, count, out, 1);
}
int ssa_sector_reject(const SsaMapAssets *map, int source_sector, int target_sector) {
    size_t bit, bytes;
    if (!map || !map->data || source_sector < 0 || target_sector < 0 ||
        source_sector > INT16_MAX || target_sector > INT16_MAX ||
        source_sector >= map->sector_count || target_sector >= map->sector_count)
        return -1;
    bytes = ((size_t)map->sector_count * map->sector_count + 7) / 8;
    if (map->reject > map->size || bytes > map->size - map->reject) return -1;
    bit = (size_t)source_sector * map->sector_count + (size_t)target_sector;
    return map->data[map->reject + bit / 8] & (1u << (bit & 7));
}
