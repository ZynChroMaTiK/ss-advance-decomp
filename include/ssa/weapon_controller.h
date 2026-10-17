#ifndef SSA_WEAPON_CONTROLLER_H
#define SSA_WEAPON_CONTROLLER_H
#include "ssa/weapon_animation.h"
#include "ssa/world.h"

typedef struct SsaWeaponRules {
    uint8_t continuous[10], period[10];
    uint16_t range[10];
} SsaWeaponRules;
/* Data-only decoding of records0844BC8C; no ROM code execution. */
int ssa_weapon_rules_load(SsaWeaponRules *, const uint8_t *, size_t);
typedef struct SsaWeaponAimResult {
    SsaEntity *target;
    int32_t slope, marker, fraction;
} SsaWeaponAimResult;
/* Projection of fields modified after original109F0/5170 allocations. The
 * allocation callback owns this stable storage and commits it on INSERT or
 * PROJECTILE_COMMIT. It must never return a live gameplay body as scratch. */
typedef struct SsaWeaponEffect {
    int32_t x,y,z,vx,vy,vz,travel,radius;
    SsaEntity *target;
    uint16_t type,flags;
    uint8_t state,owner,weapon;
} SsaWeaponEffect;
enum SsaWeaponEventKind {
    SSA_WEAPON_SOUND_PLAY, SSA_WEAPON_SOUND_STOP, SSA_WEAPON_PARTICLE,
    SSA_WEAPON_HITSCAN, SSA_WEAPON_PROJECTILE_CREATE,
    SSA_WEAPON_PROJECTILE_COMMIT, SSA_WEAPON_EFFECT_ALLOCATE,
    SSA_WEAPON_EFFECT_ANIMATE, SSA_WEAPON_EFFECT_INSERT,
    SSA_WEAPON_EXPLOSION, SSA_WEAPON_REQUEST
};
typedef struct SsaWeaponEvent {
    enum SsaWeaponEventKind kind;
    int32_t args[8];
    SsaEntity *target;
    SsaWeaponEffect *effect;
    int32_t result;
} SsaWeaponEvent;
/* Event args, in order (remaining args zero):
 * SOUND_PLAY/STOP: sound ID; PLAY result is original audio return value.
 * PARTICLE: ID, captured source X,Y,Z.
 * HITSCAN: start X offset,Y offset,Z offset, unsigned heading, range,slope,damage.
 * PROJECTILE_CREATE: type,distance,side offset,Z offset,signed heading,slope.
 * PROJECTILE_COMMIT: effect points to the allocation after native field writes.
 * EFFECT_ALLOCATE: no args; NULL effect means original pool exhaustion.
 * EFFECT_ANIMATE: animation,period; effect contains complete recovered fields.
 * EFFECT_INSERT: effect from allocation/animation; target is attached entity.
 * EXPLOSION: radius,damage,third original argument (0,1000,0 here).
 * REQUEST: wrapped32 request register value, also written to game.request.
 * Native callbacks may implement synchronous attacks, but must not reenter the
 * controller or mutate its control/object/rules or invalidate source/target.
 * Allocation projections remain callback-owned and live for this update.
 */
typedef struct SsaWeaponController {
    SsaInventory *inventory;
    SsaWeaponControl *control;
    SsaWeaponObject *object;
    SsaEntity *source;
    int16_t source_health;
    uint16_t held,pressed,heading,cooldown[10];
    int32_t aim_slope;
    SsaWeaponAimResult aim;
    unsigned updating;
} SsaWeaponController;
typedef struct SsaWeaponGame {
    uint32_t frame, shots[10];
    uint16_t flags,local_flags;
    uint8_t ticks,players,motion_index;
    uint32_t request;
} SsaWeaponGame;
typedef struct SsaWeaponControllerOps {
    /* Result retains marker/fraction between attempts; target/slope replaced.
     * All rays start at source plus (0,0,0x600000). */
    int (*aim)(void *, const SsaWeaponController *, uint16_t heading,
               int32_t range, SsaWeaponAimResult *);
    /* CREATE must return effect; ALLOCATE may return NULL (original pool full).
     * All other callbacks succeed >=0 or fail -1. SOUND_PLAY returns original
     * audio result in event.result. Earlier effects persist on failure. */
    int (*event)(void *, const SsaWeaponController *, SsaWeaponEvent *);
    void *context;
} SsaWeaponControllerOps;
/* Original116E4, with aim, attacks/allocations and presentation explicit.
 * Every update aims, including idle/denied fire. Consumes prior object.done;
 * run global animation ticking AFTER all gameplay updates. The wrapper122AC
 * calls cooldowns, near-wall interaction, weapon acquisition, this function,
 * then switch. Flags/keys are distinct from held(+A4)/pressed(+A6). */
int ssa_weapon_controller_step(SsaWeaponController *, SsaWeaponGame *,
                               const SsaWeaponRules *, const SsaWeaponControllerOps *);
/* PC extension: use the supplied ray without original adjacent-yaw retries. */
int ssa_weapon_controller_step_free(SsaWeaponController *, SsaWeaponGame *,
                               const SsaWeaponRules *, const SsaWeaponControllerOps *);
/* PC automatic cadence: 0 defers weapon1/4/5 attack/ammo/stats only; positive
 * ticks applies accumulated single-player damage on the admitted attack.
 * Aim/start/release still run every update. Other weapons retain ROM behavior. */
int ssa_weapon_controller_step_clocked(SsaWeaponController *,SsaWeaponGame *,
    const SsaWeaponRules *,const SsaWeaponControllerOps *,unsigned attack_ticks,unsigned free_aim);
/* PC Time Strike policy: aim callback supplies only eligible living enemies.
 * Missing target at start or attack finishes a visual-only firing animation,
 * without ammo/stats/sound/effects or a later deferred shot.
 * Other weapons and the original entry points keep recovered behavior. */
int ssa_weapon_controller_step_targeted(SsaWeaponController *,SsaWeaponGame *,
    const SsaWeaponRules *,const SsaWeaponControllerOps *,unsigned free_aim);
/* Original12160: lower, commit requested, then raise; original12200 cooldowns.
 * Return1 success,-1 invalid/busy. No callbacks. */
int ssa_weapon_controller_switch(SsaWeaponController *, uint8_t ticks);
int ssa_weapon_controller_cooldowns(SsaWeaponController *, uint8_t ticks);
#endif
