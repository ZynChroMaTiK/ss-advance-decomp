#include "ssa/weapon_animation.h"
#include <string.h>

_Static_assert(sizeof(SsaWeaponObject) == 48, "sprite object layout");
_Static_assert(offsetof(SsaWeaponObject, animation) == 0x20, "animation offset");
_Static_assert(offsetof(SsaWeaponObject, done) == 0x26, "done offset");

static uint16_t read16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)read16(p) | ((uint32_t)read16(p + 2) << 16);
}

int ssa_weapon_animations_load(SsaWeaponAnimations *out, const void *data, size_t size) {
    const uint8_t *rom = data;
    SsaWeaponAnimations result;
    unsigned i;
    if (!out || !rom || size < 0x44b36cu + SSA_WEAPON_ANIMATION_COUNT * 4u)
        return -1;
    memset(&result, 0, sizeof(result));
    for (i = 0; i < SSA_WEAPON_ANIMATION_WORDS; ++i)
        result.words[i] = read16(rom + 0x44af4c + 2u * i);
    for (i = 0; i < SSA_WEAPON_ANIMATION_COUNT; ++i) {
        uint32_t address = read32(rom + 0x44b36c + 4u * i);
        unsigned offset, n;
        if (address < 0x0844af4c || address >= 0x0844b36c || (address & 1))
            return -1;
        offset = (address - 0x0844af4c) / 2;
        for (n = 0; offset + n < SSA_WEAPON_ANIMATION_WORDS; ++n)
            if (result.words[offset + n] >= 0xfffe) break;
        if (!n || offset + n >= SSA_WEAPON_ANIMATION_WORDS) return -1;
        result.offsets[i] = (uint16_t)offset;
        result.lengths[i] = (uint16_t)(n + 1);
    }
    result.loaded = 1;
    *out = result;
    return 1;
}

int ssa_weapon_object_init(SsaWeaponObject *object) {
    if (!object) return -1;
    memset(object, 0, sizeof(*object));
    object->scale_x = object->scale_y = 0x1000;
    object->layer = 1;
    object->animation = object->previous_frame = 0xffff;
    object->flags = 1;
    return 1;
}

int ssa_weapon_animation_set(SsaWeaponObject *object, uint16_t animation, uint16_t period) {
    if (!object) return -1;
    if (animation > 0xe8 && animation != 0xffff) return 0;
    object->animation = animation;
    object->period = (uint8_t)period;
    object->frame_index = 0;
    object->elapsed = 0;
    object->done = 0;
    return 1;
}

int ssa_weapon_animation_idle(SsaWeaponObject *object, SsaWeaponControl *control) {
    if (!object || !control) return -1;
    control->flags &= ~UINT32_C(0x1400);
    return ssa_weapon_animation_set(object, (uint16_t)(control->current - 1u), 3);
}

int ssa_weapon_animation_resolve(SsaWeaponObject *object,const SsaWeaponAnimations *animations) {
    unsigned start,length;
    if(!object || !animations || animations->loaded!=1)return -1;
    if(object->animation==0xffff){object->previous_frame|=0x3fff;object->frame|=0x3fff;return 1;}
    if(object->animation>=SSA_WEAPON_ANIMATION_COUNT)return -1;
    start=animations->offsets[object->animation];length=animations->lengths[object->animation];
    if(object->frame_index>=length || start+object->frame_index>=SSA_WEAPON_ANIMATION_WORDS)return -1;
    object->frame=(uint16_t)((object->frame&0xc000)|(animations->words[start+object->frame_index]&0x3fff));return 1;
}

int ssa_weapon_animation_step(SsaWeaponObject *object, const SsaWeaponAnimations *animations,
                              uint16_t game_flags, unsigned ticks) {
    SsaWeaponObject next;
    unsigned start, length, position;
    if (!object || !animations || animations->loaded != 1 || ticks > 255) return -1;
    if ((game_flags & 1) && object->type == 2) return 1;
    if (object->animation == 0xffff) {
        object->previous_frame |= 0x3fff;
        object->frame |= 0x3fff;
        return 1;
    }
    if (object->animation >= SSA_WEAPON_ANIMATION_COUNT) return -1;
    start = animations->offsets[object->animation];
    length = animations->lengths[object->animation];
    position = object->frame_index;
    if (length < 2 || start + length > SSA_WEAPON_ANIMATION_WORDS || position >= length - 1)
        return -1;
    next = *object;
    next.frame = (uint16_t)((next.frame & 0xc000) | (animations->words[start + position] & 0x3fff));
    if (next.period) {
        next.elapsed = (uint8_t)(next.elapsed + ((next.flags & 0x10) ? 1 : ticks));
        while (next.elapsed >= next.period) {
            uint16_t value;
            next.elapsed = (uint8_t)(next.elapsed - next.period);
            if (++position >= length) return -1;
            value = animations->words[start + position];
            if (value == 0xffff) {
                next.frame_index = 0;
                next.done = 1;
                position = 0;
            } else if (value == 0xfffe) {
                next.done = 1;
                next.period = 255;
                break;
            } else {
                if (value >= 0xfffa && value <= 0xfffd) next.done = (uint8_t)value;
                ++next.frame_index;
            }
            next.frame = (uint16_t)((next.frame & 0xc000) | (animations->words[start + position] & 0x3fff));
        }
    }
    *object = next;
    return 1;
}
