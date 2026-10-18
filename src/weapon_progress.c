#include "ssa/weapon_progress.h"
int ssa_weapon_progress_capture(SsaWeaponProgress *saved,SsaWeaponProgressStats stats[10],
    const SsaInventory *inventory,const SsaWeaponControl *control,uint8_t setting)
{
    unsigned i;
    if(!saved || !stats || !inventory || !control)return -1;
    saved->current=control->current;saved->weapons=inventory->weapons;
    for(i=0;i<10;++i)saved->ammo[i]=inventory->ammo[i];
    saved->requested=control->requested;
    for(i=0;i<10;++i){
        stats[i].owned=(inventory->weapons>>i)&1;stats[i].ammo=inventory->ammo[i];
        if(setting)stats[i].shots=stats[i].hits=0;
    }
    return 1;
}
int ssa_weapon_progress_restore(SsaInventory *inventory,SsaWeaponControl *control,
    SsaWeaponObject *object,const SsaWeaponProgress *saved)
{
    unsigned i;
    if(!inventory || !control || !object || !saved)return -1;
    control->current=saved->current;
    inventory->weapons=saved->weapons;
    for(i=0;i<10;++i)inventory->ammo[i]=saved->ammo[i];
    control->requested=(uint16_t)control->current;
    /* FC04 can leave the sprite unchanged for an invalid animation. The
     * original caller still completes its inventory copy in that case. */
    return ssa_weapon_animation_set(object,(uint16_t)(control->current-1u),3)<0?-1:1;
}
