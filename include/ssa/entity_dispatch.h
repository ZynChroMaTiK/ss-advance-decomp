#ifndef SSA_ENTITY_DISPATCH_H
#define SSA_ENTITY_DISPATCH_H
#include "ssa/entity_pool.h"
#include "ssa/world.h"

typedef struct SsaEntityDispatch {
    SsaEntityPool *pool;
    /* All 100 stable slot addresses, including unoccupied payloads. */
    SsaEntity *const *entities;
    SsaEntity *current;             /* 0300443C */
    uint32_t frame_counter;         /* 0300461C */
    uint32_t walk_count;            /* 03004358 */
    uint32_t path_count;            /* 030000B0, reset by 083E4 */
    /* The physical halfword immediately after active_slots[99] in the ROM
     * layout. The final iteration of a full pool reads it for auxiliary data.
     * Do not infer this word from active_count or replace it with a sentinel. */
    uint16_t active_tail;
    uint16_t auxiliary[101];        /* 03004370, with trailing zero */
    unsigned auxiliary_count;
    unsigned updating;
} SsaEntityDispatch;

typedef struct SsaEntityDispatchOps {
    /* entry is the original even routine address selected by04E04. Return
     * >=0 success/-1 failure. Controllers may allocate/release through pool
     * APIs, mutate payloads/counters and redirect current to another stable
     * slot. Keep all bindings/storage stable; do not reenter this dispatcher.
     * No callback runs for unhandled types, including zero/dead payloads. */
    int (*handler)(void *,SsaEntityDispatch *,uint32_t entry,unsigned slot);
    void *context;
} SsaEntityDispatchOps;
/* Original 128-entry table64..E3; zero means no handler. */
uint32_t ssa_entity_dispatch_handler(uint16_t type);
/* Whole04E04 plus083E4 reset; 0 success/-1 invalid or callback failure.
 * Frame count increments with wrap; walk/path counters clear before handlers.
 * Re-read active_count every iteration. A swap-release may skip the moved
 * actor; appended actors may run this frame. The auxiliary entry is NEXT
 * physical active index+1 when current->type remains nonzero, not this slot.
 * Successful calls restore current to slot0. Native callback errors preserve
 * earlier effects, terminate the partial auxiliary list and also restore slot0.
 * Invalid entry arguments return unchanged. Contents after the terminator stay
 * stale, matching the original. No controller, freeze/thaw or animation code
 * is implied by this scheduler; every selected handler is required. */
int ssa_entity_dispatch_step(SsaEntityDispatch *,const SsaEntityDispatchOps *);
#endif
