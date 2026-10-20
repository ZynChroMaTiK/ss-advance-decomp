#ifndef SSA_PICKUP_RUNTIME_H
#define SSA_PICKUP_RUNTIME_H
#include "ssa/combat_runtime.h"
#include "ssa/pickup.h"
typedef struct SsaPickupRuntime SsaPickupRuntime;
typedef struct SsaPickupRuntimeOps {
    /* SOUND/MESSAGE/REQUEST only; trigger queueing is native. Observe-only,
     * void as in the original dispatcher. Do not mutate/reenter bound state. */
    void (*event)(void *,const SsaPickupRuntime *,unsigned player,
                  enum SsaPickupEvent,unsigned,unsigned);
    void *context;
} SsaPickupRuntimeOps;
struct SsaPickupRuntime {
    SsaCombatRuntime *combat;
    SsaPickupGame game;
    SsaPickupRuntimeOps ops;
    SsaPickup current;
    unsigned slot,player,loaded,updating,failed;
    unsigned repeat_weapon; /* PC variant pickup policy;0 keeps original behavior. */
    unsigned pc_pickups; /* Store unowned ammo and use duplicate weapons for ammo. */
    unsigned collect_owned_weapons; /* Consume duplicate guns even at full ammo. */
};
/* Borrow stable combat/spawn/player/rules storage. Binds an unused world
 * execute_special callback; refuses to replace an existing one. Initialize
 * after map adoption and combat binding; free before those owners. Returns0/-1.
 * Item is actor.animation (+58), not activation.state (+3E). Trigger is
 * script.trigger (+68). Inventory.health is authoritative; mirror is updated.
 * Normal contacts are supported; multiplayer REQUEST is forwarded but packet
 * replay/nearest pickup removal is not implemented by this adapter. */
int ssa_pickup_runtime_init(SsaPickupRuntime *,SsaCombatRuntime *,
                          const SsaInventoryRules *,const SsaPickupRuntimeOps *);
int ssa_pickup_runtime_free(SsaPickupRuntime *);
/* Exact SsaSpecialExecute signature. Called only by an active world query for
 * its currently executing special. Only that owned E1 may be unlinked/released.
 * Static actor storage is cleared, never freed/reused during the query. Removal
 * ends cell traversal. Script queue failure is latched as failed and returns-1;
 * the original void-callback dispatcher can retain subsequent inventory effects,
 * but removal/presentation is suppressed after failure. No rollback promised. */
int ssa_pickup_runtime_execute(void *,const SsaEntity *,SsaEntity *);
#endif
