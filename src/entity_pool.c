#include "ssa/entity_pool.h"
#include <string.h>

static int valid(const SsaEntityPool *pool) {
    uint8_t seen[SSA_ENTITY_POOL_CAPACITY] = {0};
    unsigned i, slot;
    if (!pool || pool->active_count > SSA_ENTITY_POOL_CAPACITY) return 0;
    for (i = 0; i < pool->active_count; ++i) {
        slot = pool->active_slots[i];
        if (slot >= SSA_ENTITY_POOL_CAPACITY || seen[slot] ||
            pool->occupied[slot] != 1 || pool->positions[slot] != i) return 0;
        seen[slot] = 1;
    }
    for (i = pool->active_count; i < SSA_ENTITY_POOL_CAPACITY; ++i) {
        slot = pool->free_slots[i];
        if (slot >= SSA_ENTITY_POOL_CAPACITY || seen[slot] ||
            pool->occupied[slot] || pool->positions[slot]) return 0;
        seen[slot] = 1;
    }
    return 1;
}

int ssa_entity_pool_init(SsaEntityPool *pool) {
    unsigned i;
    if (!pool) return -1;
    memset(pool, 0, sizeof(*pool));
    for (i = 0; i < SSA_ENTITY_POOL_CAPACITY; ++i)
        pool->free_slots[i] = (uint16_t)i;
    return 1;
}

int ssa_entity_pool_allocate(SsaEntityPool *pool, unsigned *slot) {
    unsigned index, position;
    if (!slot || !valid(pool)) return -1;
    position = pool->active_count;
    if (position == SSA_ENTITY_POOL_CAPACITY) return 0;
    index = pool->free_slots[position];
    pool->active_slots[position] = (uint16_t)index;
    pool->positions[index] = (uint16_t)position;
    pool->occupied[index] = 1;
    pool->active_count = (uint16_t)(position + 1u);
    *slot = index;
    return 1;
}

int ssa_entity_pool_release(SsaEntityPool *pool, unsigned slot) {
    unsigned position, last, moved;
    if (!valid(pool) || slot >= SSA_ENTITY_POOL_CAPACITY ||
        !pool->occupied[slot]) return -1;
    position = pool->positions[slot];
    last = (unsigned)pool->active_count - 1u;
    pool->free_slots[last] = (uint16_t)slot;
    pool->active_count = (uint16_t)last;
    if (position != last) {
        moved = pool->active_slots[last];
        pool->active_slots[position] = (uint16_t)moved;
        pool->positions[moved] = (uint16_t)position;
    }
    pool->occupied[slot] = 0;
    pool->positions[slot] = 0;
    return 1;
}
