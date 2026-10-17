#include "ssa/weapon_loadout.h"
#include <limits.h>
#include <string.h>
static int16_t half(uint16_t n) {return n<=INT16_MAX?(int16_t)n:(int16_t)(-1-(int32_t)(UINT16_MAX-n));}
int ssa_weapon_loadouts_load(SsaWeaponLoadouts *out,const void *data,size_t size) {
    const uint8_t *rom=data;
    SsaWeaponLoadouts next;
    unsigned row,weapon;
    if(!out || !rom || size<0x44bcf0u+(SSA_WEAPON_LOADOUT_ROWS+1u)*20u) return -1;
    memset(&next,0,sizeof(next));
    for(row=0;row<SSA_WEAPON_LOADOUT_ROWS;++row)
        for(weapon=0;weapon<SSA_WEAPON_COUNT-1;++weapon) {
            size_t at=0x44bcf0u+(row+1u)*20u+weapon*2u;
            next.ammo[row][weapon]=half((uint16_t)(rom[at]|(unsigned)rom[at+1]<<8));
        }
    next.loaded=1;*out=next;return 1;
}
int ssa_weapon_loadout_initialize(SsaWeaponLoadout *state,SsaWeaponLoadoutPlayer *players,
    unsigned count,const SsaWeaponLoadouts *loadouts,const SsaInventoryRules *rules,
    unsigned setting,uint8_t god_mode,const SsaWeaponLoadoutOps *ops) {
    unsigned i,j;
    int result=-1;
    if(!state || state->updating || count>4 || (count && !players) || !loadouts ||
       !loadouts->loaded || !rules || setting>SSA_WEAPON_LOADOUT_ROWS || !ops || !ops->allocate) return -1;
    for(i=0;i<count;++i) {
        if(!players[i].control) return -1;
        if((players[i].control->keys&4) && (!players[i].inventory || !players[i].cooldowns)) return -1;
    }
    state->updating=1;
    for(i=0;i<count;++i) {
        SsaWeaponLoadoutPlayer *p=&players[i];
        SsaWeaponObject *object=NULL;
        if(!(p->control->keys&4)) continue;
        if(ops->allocate(ops->context,i,&object)<0 || !object) goto done;
        p->control->flags=0;p->object=object;p->inventory->weapons=1;
        object->x=0x7800;object->y=0xa000;object->type=2;object->layer=14;object->owner=(uint8_t)(i+1);
        p->inventory->pending_count=0;p->inventory->ammo[0]=999;
        if(setting) {
            for(j=1;j<SSA_WEAPON_COUNT;++j) {
                int16_t ammo=loadouts->ammo[setting-1][j-1];
                if(ammo) {
                    int16_t cap=half(rules->ammo_capacity[j]);
                    p->inventory->ammo[j]=ammo<cap?ammo:cap;
                    p->inventory->weapons|=(uint16_t)(1u<<j);
                } else p->inventory->ammo[j]=0;
            }
        }
        if(god_mode) {
            for(j=1;j<SSA_WEAPON_COUNT;++j) p->inventory->ammo[j]=999;
            p->inventory->weapons=0x3ff;
        }
        p->control->requested=1;
        if(ssa_weapon_cycle(p->inventory,p->control,-1)<0) goto done;
        p->control->current=p->control->requested;
        if(ssa_weapon_animation_set(object,(uint16_t)(p->control->current-1u),3)!=1) goto done;
        p->cooldowns[i]=0;
    }
    result=1;
done:
    state->updating=0;return result;
}
