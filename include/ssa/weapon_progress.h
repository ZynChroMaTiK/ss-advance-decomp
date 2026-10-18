#ifndef SSA_WEAPON_PROGRESS_H
#define SSA_WEAPON_PROGRESS_H
#include "ssa/inventory.h"
#include "ssa/weapon_animation.h"
typedef struct SsaWeaponProgress {
    uint32_t current;
    uint16_t weapons;
    int16_t ammo[10];
    uint16_t requested;
} SsaWeaponProgress;
typedef struct SsaWeaponProgressStats {
    int32_t owned, ammo;
    uint32_t shots, hits;
} SsaWeaponProgressStats;
/* Original1231C. Capture equipped/requested guns, ownership and ammo. Refresh
 * the ten original GAME+90 rows, preserving shot/hit counters when setting0;
 * any nonzero original settings+1B byte clears those counters. No inventory
 * health/protection/queue or control fields change. Return1/-1; null rejects
 * before writes. Arguments must refer to distinct live objects. */
int ssa_weapon_progress_capture(SsaWeaponProgress *,SsaWeaponProgressStats [10],
    const SsaInventory *,const SsaWeaponControl *,uint8_t setting);
/* Original123C4: restore only the equipped weapon, owned mask and ammunition;
 * requested becomes current and its idle animation restarts at period3.
 * Health, protection, pending pickups and flags are untouched. */
int ssa_weapon_progress_restore(SsaInventory *,SsaWeaponControl *,SsaWeaponObject *,
    const SsaWeaponProgress *);
#endif
