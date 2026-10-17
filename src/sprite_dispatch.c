#include "ssa/sprite_dispatch.h"

uint32_t ssa_sprite_dispatch_handler(uint16_t type)
{
    static const uint32_t handlers[18] = {
        0, 0x0800F150, 0x0800F100, 0x0800F0B8, 0x0800E700, 0x0800E7A4,
        0x0800F1C0, 0x0800F27C, 0x0800ECF8, 0x0800EE60, 0x0800EFB4,
        0x0800F344, 0x0800E9D4, 0x0800F4A4, 0x0800F5A4, 0x0800F6C8,
        0x0800F724, 0x0800F794
    };
    return type >= 1 && type <= 18 ? handlers[type - 1] : 0;
}

static int current_valid(const SsaSpriteDispatch *d)
{
    unsigned i;
    if (!d->current || d->active_count > SSA_SPRITE_CAPACITY) return 0;
    for (i = 0; i < SSA_SPRITE_CAPACITY; ++i)
        if (d->current == d->objects[i]) return 1;
    return 0;
}

int ssa_sprite_dispatch_step(SsaSpriteDispatch *d, const SsaSpriteDispatchOps *ops)
{
    uint32_t i, cursor = 0;
    int result = -1;
    if (!d || d->updating || !ops || !ops->handler ||
        d->active_count > SSA_SPRITE_CAPACITY) return -1;
    for (i = 0; i < SSA_SPRITE_CAPACITY; ++i)
        if (!d->objects[i]) return -1;
    d->updating = 1;
    d->submitted_count = 0;
    d->counter++;
    d->state = 1;
    for (i = 0; i < d->active_count; ++i) {
        unsigned slot;
        uint32_t entry;
        if (cursor >= SSA_SPRITE_CAPACITY ||
            d->active[cursor] >= SSA_SPRITE_CAPACITY) goto done;
        slot = (unsigned)d->active[cursor];
        d->current = d->objects[slot];
        d->current_index = i;
        entry = ssa_sprite_dispatch_handler(d->current->type);
        if (entry && ops->handler(ops->context, d, entry, slot) < 0) goto done;
        d->state = 1;
        if (!current_valid(d)) goto done;
        if (d->current->type) {
            SsaWeaponObject *object = d->current;
            unsigned index = d->submitted_count;
            d->state = 2;
            if (index >= SSA_SPRITE_CAPACITY) goto done;
            d->submitted_count = (uint8_t)(index + 1u);
            d->submitted[index] = (uint8_t)object->active_index;
            if (ops->submit && ops->submit(ops->context, d, object) < 0) goto done;
            if (!current_valid(d)) goto done;
            cursor++;
        }
    }
    result = 0;
done:
    d->state = 3;
    d->current = d->objects[0];
    d->updating = 0;
    return result;
}
