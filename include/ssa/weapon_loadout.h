#ifndef SSA_WEAPON_LOADOUT_H
#define SSA_WEAPON_LOADOUT_H
#include "ssa/weapon_animation.h"

enum { SSA_WEAPON_LOADOUT_ROWS = 36 };
typedef struct SsaWeaponLoadouts {
    int16_t ammo[SSA_WEAPON_LOADOUT_ROWS][SSA_WEAPON_COUNT-1];
    unsigned loaded;
} SsaWeaponLoadouts;
typedef struct SsaWeaponLoadoutPlayer {
    SsaInventory *inventory;
    SsaWeaponControl *control;
    SsaWeaponObject *object;
    uint16_t *cooldowns; /* Stable array of ten; original clears only [player]. */
} SsaWeaponLoadoutPlayer;
typedef struct SsaWeaponLoadout { unsigned updating; } SsaWeaponLoadout;
typedef struct SsaWeaponLoadoutOps {
    /* Original F9B4 allocation boundary. Return >=0 success, -1 error. NULL
     * object is a native failure (original dereferences NULL). Ordinarily use
     * ssa_weapon_object_init on newly allocated storage. Do not reenter,
     * replace pointers or mutate player/inventory state in this callback. */
    int (*allocate)(void *, unsigned player, SsaWeaponObject **);
    void *context;
} SsaWeaponLoadoutOps;
/* Copy the 36 nonzero-setting records as data; no ROM execution. Setting zero
 * has no table access. Returns1/-1; invalid input leaves output unchanged. */
int ssa_weapon_loadouts_load(SsaWeaponLoadouts *,const void *rom,size_t size);
/* Whole1240C for0..4indexedplayers. control.keys is original BC flags;
 * inactive(bit4clear)players are untouched. setting is original settings+27,
 * god_mode is settings+26. Reject setting>36 instead of reading unrelatedROM.
 * Allocation and earlier players persist on later failures. Return1/-1.
 * Object pointer is replaced only after successful allocation. This does not
 * own/free allocated objects, grant health, or initialize a complete player. */
int ssa_weapon_loadout_initialize(SsaWeaponLoadout *,SsaWeaponLoadoutPlayer *,
    unsigned count,const SsaWeaponLoadouts *,const SsaInventoryRules *,
    unsigned setting,uint8_t god_mode,const SsaWeaponLoadoutOps *);
#endif
