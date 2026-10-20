#ifndef SSA_COMBAT_RUNTIME_INTERNAL_H
#define SSA_COMBAT_RUNTIME_INTERNAL_H
#include "ssa/combat_runtime.h"
/* Private synchronous composition interface. A scope exists only during run;
 * it owns combat, player and spawn update locks. It must never escape callback
 * storage or be exposed to presentation callbacks. World tracing can temporarily
 * take query_active, but must release it before applying hits/explosions. */
typedef struct SsaCombatScope SsaCombatScope;
static inline int ssa_combat_boss_projectile(const SsaCombatRuntime *r,
    const SsaEntity *projectile,const SsaEntity *owner)
{
    return r->boss_projectile_immunity && projectile && owner &&
        (owner->type==0xd7 || owner->type==0xd8) &&
        projectile->type>=0x64 && projectile->type<=0x6b;
}
int ssa_combat_scope_run(SsaCombatRuntime *, const SsaCombatRuntimeOps *,
    int (*run)(void *,SsaCombatScope *),void *);
int ssa_combat_scope_health(SsaCombatScope *,SsaEntity *,int16_t *);
int ssa_combat_scope_damage(SsaCombatScope *,SsaEntity *,const SsaEntity *,
    const SsaEntity *,uint32_t);
int ssa_combat_scope_melee(SsaCombatScope *,SsaEntity *,const SsaEntity *,uint32_t);
int ssa_combat_scope_explode(SsaCombatScope *,SsaEntity *,const SsaEntity *,
    const SsaEntity *,uint32_t,uint32_t,int);
#endif
