#ifndef SSA_HUD_NOTICE_H
#define SSA_HUD_NOTICE_H
#include "ssa/hud_sprites.h"

/* Original E914/E9D4 message state, separate from diagnostic host text.
 * GAME+69/6A/6B/73/74/78/7A and the42-entry glyph scratch are retained.
 * Borrows immutable ROM and animation data. Keep stable while stepping.
 * Uses the caller's shared HUD game_flags/notice_timer/rectangle fields. */
typedef struct SsaHudNotice {
    const uint8_t *rom;size_t rom_size;
    uint8_t text[512],x[42],y[42];
    uint16_t image[42],id,value;
    uint8_t length,count,shift,base_y,progress;
    unsigned language,loaded;
} SsaHudNotice;
int ssa_hud_notice_init(SsaHudNotice *,const void *rom,size_t,unsigned language);
/* Original localized literal HUD notices. Integer/multi-argument encyclopedia
 * formatting is a separate screen and is explicitly rejected here. Values/id
 * truncate16bits and Y/font shift8bits, as E914. Strings exceeding native
 * storage or the original42-glyph scratch reject before any writes.1/-1. */
int ssa_hud_notice_set(SsaHudNotice *,SsaHudSpriteState *,unsigned id,unsigned value,
                       unsigned y,unsigned shift);
/* Complete E9D4 for one typeD sprite, including layout on subtype0, timer,
 * frame readiness/reveal latch and immediate E410 frame selection. Must run
 * in original sprite order, before the normal E460 animation/cache phases.
 * Other sprite handlers use the same shared HUD state. Invalid input leaves
 * all state unchanged.1/-1. */
int ssa_hud_notice_step(SsaWeaponObject *,SsaHudSpriteState *,SsaHudNotice *,
                        const SsaWeaponAnimations *,unsigned ticks);
#endif
