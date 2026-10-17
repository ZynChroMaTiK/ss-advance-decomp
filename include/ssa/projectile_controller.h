#ifndef SSA_PROJECTILE_CONTROLLER_H
#define SSA_PROJECTILE_CONTROLLER_H
#include "ssa/projectile.h"

typedef struct SsaProjectileController {
    SsaProjectile *projectile;
    unsigned updating;
    uint8_t destroy_requested;
} SsaProjectileController;
typedef struct SsaProjectileControllerGame {
    uint32_t credit, hit_weapon;
} SsaProjectileControllerGame;
typedef enum SsaProjectileEvent {
    SSA_PROJECTILE_ANIMATION,
    SSA_PROJECTILE_EXPLOSION,
    SSA_PROJECTILE_PARTICLE,
    SSA_PROJECTILE_DESTROY
} SsaProjectileEvent;
typedef struct SsaProjectileControllerOps {
    /* Whole 10A20: 1 continues, 0 impacts, -1 failure. May mutate projectile. */
    int (*motion)(void *, SsaProjectile *);
    /* >=0 success, <0 failure. ANIMATION(id,period,0), EXPLOSION(radius,
     * damage,include_players), PARTICLE(id,0,0), DESTROY(0,0,0).
     * Animation fields are already set at the event. Particle XYZ comes from
     * projectile.entity. Destroy is deferred until step returns; successful
     * event sets destroy_requested, so storage must remain stable throughout.
     * Events observe state and may not mutate/reenter the controller or body.
     */
    int (*event)(void *, const SsaProjectileController *, SsaProjectileEvent,
                 int32_t, int32_t, int32_t);
    void *context;
} SsaProjectileControllerOps;
/* Complete dispatch/controllers for 65,66,67,69,6B, verified from04E04 table.
 * 0 updated, 1 deferred destruction, -1 invalid or callback failure. Earlier
 * changes persist on failure. Explosion credit/hit_weapon clear after its
 * callback even on failure (native cleanup); successful original calls clear
 * them before particle/destruction. Unsupported types are rejected unchanged.
 */
int ssa_projectile_controller_step(SsaProjectileController *,
    SsaProjectileControllerGame *, const SsaProjectileControllerOps *);
#endif
