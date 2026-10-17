#ifndef SSA_INVENTORY_H
#define SSA_INVENTORY_H
#include <stddef.h>
#include <stdint.h>

enum { SSA_WEAPON_COUNT = 10, SSA_PENDING_WEAPON_CAPACITY = 10 };
typedef struct SsaInventory {
    uint16_t weapons;
    int16_t ammo[SSA_WEAPON_COUNT];
    uint16_t pending_count;
    uint32_t pending_weapons[SSA_PENDING_WEAPON_CAPACITY];
    int16_t health, stat_c4, stat_c6;
} SsaInventory;
typedef struct SsaInventoryRules {
    uint16_t ammo_capacity[SSA_WEAPON_COUNT];
} SsaInventoryRules;

/* Data only, from weapon records at ROM offset 0x44BC8C. Returns 0 or -1.
 * No output changes when the ROM span or pointers are invalid. */
int ssa_inventory_rules_load(SsaInventoryRules *, const uint8_t *rom, size_t size);
/* Original 080114B8 / 08011558. Weapon IDs are 1..10. Returns 0/1,
 * or -1 on invalid input. Queueing does not grant ownership yet; repeated
 * unowned weapon IDs can be queued. Rejects the original buffer overflow. */
int ssa_inventory_has_weapon(const SsaInventory *, unsigned weapon);
int ssa_inventory_queue_weapon(SsaInventory *, unsigned weapon);
/* Original 080114D4 / 08011534. Amount is truncated to signed 16 bits.
 * Unowned ammo is accepted only in multiplayer. Signed 16-bit addition wraps
 * before the capacity comparison, as in the original. add returns 0/1/-1;
 * fill returns 0/-1 and adds 2000 for each eligible weapon. */
int ssa_inventory_add_ammo(SsaInventory *, const SsaInventoryRules *,
                           unsigned weapon, int amount, unsigned player_count);
int ssa_inventory_fill_ammo(SsaInventory *, const SsaInventoryRules *,
                            unsigned player_count);
/* Original 0800549C / 080054D8 / 08005510. Amount 100 bypasses the normal
 * 100 cap. Other amounts change only values below 100, then cap at 100.
 * Arithmetic wraps to signed 16 bits. NULL is ignored. */
void ssa_inventory_add_health(SsaInventory *, int amount);
void ssa_inventory_add_stat_c4(SsaInventory *, int amount);
void ssa_inventory_add_stat_c6(SsaInventory *, int amount);
#endif
