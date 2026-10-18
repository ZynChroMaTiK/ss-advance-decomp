#ifndef SSA_WEAPON_ANIMATION_H
#define SSA_WEAPON_ANIMATION_H
#include <stddef.h>
#include <stdint.h>
#include "ssa/weapon_select.h"

/* Fixed-width native projection of the original 48-byte sprite object. link
 * is retained as an opaque value, never dereferenced as a host pointer. */
typedef struct SsaWeaponObject {
    uint32_t link;
    uint16_t type, subtype, slot, active_index;
    int32_t x, y;
    int16_t screen_x, screen_y;
    uint16_t previous_frame, frame;
    uint32_t flags;
    uint16_t animation, frame_index;
    uint8_t period, elapsed, done, unknown27;
    uint16_t scale_x, scale_y, unknown2c;
    uint8_t layer, owner;
} SsaWeaponObject;

enum { SSA_WEAPON_ANIMATION_COUNT = 233, SSA_WEAPON_ANIMATION_WORDS = 528 };
typedef struct SsaWeaponAnimations {
    uint16_t words[SSA_WEAPON_ANIMATION_WORDS];
    uint16_t offsets[SSA_WEAPON_ANIMATION_COUNT];
    uint16_t lengths[SSA_WEAPON_ANIMATION_COUNT];
    unsigned loaded;
} SsaWeaponAnimations;

/* Copy and validate the animation table/data from the supplied ROM. No ROM
 * instructions execute, and the caller may release ROM storage after loading.
 * Return 1/-1. Failed loading leaves output unchanged. */
int ssa_weapon_animations_load(SsaWeaponAnimations *, const void *rom, size_t size);
/* Native equivalent of FAE8's cleared object defaults, excluding GBA DMA. */
int ssa_weapon_object_init(SsaWeaponObject *);
/* FC04. Accept animation0..E8 orFFFF (disabled); period truncates to byte.
 * Return1 accepted,0 invalid animation/no-op,-1 null object. */
int ssa_weapon_animation_set(SsaWeaponObject *, uint16_t animation, uint16_t period);
/* 116BC. Clear control flags1400 and set current-1, period3. Return1/0/-1. */
int ssa_weapon_animation_idle(SsaWeaponObject *, SsaWeaponControl *);
/* E410: resolve the current animation word without advancing its clock.
 * FFFF disables both frame mirrors. Invalid data leaves object unchanged. */
int ssa_weapon_animation_resolve(SsaWeaponObject *,const SsaWeaponAnimations *);
/* One object's exact E460 update. game_flags is GAME+4 and ticks is GAME+7.
 * Paused type2 objects skip; flags10 use a single tick. FFFF loops and sets
 * done1; FFFE holds last frame with periodFF; FFFA..FFFD write marker byte to
 * done. elapsed arithmetic wraps to byte before threshold checks.
 * Return1 or-1 for invalid integration, without mutation on failure.
 * Run after gameplay consumes the previous frame's completion state. */
int ssa_weapon_animation_step(SsaWeaponObject *, const SsaWeaponAnimations *,
                              uint16_t game_flags, unsigned ticks);
#endif
