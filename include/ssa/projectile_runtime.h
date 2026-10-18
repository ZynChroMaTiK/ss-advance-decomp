#ifndef SSA_PROJECTILE_RUNTIME_H
#define SSA_PROJECTILE_RUNTIME_H
#include "ssa/projectile_create.h"
#include "ssa/projectile_motion.h"
#include "ssa/projectile_controller.h"
#include "ssa/projectile_bounce.h"
#include "ssa/combat_runtime.h"
#include "ssa/weapon_controller.h"
#include "ssa/particle_create.h"
/* Measured original E3 scene:9 steady updates /60 hardware refreshes. */
enum {SSA_TIME_STRIKE_GBA_INTERVALS=3,SSA_TIME_STRIKE_GBA_REFRESHES=20};
typedef struct SsaProjectileRuntime {
    SsaCombatRuntime *combat;
    const SsaMathTables *math;
    SsaProjectile projectiles[100];
    SsaWeaponEffect effects[100];
    uint8_t active[100];
    uint32_t *hit_counts[4];
    uint8_t *credited_kills[4];
    SsaProjectileCreator creator;
    SsaProjectileBounce bounce;
    unsigned updating;
    unsigned unlimited_player_range; /* PC shots have no age/distance expiry. */
    unsigned ballistic_player_rockets; /* PC type65 detaches from view at launch. */
    unsigned ballistic_player_prion; /* PC type69 launches on the sight ray and detaches. */
    unsigned ballistic_player_cannon; /* PC type67 detaches from view at launch. */
    unsigned rolling_cannonballs; /* PC floor contact continues XY motion; airborne gravity stays unchanged. */
    unsigned ceiling_impacts; /* PC rockets/Prion terminate on vertical ceiling contact. */
    const SsaParticleRules *particle_rules;
    int (*particle_sound)(void *,const struct SsaProjectileRuntime *,const SsaProjectile *,unsigned);
    void *particle_context;
    unsigned time_strike_gba_clock; /* PC E3 gravity uses original update cadence. */
    uint64_t time_strike_phase[100];
    uint8_t time_strike_tick_phase[100];
} SsaProjectileRuntime;
typedef struct SsaProjectileRuntimeOps {
    const SsaCombatRuntimeOps *combat;
    /* Required presentation boundary. Observe only; must not mutate/reenter
     * runtime, world, pool, actors or players. Earlier effects persist on error. */
    int (*particle)(void *, const SsaProjectileRuntime *, unsigned,
                    int32_t, int32_t, int32_t);
    void *context;
} SsaProjectileRuntimeOps;
/* Zero-initialize. Shared script_spawn pool, stable storage. Does not own combat/math. Free this
 * adapter before bound weapons/combat/spawn/world. Do not release/reuse these
 * actors through script_spawn, or release/reuse a target while E2 follows it.
 * Target presence/type is validated, but same-type slot reuse has no generation
 * identity in the original projection. Activation health/timer/state are
 * canonical between steps; projectile speed/source/owner/weapon stay here.
 * hit_counts points to ten persistent counters per player. Multiplayer credit
 * requires that shooter's persistent credited_kills byte. Keep both arrays and
 * bound weapon storage alive until this runtime is freed. Type69 conversion
 * sets target parameter34=30 and preserves parameter36. The CD runtime consumes
 * these canonical actor fields for periodic health drain; shared navigation
 * implements flag400 retargeting. This is not a freeze/thaw movement gate.
 * No actor animation playback is provided. entity_runtime separately supplies
 * the native scheduler with partial controller bindings, not every type. */
int ssa_projectile_runtime_init(SsaProjectileRuntime *,SsaCombatRuntime *,const SsaMathTables *);
int ssa_projectile_runtime_free(SsaProjectileRuntime *);
/* 0 updated,1 destroyed,-1 invalid/error. Types65..6B,96,E3 and attachedE2.
 * Newly spawned slots must be visited according to original active-pool order. */
int ssa_projectile_runtime_step(SsaProjectileRuntime *,unsigned slot,unsigned ticks,
                               const SsaProjectileRuntimeOps *);
/* Read-only PC E3 presentation: interpolate toward its next clocked position.
 * Collision, landing, damage and lifetime still use the unchanged simulation. */
int ssa_projectile_runtime_render_z(const SsaProjectileRuntime *,unsigned slot,int32_t *z);
int ssa_projectile_runtime_release(SsaProjectileRuntime *,unsigned slot);
/* Optional native14B60 particle backend. Rules and callback context stay stable
 * until runtime free. Sound is an explicit presentation service, not allocation.
 * With this bound, projectile impact particles are created natively; the old
 * RuntimeOps.particle boundary is used only by unbound legacy callers. */
int ssa_projectile_runtime_bind_particles(SsaProjectileRuntime *,const SsaParticleRules *,
    int (*sound)(void *,const SsaProjectileRuntime *,const SsaProjectile *,unsigned),void *);
/* Outside existing combat scopes: 0 success (including pool exhaustion), -1
 * failure. Native96 animation is advanced by the shared actor-animation stage. */
int ssa_projectile_runtime_particle(SsaProjectileRuntime *,unsigned,int32_t,int32_t,int32_t,
    const SsaCombatRuntimeOps *);
/* Native weapon/combat callbacks or script_spawn's live VM particle callback
 * only. Requires the matching combat+spawn or level+script+spawn update locks.
 * No query may be active. This is creation, not a presentation observer. */
int ssa_projectile_runtime_particle_held(SsaProjectileRuntime *,unsigned,int32_t,int32_t,int32_t);
#endif
