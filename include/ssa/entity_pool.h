#ifndef SSA_ENTITY_POOL_H
#define SSA_ENTITY_POOL_H
#include <stdint.h>

#define SSA_ENTITY_POOL_CAPACITY 100u

/* Index bookkeeping from 08005238/05170/051C8. Payloads have stable caller-owned
 * storage. The original free stack grows down, while the active list grows up.
 * Exposed arrays are for reading, not caller mutation. Entries beyond the live
 * active prefix (and before the live free suffix) intentionally remain stale.
 * positions models entity+3A; occupied supplies native double-release guards. */
typedef struct {
    uint16_t free_slots[SSA_ENTITY_POOL_CAPACITY];
    uint16_t active_slots[SSA_ENTITY_POOL_CAPACITY];
    uint16_t positions[SSA_ENTITY_POOL_CAPACITY];
    uint16_t active_count;
    uint8_t occupied[SSA_ENTITY_POOL_CAPACITY];
} SsaEntityPool;

/* Init returns 1, or -1 for NULL. Clear all caller payloads before use. Native
 * init additionally zeroes stale active-array bytes left unspecified by ROM. */
int ssa_entity_pool_init(SsaEntityPool *pool);
/* Returns 1 with slot, 0 when full, -1 on invalid arguments/corrupt metadata.
 * The output must not alias pool storage. Failure leaves output unchanged.
 * Original allocation does NOT clear payloads. free count is 100-active_count. */
int ssa_entity_pool_allocate(SsaEntityPool *pool, unsigned *slot);
/* Returns 1, or -1 without mutation on invalid slot/state. Swaps the final
 * active slot into this slot's position; then marks the released slot free.
 * Caller must unlink the payload from world lists BEFORE release, and clear it
 * AFTER success, as original 052B0 does. No group/script/world effects occur
 * inside the original release routine. No callback or reentry is performed. */
int ssa_entity_pool_release(SsaEntityPool *pool, unsigned slot);
#endif
