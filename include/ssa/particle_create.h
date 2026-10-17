#ifndef SSA_PARTICLE_CREATE_H
#define SSA_PARTICLE_CREATE_H
#include "ssa/projectile_create.h"

typedef struct SsaParticleRules {
    uint16_t type[16], animation[16], period[16];
    unsigned loaded;
} SsaParticleRules;
typedef struct SsaParticleCreateOps {
    int (*allocate)(void *, SsaProjectile **);
    int (*animation)(void *, SsaProjectile *);
    int (*insert)(void *, SsaProjectile *);
    int (*sound)(void *, SsaProjectile *, unsigned);
    void *context;
} SsaParticleCreateOps;
/* Copies original 0844C07C/C0BC/C0FC tables. No retained ROM pointer. */
int ssa_particle_rules_load(SsaParticleRules *,const uint8_t *,size_t);
/* Whole14B60: allocation, animation, insertion, optional sound, then flags.
 * 1 created, 0 exhausted, -1 invalid/callback failure. Successful allocation
 * remains exposed through out on later failure; storage must stay stable.
 * Callbacks may initialize animation/insert this body, not release or reenter.
 * No source/owner is assigned: these are independent animated type96 effects. */
int ssa_particle_create(SsaProjectileCreator *,const SsaParticleRules *,unsigned,
    int32_t,int32_t,int32_t,const SsaParticleCreateOps *,SsaProjectile **out);
/* Whole14C58; 1 requests deferred destruction, 0 retains, -1 invalid/reentry.
 * Caller performs original037E0 unlink and051C8 release when 1 is returned. */
int ssa_particle_step(SsaProjectile *);

typedef struct SsaWeaponImpactState {
    uint32_t random_state;
    unsigned updating;
} SsaWeaponImpactState;
typedef struct SsaWeaponImpactOps {
    int (*particle)(void *,unsigned,int32_t,int32_t,int32_t);
    void *context;
} SsaWeaponImpactOps;
/* Whole9D4C selection. source is original03004760, motion_index GAME+68,
 * weapon is source player's current +78. target_hit is original r3!=0.
 * Return0/-1. LCG changes persist if particle creation fails. */
int ssa_weapon_impact(SsaWeaponImpactState *,const SsaEntity *source,
    unsigned motion_index,uint16_t weapon,int target_hit,int32_t x,int32_t y,
    int32_t z,const SsaWeaponImpactOps *);
#endif
