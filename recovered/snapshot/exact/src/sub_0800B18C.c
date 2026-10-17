#include "common.h"

typedef struct RejectMap {
    u8 pad0[10];
    u16 sector_count;
    u8 padC[52];
    u8 *reject;
} RejectMap;

s32 sub_0800B18C(RejectMap *map, s16 source, s16 target) {
    u32 reject = (u32)map->reject;
    u32 bit = source * map->sector_count + target;
    u32 mask;
    reject += bit >> 3;
    mask = 1 << (bit & 7);
    reject = *(u8 *)reject;
    return mask & reject;
}
