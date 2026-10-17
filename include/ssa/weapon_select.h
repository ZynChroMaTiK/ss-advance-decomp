#ifndef SSA_WEAPON_SELECT_H
#define SSA_WEAPON_SELECT_H
#include "ssa/inventory.h"

typedef struct SsaWeaponControl {
    uint32_t current, flags;
    uint16_t requested, keys;
} SsaWeaponControl;

/* 08011588: consume one pending weapon (LIFO), next key 0x40, previous key
 * 0x80, or request previous when current ammo is exactly zero. Flags0x2C00
 * inhibit consumption. Returns0 (none), weapon1..10,254(next),255(previous),
 * or -1 for invalid pointers/count/current/queued ID. Error does not mutate. */
int ssa_weapon_take_request(SsaInventory *, SsaWeaponControl *);
/* 08011FFC plus12038/12050: cycle from requested in direction +1/-1,
 * wrapping sixteen slots, to an owned weapon with nonzero ammo. Returns
 * chosenID1..10, or -1 without mutation when none is available or the
 * ownership mask references unsupported slots11..16. Original loops forever
 * when no eligible weapon exists; the bounded failure is a native guard. */
int ssa_weapon_cycle(const SsaInventory *, SsaWeaponControl *, int direction);
/* 08011604: grant one queued weapon and half its ammo capacity, selecting it
 * only when old ownership mask < its bit; or consume a cycle request.
 * Returns0/-1. Invalid integration leaves both inputs unchanged. Weapon
 * animation/current-weapon commit is not performed by this original phase. */
int ssa_weapon_update(SsaInventory *, SsaWeaponControl *,
                       const SsaInventoryRules *, unsigned player_count);
#endif
