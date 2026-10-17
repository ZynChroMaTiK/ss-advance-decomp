#ifndef SSA_HUD_SCENE_H
#define SSA_HUD_SCENE_H
#include "ssa/hud_sprites.h"
#include "ssa/sprite_cache.h"
#include "ssa/display.h"
#include "ssa/weapon_controller.h"
#include "ssa/weapon_loadout.h"

/* Single-player diagnostic presentation owner, not the full game runtime.
 * Actual1240C loadout then101F4 HUD in their original pool order. Borrows
 * authoritative inventory/body and stable sprite assets/math. Keep address
 * stable: embedded pool/controller pointers refer back to this owner. */
typedef struct SsaHudScene {
    SsaWeaponObject objects[84];
    SsaSpriteDispatch dispatch;
    SsaSpritePool pool;
    SsaSpriteCache cache;
    SsaSpriteOam oam;
    SsaHudSpriteState state;
    SsaWeaponAnimations animations;
    SsaWeaponLoadouts loadouts;
    SsaInventoryRules inventory_rules;
    SsaWeaponLoadout loadout;
    SsaWeaponControl control;
    SsaWeaponController weapon;
    SsaWeaponObject *weapon_object;
    const SsaSpriteAssets *assets;
    const SsaMathTables *math;
    SsaDisplayRegisters registers;
    unsigned loaded,updating;
} SsaHudScene;
int ssa_hud_scene_init(SsaHudScene *,SsaInventory *,SsaEntity *,
    const SsaSpriteAssets *,const SsaMathTables *,const void *rom,size_t size,
    unsigned level,unsigned loadout_setting);
int ssa_hud_scene_free(SsaHudScene *);
/* Caller updates state.health/armor/score/time/game/local_flags from actual
 * canonical owners. Internal numeric scratch/glyph data must persist. Uses
 * native scalar HUD handlers and animation/cache/OAM; no fabricated fullgame
 * stage callbacks. Explicit idle-only weapon mode: runs12160 raise/lower,
 * not122AC aim/fire/combat. input_state3 skips that raise as originalF150.
 * H hides first-person weapon flag1 only; aim/inventory/body untouched except
 * original HUD's unlimited-ammo write and initial real loadout.1/-1. */
int ssa_hud_scene_step_idle(SsaHudScene *,unsigned ticks,unsigned input_state,
    unsigned third_person);
int ssa_hud_scene_draw(SsaHudScene *,const uint8_t *world,size_t size,
    unsigned palette_family,uint16_t *rgb555,size_t pixels);
#endif
