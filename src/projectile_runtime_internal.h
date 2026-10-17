#ifndef SSA_PROJECTILE_RUNTIME_INTERNAL_H
#define SSA_PROJECTILE_RUNTIME_INTERNAL_H
#include "ssa/projectile_runtime.h"
#include "combat_runtime_internal.h"
/* Called only inside this runtime's combat scope, already holding all players
 * and spawn. Handles projectile CREATE/COMMIT and weapon9 effect allocation,
 * animation/insertion, retaining projection storage for the call. */
int ssa_projectile_runtime_weapon(SsaProjectileRuntime *,SsaCombatScope *,
                                  SsaEntity *,SsaWeaponEvent *);
/* Existing shared combat locks held; runtime itself must be idle. Enemy aim
 * targets may be retained stable actor slots that have since been cleared or
 * reused. Sources must still be registered; arbitrary target pointers fail. */
int ssa_projectile_runtime_particle_scoped(SsaProjectileRuntime *,SsaCombatScope *,
    unsigned,int32_t,int32_t,int32_t);
int ssa_projectile_runtime_enemy_scoped(SsaProjectileRuntime *,SsaCombatScope *,
    const SsaProjectileCreateRequest *,const SsaEntity *target,int aimed);
/* Boss construction borrows the resulting projection through its subsequent
 * sound and vertical-velocity/state writes, then commits those fields. */
int ssa_projectile_runtime_enemy_create_scoped(SsaProjectileRuntime *,SsaCombatScope *,
    const SsaProjectileCreateRequest *,const SsaEntity *,int,SsaProjectile **);
int ssa_projectile_runtime_enemy_commit_scoped(SsaProjectileRuntime *,SsaCombatScope *,SsaProjectile *);
#endif
