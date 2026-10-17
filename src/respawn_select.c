#include "ssa/respawn_select.h"
#include <limits.h>

static int32_t bits(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)~v;
}
static int32_t whole(int32_t v) {
    return bits(((uint32_t)v >> 16) | (v < 0 ? UINT32_C(0xffff0000) : 0));
}
/* This caller's differences span -65535..65535, wider than ssa_direction's
 * signed-halfword API. The original helper receives full register values. */
static int32_t distance(const SsaMathTables *t, int32_t x, int32_t y) {
    int32_t ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    int32_t ratio, angle, sine, cosine, denominator;
    uint16_t heading;
    if (!ax && !ay) return 0;
    ratio = ax * 16384 / (ax + ay);
    angle = t->direction[x < 0 ? 16384 - ratio : 16384 + ratio];
    if (y > 0) angle = -angle;
    heading = (uint16_t)(angle + 16384);
    ssa_sincos(t, heading, &sine, &cosine);
    denominator = ax > ay ? sine : cosine;
    if (denominator < 0) denominator = -denominator;
    return bits((uint32_t)(ax > ay ? ax : ay) << 16) / denominator;
}

int ssa_respawn_select(SsaSpawnMarker *out, const SsaSpawnMarker *markers,
                       size_t count, const SsaRespawnPosition *players,
                       unsigned player_count, const SsaMathTables *tables) {
    size_t i, best_index = 0;
    int32_t best = 0;
    unsigned j;
    if (!out || (!markers && count) || (!players && player_count) ||
        player_count > 4 || !tables) return -1;
    for (i = 0; i < count; ++i) {
        int32_t nearest = 10000;
        for (j = 0; j < player_count; ++j) {
            int32_t d;
            if (!players[j].x && !players[j].y) continue;
            d = distance(tables, whole(players[j].x) - markers[i].x,
                          whole(players[j].y) - markers[i].y);
            if (d < nearest) nearest = d;
        }
        if (nearest > best) { best = nearest; best_index = i; }
    }
    if (!best) return 0;
    *out = markers[best_index];
    out->z = out->y;
    return 1;
}
