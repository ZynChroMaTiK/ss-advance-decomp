#include "ssa/sector_movers.h"
#include <string.h>

static unsigned u16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static int s16(unsigned v) { return v < 32768 ? (int)v : (int)v - 65536; }
static uint32_t u32(const uint8_t *p)
{ return u16(p) | (uint32_t)u16(p + 2) << 16; }
static void put16(uint8_t *p, unsigned v)
{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static int range(size_t size, size_t offset, size_t count)
{ return offset <= size && count <= size - offset; }
static int valid(const SsaSectorMovers *s)
{
    unsigned i;
    if (!s || !s->map || !s->map->data || s->count > 25 ||
        s->hazard_count > 8 || s->updating ||
        !range(s->map->size, s->map->sectors, (size_t)s->map->sector_count * 20))
        return 0;
    for (i = 0; i < s->count; ++i)
        if (u16(s->records[i]) >= s->map->sector_count) return 0;
    return 1;
}
static int sound(const SsaSectorMoverOps *ops, unsigned id)
{ return !ops || !ops->sound || ops->sound(ops->context, id) >= 0; }
static void add_hazard(SsaSectorMovers *s, unsigned sector)
{
    if (s->hazard_count < 8) s->hazard_sectors[s->hazard_count++] = sector;
}
static void remove_hazard(SsaSectorMovers *s, unsigned sector)
{
    unsigned i;
    for (i = 0; i < s->hazard_count; ++i)
        if ((s->hazard_sectors[i] & 65535) == sector) {
            s->hazard_sectors[i] = s->hazard_sectors[--s->hazard_count];
            break;
        }
}
int ssa_sector_movers_init(SsaSectorMovers *out, SsaMapAssets *map,
                           const uint8_t *rom, size_t size, unsigned level)
{
    SsaSectorMovers next;
    uint32_t offset;
    unsigned i;
    if (!out || out->updating || !rom || level >= 16 ||
        !range(size, 0x44dd3c, 80)) return 0;
    memset(&next, 0, sizeof next);
    memcpy(next.hazard_sectors, out->hazard_sectors, sizeof next.hazard_sectors);
    next.map = map;
    next.count = rom[0x44dd3c + level];
    offset = u32(rom + 0x44dd4c + level * 4);
    if (offset < 0x08000000) return 0;
    offset -= 0x08000000;
    if (!range(size, offset, sizeof next.records)) return 0;
    memcpy(next.records, rom + offset, sizeof next.records);
    if (!valid(&next)) return 0;
    for (i = 0; i < next.count; ++i) {
        uint8_t *r = next.records[i];
        uint8_t *sector = map->data + map->sectors + u16(r) * 20;
        unsigned flags = u16(r + 14);
        if (flags & 256) put16(sector, u16(r + ((flags & 1) ? 10 : 8)));
        else put16(r + 14, u16(sector) == u16(sector + 2) ?
                   flags & ~1u : flags | 1u);
        put16(r + 16, u16(sector + 18));
    }
    *out = next;
    return 1;
}
int ssa_sector_movers_command(SsaSectorMovers *s, unsigned index,
                              SsaSectorMoverCommand command,
                              const SsaSectorMoverOps *ops)
{
    uint8_t *r;
    unsigned flags;
    int ok = 1;
    if (!valid(s) || index >= s->count || (unsigned)command > 3) return 0;
    s->updating = 1;
    r = s->records[index];
    flags = u16(r + 14);
    switch (command) {
    case SSA_SECTOR_MOVER_OPEN:
        put16(r + 14, flags | 9); ok = sound(ops, 0x67); break;
    case SSA_SECTOR_MOVER_CLOSE:
        put16(r + 14, (flags & ~1u) | 8);
        ok = sound(ops, 0x67);
        if (ok && !(flags & 256)) add_hazard(s, u16(r));
        break;
    case SSA_SECTOR_MOVER_LOCK: put16(r + 14, flags | 4); break;
    case SSA_SECTOR_MOVER_UNLOCK: put16(r + 14, flags & ~4u); break;
    }
    s->updating = 0;
    return ok;
}
int ssa_sector_movers_activate_line(SsaSectorMovers *s, unsigned line,
                                    const SsaSectorMoverOps *ops)
{
    unsigned i;
    const uint8_t *l;
    int ok = 1;
    if (!valid(s) || line >= s->map->line_count ||
        !range(s->map->size, s->map->lines, (size_t)s->map->line_count * 40))
        return 0;
    s->updating = 1;
    l = s->map->data + s->map->lines + line * 40;
    for (i = 0; i < s->count; ++i) {
        uint8_t *r = s->records[i];
        unsigned flags = u16(r + 14), sector = u16(r);
        if ((s16(u16(l + 12)) == (int)sector ||
             s16(u16(l + 14)) == (int)sector) && !(flags & 4)) {
            if (!(flags & 1)) {
                put16(r + 14, flags | 8);
                if (!sound(ops, 0x67)) { ok = 0; break; }
            }
            put16(r + 14, u16(r + 14) | 1);
            break;
        }
    }
    s->updating = 0;
    return ok;
}
int ssa_sector_movers_step(SsaSectorMovers *s, unsigned ticks,
                           const SsaSectorMoverOps *ops)
{
    unsigned i;
    int ok = 1;
    if (!valid(s) || ticks > 255) return 0;
    s->updating = 1;
    for (i = 0; i < s->count; ++i) {
        uint8_t *r = s->records[i];
        uint8_t *sec = s->map->data + s->map->sectors + u16(r) * 20;
        unsigned flags = u16(r + 14), delta = u16(r + 12) * ticks;
        unsigned floor_mover = flags & 256, opening = flags & 1;
        uint8_t *height = sec + (floor_mover ? 0 : 2);
        unsigned h = (opening ? u16(height) + delta : u16(height) - delta) & 65535;
        int limit = floor_mover ? s16(u16(r + (opening ? 10 : 8))) :
                    s16(u16(sec)) + (opening ? 224 : 0);
        put16(height, h);
        if ((opening && s16(h) > limit) || (!opening && s16(h) < limit)) {
            if (flags & 8) {
                put16(r + 14, flags & ~8u);
                if (!sound(ops, 0x68)) { ok = 0; break; }
                if (!floor_mover && !opening) remove_hazard(s, u16(r));
            }
            put16(height, (unsigned)limit);
        }
    }
    s->updating = 0;
    return ok;
}
int ssa_sector_movers_hazard_contains(const SsaSectorMovers *s, uint32_t sector)
{
    unsigned i;
    if (!valid(s)) return -1;
    for (i = 0; i < s->hazard_count; ++i)
        if (s->hazard_sectors[i] == sector) return 1;
    return 0;
}
