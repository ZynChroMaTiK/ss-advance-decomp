#ifndef SSA_HUD_SPRITES_H
#define SSA_HUD_SPRITES_H
#include "ssa/sprite_pool.h"

/* Complete101F4 HUD construction: actual types, animations, positions and
 * scales. Requires74 free slots for one player,59+2*players for2..4.
 * motion_player is original GAME+68, not display-local0300464E. Level controls
 * only the original level5 icon choice. Objects are not advanced or drawn.
 * Returns allocated count/-1; insufficient capacity fails before mutations.
 * Numeric/message controllers still require live gameplay state. */
int ssa_hud_sprites_create(SsaSpritePool *,unsigned players,
                            unsigned motion_player,unsigned level);

/* Explicit display-local projection for the recovered scalar HUD handlers.
 * No invented inventory values: caller supplies original authoritative state.
 * Numeric scratch persists between calls/frames and is rebuilt only by the
 * original designated subtype. Do not rebuild it independently per digit. */
typedef struct SsaHudSpriteState {
    uint16_t local_flags,current_weapon; /* current_weapon is PLAYER+78 REQUESTED. */
    uint32_t weapon_flags,weapon_action; /* weapon_action is PLAYER+28 COMMITTED. */
    int32_t aim_marker;
    uint8_t game_value,player_values[4],skip_player_value;
    int16_t health,armor,boss_health;
    uint8_t boss_present;
    int16_t ammo[10];
    uint32_t score,time;
    uint16_t game_flags;
    int16_t notice_timer;
    uint8_t rectangle[4];
    int32_t rectangle_tail[4];
    uint16_t ammo_x[3],ammo_image[3],score_x[8],score_image[8],time_x[5],time_image[5];
    uint8_t glyph_widths[128];
    unsigned glyphs_loaded;
} SsaHudSpriteState;
/* Copies glyph widths at0844BC0C without resetting gameplay/scratch fields. */
int ssa_hud_glyphs_load(SsaHudSpriteState *,const void *rom,size_t size);
/* Whole F0B8(type4),E700(5),E7A4(6),F1C0(7),F27C(8),ECF8(9),F344(C),
 * F4A4(E),F5A4(F),F6C8(10),F724(11),F794(12), hexadecimal types.
 * E9D4(typeD) only the inactive-notice/automap branch; active notices return0.
 * Bounded numeric fields: original3/8/5-character scratch capacities. Outsize
 * values/subtypes fail explicitly instead of original scratch corruption.
 * Returns1 handled,0 unsupported,-1 invalid. Unsupported leaves all untouched.
 * Calls originalFC04 native setter; no animation clock/pixel draw implied. */
int ssa_hud_sprite_step(SsaWeaponObject *,SsaHudSpriteState *);
/* Whole F100(type3) visibility+done release, through the native pool. */
int ssa_hud_transient_step(SsaSpritePool *,SsaWeaponObject *,uint16_t local_flags);
#endif
