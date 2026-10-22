#ifndef SSA_CLASSIC_HUD_H
#define SSA_CLASSIC_HUD_H
#include "ssa/hud_sprites.h"
#include "ssa/sprite_assets.h"
#include "ssa/inventory.h"

/* Original small menu font, decoded once for static Classic HUD notices. */
typedef struct SsaClassicHudText {
    uint8_t pixels[4032],glyph[256],metrics[252];
    uint16_t colors[16];
    unsigned loaded;
} SsaClassicHudText;
int ssa_classic_hud_text_load(SsaClassicHudText *,const uint8_t *rom,size_t);
int ssa_classic_hud_text(const SsaClassicHudText *,const SsaSpriteAssets *,
    const char *text,int y,uint8_t *canvas,uint8_t *background,size_t capacity);

/* Reconstructed prototype layout using retained ROM sprites. Transparent
 * indexed240x160 canvas; original assets/palette remain borrowed and unchanged.
 * The old health-digit controller is used; missing layout/data bindings are
 * native presentation policy, not recovered original constructor behavior.
 * ammo_flash_mask selects a green-to-neutral palette swap for ammo digits and
 * original meter pixels; source assets/palette remain unchanged.
 * Optional background receives a same-sized mask for45%-transparent black
 * backings. Foreground indices remain separate and fully opaque.
 * Returns1/-1. A failed draw may leave partial canvas output. */
int ssa_classic_hud_draw(const SsaSpriteAssets *,const SsaWeaponAnimations *,
    const SsaHudSpriteState *,const SsaInventoryRules *,
    unsigned ammo_flash_mask,unsigned health_flash,uint32_t high_score,uint8_t *canvas,uint8_t *background,size_t capacity);
/* Transient centered horizontal selector, composed into the existing layers.
 * Available weapons follow weapon order, sharing one icon for both Colt variants.
 * The requested weapon's entire icon/frame uses the neutral palette immediately. */
int ssa_classic_hud_weapon_wheel(const SsaSpriteAssets *,const SsaInventory *,
    unsigned selected,
    uint8_t *canvas,uint8_t *background,size_t capacity);
/* Shared inbox geometry: attention0 or30. After2s analysis, snap inward,
 * hold2s, then snap back. Idle215,2; attention108,28 (108,40 with a boss bar). */
int ssa_classic_hud_inbox_position(unsigned ticks,unsigned boss,int *left,int *top,unsigned *attention);
/* Retained analyzing sprite79 without backing;300-tick notification timeline.
 * Green/white blink remains every15 simulation ticks during the notification. */
int ssa_classic_hud_netricsa(const SsaSpriteAssets *,const SsaWeaponAnimations *,
    unsigned unread,unsigned ticks,unsigned boss,uint8_t *canvas,uint8_t *background,size_t capacity);
/*45% transparency retains45% of the underlying world/weapon color. */
uint16_t ssa_classic_hud_darken555(uint16_t color);
uint32_t ssa_classic_hud_darken888(uint32_t color);
/* Analysis switches fully on/off every15 simulation ticks; no backing. */
unsigned ssa_classic_hud_alpha(unsigned x,unsigned y,unsigned ticks,unsigned boss);
uint32_t ssa_classic_hud_blend888(uint32_t foreground,uint32_t behind,unsigned alpha);
uint16_t ssa_classic_hud_blend555(uint16_t foreground,uint16_t behind,unsigned alpha);
int ssa_classic_hud_use(const SsaSpriteAssets *,uint8_t *canvas,size_t capacity);
enum {SSA_CLASSIC_CROSSHAIR_SIZE=24,SSA_CLASSIC_CROSSHAIR_PIXELS=24*24,
      SSA_CLASSIC_CROSSHAIR_CENTER=12};
/* Fixed19x19 supplied crosshair in24x24 storage, dot at(12,12). White without a living enemy; TFE health
 * thresholds use green>=60%, yellow>=25%, otherwise red. OBJ palette only. */
int ssa_classic_hud_crosshair(const SsaSpriteAssets *,int health,unsigned maximum,uint8_t pixels[SSA_CLASSIC_CROSSHAIR_PIXELS]);
/* Retail HUD statistics/cursor replaced by Classic; weapons and notices stay. */
int ssa_classic_hud_replaces(unsigned sprite_type);
#endif
