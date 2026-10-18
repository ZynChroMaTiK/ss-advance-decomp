#ifndef SSA_WEAPON_RUNTIME_H
#define SSA_WEAPON_RUNTIME_H
#include "ssa/weapon_controller.h"
#include "ssa/weapon_hit.h"
#include "ssa/combat_runtime.h"
#include "ssa/weapon_loadout.h"
typedef struct SsaProjectileRuntime SsaProjectileRuntime;
enum {
    SSA_CHAINSAW_PER_CALL=0,SSA_CHAINSAW_TOMMY_CADENCE=1,SSA_CHAINSAW_GBA_CADENCE=2,
    /* Original first-level trace:85 steady attack intervals /584 refreshes. */
    SSA_CHAINSAW_GBA_INTERVALS=85,SSA_CHAINSAW_GBA_REFRESHES=584
};

typedef struct SsaWeaponRuntime {
    SsaPlayerGameplay *player;
    SsaCombatRuntime *combat;
    SsaProjectileRuntime *projectiles;
    const SsaInventoryRules *inventory_rules;
    const SsaWeaponRules *rules;
    const SsaWeaponAnimations *animations;
    SsaWeaponController controller;
    SsaWeaponObject object;
    SsaWeaponAim trace;
    SsaMapCollision interaction;
    uint32_t hits[10];
    uint8_t *credited_kills; /* Multiplayer: this shooter's persistent kill byte. */
    unsigned updating;
    unsigned unlimited_ranged; /* PC policy; weapon1 retains ROM melee reach. */
    /* PC weapon4/5 (optionally1) cadence in60Hz ticks (or GBA refreshes below); 0 retains ROM per-call
     * firing. Countdown/elapsed freeze with pause and reset on release/switch. */
    unsigned automatic_period,automatic_remaining,automatic_elapsed,automatic_weapon;
    /* Original measured frame cadence: period counts GBA refreshes when set.
     * Advance the rational GBA clock from60Hz host simulation, never rendering. */
    unsigned automatic_gba_clock;
    /* PC policy: double minigun GBA clock; chainsaw mode0 per-call,1 Tommy
     * cadence,2 measured original GBA average (requires GBA clock). */
    unsigned double_minigun,clocked_chainsaw;
    unsigned chainsaw_range; /* PC trace units;0 retains the ROM range. */
    unsigned automatic_damage_phase; /* Fractional GBA ticks, mode2 chainsaw. */
    uint64_t automatic_phase;
    /* PC click buffering: retain a press until one attack is committed.
     * The pending requested weapon is0 when empty; release alone preserves it. */
    unsigned buffer_fire,fire_pending_weapon;
    /* PC policy: queue changes until firing completes. Continuous weapons
     * coast through the current animation cycle without additional attacks. */
    unsigned finish_fire_before_switch,switch_wait;
    unsigned time_strike_requires_target; /* PC weapon9: valid living enemy or no shot. */
    unsigned time_strike_empty_pending; /* Buffered visual-only gesture until ready. */
    unsigned fire_inhibited; /* PC variant handoff: finish the current shot, buffer new clicks. */
    unsigned double_colt,switch_blocked; /* PC dual firing clock / independent hand recoil gate. */
    unsigned colt_pair,colt_pair_pending; /* PC dual input queues two alternating starts. */
    unsigned alternate_shotgun; /* PC fourteen-pellet pattern,5base damage each. */
    unsigned shotgun_animating,shotgun_elapsed; /* Alternate cycle:105ticks/1.75s. */
} SsaWeaponRuntime;
typedef struct SsaWeaponRuntimeOps {
    const SsaCombatRuntimeOps *combat;
    /* Sound, particles, requests and unbound projectile/effect creation.
     * HITSCAN and EXPLOSION are consumed natively, never forwarded. With the
     * projectile backend bound, CREATE/COMMIT are also consumed natively. Unsupported
     * requests must return -1, not silently succeed. Observe player/world state;
     * allocation events may supply their separately owned stable effect object.
     * No reentry or direct mutation/release of registered bodies. */
    int (*event)(void *,const SsaWeaponRuntime *,SsaWeaponEvent *);
    /* Original9D4C boundary: NULL target is wall impact, otherwise entity hit.
     * Called before damage. Return>=0/-1; earlier effects persist on failure. */
    int (*impact)(void *,const SsaWeaponRuntime *,const SsaWeaponHit *);
    void *context;
} SsaWeaponRuntimeOps;
/* Bind an existing initialized current/requested weapon1..10. Does not grant
 * a loadout. Copy weapon_view and select idle animation; all referenced storage
 * must remain stable. Zero-init; free before gameplay/combat/world. Return0/-1. */
int ssa_weapon_runtime_init(SsaWeaponRuntime *,SsaPlayerGameplay *,SsaCombatRuntime *,
    const SsaInventoryRules *,const SsaWeaponRules *,const SsaWeaponAnimations *);
/* Start original1240C loadouts and bind active indexed players. All weapon
 * runtimes must be unbound and stable; gameplay/combat are already initialized.
 * Uses one embedded weapon object per player, not the original sprite pool.
 * Inactive players are untouched. Initial loadout changes and earlier successful
 * bindings persist on a later allocation failure; free bound runtimes explicitly.
 * No health or movement initialization is implied. Return0/-1. */
int ssa_weapon_runtimes_start(SsaWeaponRuntime *const *,SsaPlayerGameplay *const *,
    SsaCombatRuntime *,const SsaInventoryRules *,const SsaWeaponRules *,
    const SsaWeaponAnimations *,const SsaWeaponLoadouts *,unsigned setting,uint8_t god_mode);
int ssa_weapon_runtime_free(SsaWeaponRuntime *);
/* Optional native shared-pool projectile backend. Bind after credited_kills is
 * assigned. Keep this weapon and its counters alive until projectile runtime
 * teardown; free projectile runtime BEFORE weapons. Return0/-1. */
int ssa_weapon_runtime_bind_projectiles(SsaWeaponRuntime *,SsaProjectileRuntime *);
/* Bind the existing gameplay WEAPON_ANIMATION respawn callback to this helper.
 * It copies weapon_view then sets animation/period and weapon_action. It is
 * permitted during player respawn, but not during a weapon update. */
int ssa_weapon_runtime_respawn(SsaWeaponRuntime *,uint16_t animation,uint16_t period);
/* F150 visibility/pause wrapper plus122AC: cooldowns, near-wall lowering12230,
 * selection11604, controller116E4, lower/commit/raise12160. Call after player/
 * enemy updates, movers and script VM (original F85C sprite stage); do not
 * run concurrently with player/enemy/level updates. game fields are explicit
 * GAME projections; local_flags is sampled from combat's display-local player.
 * Source stays set to this body; credit and selected-hit-stat clear on exit as
 * in ROM (also on failure). Hits are indexed by weapon BEFORE selection.
 * Multiplayer requires credited_kills bound to THIS player's persistent game
 * counter before step. The shared damage counter pointer is restored on exit.
 * Animation is separate: tick all objects once AFTER all weapon controllers.
 * held/pressed are original A4/A6 button masks (fire bit1), not movement flags.
 * Return0/-1. Native scope locks all bound players/spawn/combat across callbacks. */
int ssa_weapon_runtime_step(SsaWeaponRuntime *,SsaWeaponGame *,uint16_t held,
    uint16_t pressed,const SsaWeaponRuntimeOps *);
int ssa_weapon_runtime_animate(SsaWeaponRuntime *,uint16_t game_flags,unsigned ticks);
#endif
