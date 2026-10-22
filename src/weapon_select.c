#include "ssa/weapon_order.h"
#include "ssa/weapon_select.h"

int ssa_weapon_take_request(SsaInventory *inventory, SsaWeaponControl *control) {
    unsigned weapon;
    if (!inventory || !control) return -1;
    if (control->flags & 0x2c00u) return 0;
    if (inventory->pending_count) {
        if (inventory->pending_count > SSA_PENDING_WEAPON_CAPACITY) return -1;
        weapon = inventory->pending_weapons[inventory->pending_count - 1];
        if (weapon < 1 || weapon > SSA_WEAPON_COUNT) return -1;
        --inventory->pending_count;
        return (int)weapon;
    }
    if (control->keys & 0x40u) {
        control->keys &= (uint16_t)~0x40u;
        return 254;
    }
    if (control->keys & 0x80u) {
        control->keys &= (uint16_t)~0x80u;
        return 255;
    }
    if (control->current < 1 || control->current > SSA_WEAPON_COUNT) return -1;
    return inventory->ammo[control->current - 1] == 0 ? 255 : 0;
}

int ssa_weapon_cycle(const SsaInventory *inventory, SsaWeaponControl *control, int direction) {
    unsigned slot, i;
    if (!inventory || !control || (direction != 1 && direction != -1)
        || (inventory->weapons & 0xfc00u)) return -1;
    slot = ((unsigned)control->requested - 1u) & 15u;
    for (i = 0; i < 16; ++i) {
        slot = (slot + (direction == 1 ? 1u : 15u)) & 15u;
        if (slot < SSA_WEAPON_COUNT && (inventory->weapons & (1u << slot))
            && inventory->ammo[slot] != 0) {
            control->requested = (uint16_t)(slot + 1);
            return (int)(slot + 1);
        }
    }
    return -1;
}

static int weapon_update(SsaInventory *inventory, SsaWeaponControl *control,
                       const SsaInventoryRules *rules, unsigned player_count, unsigned pc_order) {
    SsaInventory next_inventory;
    SsaWeaponControl next_control;
    int request;
    if (!inventory || !control || !rules || player_count > 255u) return -1;
    next_inventory = *inventory;
    next_control = *control;
    request = ssa_weapon_take_request(&next_inventory, &next_control);
    if (request < 0) return -1;
    if (request > 0 && request <= SSA_WEAPON_COUNT) {
        unsigned bit = 1u << ((unsigned)request - 1);
        int select = next_inventory.weapons < bit;
        if(pc_order){unsigned i;select=1;
            for(i=0;i<SSA_WEAPON_COUNT;++i)if((next_inventory.weapons&(1u<<i)) &&
                ssa_weapon_order_rank(i+1)>=ssa_weapon_order_rank((unsigned)request)){select=0;break;}
        }
        next_inventory.weapons |= (uint16_t)bit;
        if (ssa_inventory_add_ammo(&next_inventory, rules, (unsigned)request,
                                   rules->ammo_capacity[request - 1] >> 1,
                                   player_count) < 0) return -1;
        if (select) {
            next_control.requested = (uint16_t)request;
            next_control.flags |= 0x2100u;
        }
    } else if (request) {
        if ((pc_order?ssa_weapon_cycle_pc:ssa_weapon_cycle)(&next_inventory, &next_control, request == 254 ? 1 : -1) < 0)
            return -1;
        next_control.flags |= 0x2100u;
    }
    *inventory = next_inventory;
    *control = next_control;
    return 0;
}

int ssa_weapon_cycle_pc(const SsaInventory *inventory,SsaWeaponControl *control,int direction) {
    unsigned slot,i;
    if(!inventory || !control || (direction!=1 && direction!=-1) ||
       (inventory->weapons&0xfc00u) || control->requested<1 || control->requested>10)return -1;
    slot=ssa_weapon_order_rank(control->requested);
    for(i=0;i<10;++i){unsigned id;
        slot=(slot+(direction==1?1:9))%10;id=ssa_weapon_order_id(slot);
        if((inventory->weapons&(1u<<(id-1))) && inventory->ammo[id-1]){
            control->requested=(uint16_t)id;return (int)id;
        }
    }
    return -1;
}
int ssa_weapon_update(SsaInventory *inventory,SsaWeaponControl *control,
    const SsaInventoryRules *rules,unsigned player_count) {
    return weapon_update(inventory,control,rules,player_count,0);
}
int ssa_weapon_update_pc(SsaInventory *inventory,SsaWeaponControl *control,
    const SsaInventoryRules *rules,unsigned player_count) {
    return weapon_update(inventory,control,rules,player_count,1);
}
