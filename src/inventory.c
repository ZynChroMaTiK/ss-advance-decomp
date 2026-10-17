#include "ssa/inventory.h"

static int16_t signed_half(uint16_t value) {
    return value <= 32767u ? (int16_t)value : (int16_t)(-1 - (int)(65535u - value));
}

int ssa_inventory_rules_load(SsaInventoryRules *rules, const uint8_t *rom, size_t size) {
    unsigned i;
    if (!rules || !rom || size < 0x44bc8cu + 9u * 12u + 8u) return -1;
    for (i = 0; i < SSA_WEAPON_COUNT; ++i) {
        size_t offset = 0x44bc8cu + i * 12u + 6u;
        rules->ammo_capacity[i] = (uint16_t)((unsigned)rom[offset] | ((unsigned)rom[offset + 1] << 8));
    }
    return 0;
}

int ssa_inventory_has_weapon(const SsaInventory *inventory, unsigned weapon) {
    if (!inventory || weapon < 1 || weapon > SSA_WEAPON_COUNT) return -1;
    return (inventory->weapons & (1u << (weapon - 1))) != 0;
}

int ssa_inventory_queue_weapon(SsaInventory *inventory, unsigned weapon) {
    int owned = ssa_inventory_has_weapon(inventory, weapon);
    if (owned < 0) return -1;
    if (owned) return 0;
    if (inventory->pending_count >= SSA_PENDING_WEAPON_CAPACITY) return -1;
    inventory->pending_weapons[inventory->pending_count++] = weapon;
    return 1;
}

int ssa_inventory_add_ammo(SsaInventory *inventory, const SsaInventoryRules *rules,
                           unsigned weapon, int amount, unsigned player_count) {
    int owned = ssa_inventory_has_weapon(inventory, weapon);
    int16_t *ammo;
    uint16_t maximum;
    if (owned < 0 || !rules || player_count > 255u) return -1;
    if (!owned && player_count <= 1) return 0;
    ammo = &inventory->ammo[weapon - 1];
    maximum = rules->ammo_capacity[weapon - 1];
    if ((int)*ammo >= (int)maximum) return 0;
    *ammo = signed_half((uint16_t)((uint16_t)*ammo + (uint16_t)amount));
    if ((int)*ammo > (int)maximum) *ammo = signed_half(maximum);
    return 1;
}

int ssa_inventory_fill_ammo(SsaInventory *inventory, const SsaInventoryRules *rules,
                            unsigned player_count) {
    unsigned weapon;
    if (!inventory || !rules || player_count > 255u) return -1;
    for (weapon = 1; weapon <= SSA_WEAPON_COUNT; ++weapon)
        (void)ssa_inventory_add_ammo(inventory, rules, weapon, 2000, player_count);
    return 0;
}

static void add_stat(int16_t *stat, int amount) {
    int16_t delta = signed_half((uint16_t)amount);
    if (delta == 100) {
        *stat = signed_half((uint16_t)((uint16_t)*stat + 100u));
    } else if (*stat < 100) {
        *stat = signed_half((uint16_t)((uint16_t)*stat + (uint16_t)delta));
        if (*stat > 100) *stat = 100;
    }
}

void ssa_inventory_add_health(SsaInventory *inventory, int amount) {
    if (inventory) add_stat(&inventory->health, amount);
}
void ssa_inventory_add_stat_c4(SsaInventory *inventory, int amount) {
    if (inventory) add_stat(&inventory->stat_c4, amount);
}
void ssa_inventory_add_stat_c6(SsaInventory *inventory, int amount) {
    if (inventory) add_stat(&inventory->stat_c6, amount);
}
