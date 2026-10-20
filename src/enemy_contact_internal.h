#ifndef SSA_ENEMY_CONTACT_INTERNAL_H
#define SSA_ENEMY_CONTACT_INTERNAL_H
#include "ssa/script_spawn.h"
/* PC contact hostility is symmetric. Preserve ordinary enemy/player rules
 * and reject dead or retired bodies using canonical actor health. */
static int ssa_enemy_converted_contact(const SsaScriptSpawn *spawn,
    const SsaEntity *source,const SsaEntity *target)
{
    unsigned i;
    if(!target || source==target || source->type<0xcd || source->type>0xd8 ||
       target->type<0xcd || target->type>0xd8 ||
       ((source->body.flags|target->body.flags)&(0x10|0x20)) ||
       !((source->body.flags|target->body.flags)&0x400) || target->owner!=spawn->world)return 0;
    for(i=0;i<100;++i)if(target==&spawn->actors[i].entity)
        return spawn->pool.occupied[i] && spawn->actors[i].owned && spawn->actors[i].activation.health>0;
    return 0;
}
#endif
