#include "ssa/pickup.h"

static void event(SsaPickupGame *g, enum SsaPickupEvent kind, unsigned a, unsigned b) {
    g->ops.event(g->ops.context,kind,a,b);
}

int ssa_pickup_touch_alternate(SsaPickupGame *g,const SsaPickupPlayer *p,SsaPickup *pickup,
                     uint32_t override_item,unsigned repeat_weapon) {
    static const uint8_t weapons[10]={2,3,4,6,0,5,10,8,9,7};
    static const uint8_t ammo_weapons[10]={2,3,8,0,6,10,4,5,9,7};
    static const uint8_t amounts[10]={10,10,1,0,5,5,50,50,5,5};
    SsaInventory *inv;
    uint32_t item;
    unsigned sound=0x35,message=0;
    int local,accepted=1;
    if (repeat_weapon>SSA_WEAPON_COUNT || !g || !p || !g->rules || !(inv=p->inventory) || !g->ops.event ||
        !g->ops.remove || (!override_item && !pickup) ||
        (override_item && !g->ops.nearest)) return -1;
    if ((uint16_t)(p->entity_type-0xc9u)>3u) return 0;
    local=p->body_player_index==g->local_player;
    if (!local && g->player_count>1 && !override_item) return 0;
    if (pickup && (pickup->trigger&0x100)) {
        event(g,SSA_PICKUP_TRIGGER,pickup->trigger&255u,0);
        pickup->trigger&=0xfeffu;
    }
    item=override_item ? override_item : pickup->item;
    if (item && g->player_count>1 && !override_item) {
        event(g,SSA_PICKUP_REQUEST,p->index,(uint16_t)item);
        return 2;
    }
    if (override_item) {
        SsaPickup *nearest=g->ops.nearest(g->ops.context,p);
        if (!nearest || nearest->item!=item) return 0;
        g->ops.remove(g->ops.context,nearest);
    }
    if (item>=0x20 && item<=0x29 && weapons[item-0x20]) {
        unsigned weapon=weapons[item-0x20];
        accepted=ssa_inventory_queue_weapon(inv,weapon);
        if(!accepted && weapon==repeat_weapon && inv->pending_count<SSA_PENDING_WEAPON_CAPACITY){
            inv->pending_weapons[inv->pending_count++]=weapon;accepted=1;
        }
        message=0x8b+weapon;
        if (weapon==8) sound=0x1d;
    } else if (item>=0x2a && item<=0x2f) {
        static const int amount[3]={25,50,100};
        unsigned n=(unsigned)item-0x2a;
        accepted=(n<3 ? inv->stat_c6 : inv->stat_c4)<100;
        if (accepted) {
            if (n<3) ssa_inventory_add_stat_c6(inv,amount[n]);
            else ssa_inventory_add_stat_c4(inv,amount[n-3]);
        }
        message=0x97+n;
    } else if (item>=0x30 && item<=0x32) {
        unsigned n=(unsigned)item-0x30;
        accepted=inv->health<(n==2 ? 200 : 100);
        if (accepted) ssa_inventory_add_health(inv,n==0 ? 25 : n==1 ? 50 : 100);
        sound=0x32+n; message=n==2 ? 0xa0 : 0x9d+n;
    } else if (item==0x33 || item==0x37) {
        if (ssa_inventory_fill_ammo(inv,g->rules,g->player_count)<0) return -1;
        if (item==0x37) ssa_inventory_add_health(inv,100);
        message=item==0x33 ? 0x96 : 0xa4;
    } else if (item>=0x34 && item<=0x3d) {
        unsigned n=(unsigned)item-0x34;
        accepted=ssa_inventory_add_ammo(inv,g->rules,ammo_weapons[n],amounts[n],g->player_count);
        message=0xa1+n;
        if (n<2) sound=0x2a;
    } else if (item!=0x3f) {
        sound=6; message=0xab;
    }
    if (accepted<0) return -1;
    if (!accepted) return 0;
    if (local) {
        event(g,SSA_PICKUP_SOUND,sound,0);
        if (message) event(g,SSA_PICKUP_MESSAGE,message,0);
    }
    if (!override_item) {
        ++g->collected;
        g->ops.remove(g->ops.context,pickup);
    }
    return 1;
}
int ssa_pickup_touch(SsaPickupGame *g,const SsaPickupPlayer *p,SsaPickup *pickup,uint32_t override_item)
{return ssa_pickup_touch_alternate(g,p,pickup,override_item,0);}
