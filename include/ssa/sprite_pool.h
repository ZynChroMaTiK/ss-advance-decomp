#ifndef SSA_SPRITE_POOL_H
#define SSA_SPRITE_POOL_H
#include "ssa/sprite_dispatch.h"

/* Original84-slot sprite pool. Borrows dispatch and its84 distinct stable
 * object bindings; no payload allocation or host pointer overlay. The same
 * objects may be bound to weapon runtime before initialization. */
typedef struct SsaSpritePool {
    SsaSpriteDispatch *dispatch;
    uint32_t free_slots[SSA_SPRITE_CAPACITY];
    uint32_t free_count;
    unsigned loaded,updating;
} SsaSpritePool;
/* Bind and run completeFA78/FAE8/101C4 reset. Zero-init pool. Clears all84
 * objects and counter/submitted_count, retains stale active/submitted arrays
 * and dispatcher current/state/index. No reset during dispatch. Return1/-1. */
int ssa_sprite_pool_init(SsaSpritePool *,SsaSpriteDispatch *);
int ssa_sprite_pool_reset(SsaSpritePool *);
/* F9B4: only writes object slot/active_index; payload is already initialized
 * by reset or earlier release. Return1 allocated,0 full,-1 invalid. Output
 * set NULL on valid exhaustion. Earlier object storage stays stable. */
int ssa_sprite_pool_allocate(SsaSpritePool *,SsaWeaponObject **);
/* FA08: swap-remove active slot, push free slot, initialize released payload.
 * Does not adjust dispatch.current/current_index; live F85C rereads type0.
 * May run in a dispatcher handler; never mutate its object bindings. */
int ssa_sprite_pool_release(SsaSpritePool *,SsaWeaponObject *);
/* FB5C andFBA0. Range endpoints are inclusive uint16. FBA0 captures original
 * count, keeps cursor on swapped-in entries, clears matching objects. */
int ssa_sprite_pool_mark(SsaSpritePool *);
int ssa_sprite_pool_remove_types(SsaSpritePool *,uint16_t first,uint16_t last);
#endif
