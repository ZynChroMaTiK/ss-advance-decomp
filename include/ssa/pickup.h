#ifndef SSA_PICKUP_H
#define SSA_PICKUP_H
#include "ssa/inventory.h"

/* Host views of the fields used by 0801B99C, not binary RAM overlays.
 * Keep objects alive throughout dispatch, including after remove(). */
typedef struct SsaPickup {
    uint16_t item, trigger;
    void *handle;
} SsaPickup;
typedef struct SsaPickupPlayer {
    SsaInventory *inventory;
    uint16_t entity_type;
    uint8_t index, body_player_index;
    void *handle;
} SsaPickupPlayer;
enum SsaPickupEvent {
    SSA_PICKUP_TRIGGER = 1, /* a=script/trigger ID (0801C670) */
    SSA_PICKUP_SOUND,       /* a=sound ID (08008D5C) */
    SSA_PICKUP_MESSAGE,     /* a=text ID; original remaining args=0,48,0 */
    SSA_PICKUP_REQUEST     /* a=player index, b=item ID (0800580C) */
};
typedef struct SsaPickupOps {
    void (*event)(void *context, enum SsaPickupEvent, unsigned a, unsigned b);
    void (*remove)(void *context, SsaPickup *);
    SsaPickup *(*nearest)(void *context, const SsaPickupPlayer *);
    void *context;
} SsaPickupOps;
typedef struct SsaPickupGame {
    const SsaInventoryRules *rules;
    SsaPickupOps ops;
    uint32_t collected;
    uint8_t player_count, local_player;
} SsaPickupGame;

/* Full dispatcher control flow and native inventory helpers. event/remove are
 * required engine boundaries, nearest is required only for a nonzero override.
 * Normal contacts use override_item=0 and a non-NULL pickup. Network replay
 * selects/removes nearest BEFORE trying to apply its matching item. No new
 * radius/height check is added. Callbacks must not reenter this dispatcher or
 * invalidate player/inventory/pickup storage. Trigger effects may update trigger.
 * Returns -1 integration error (earlier effects may have occurred), 0 ignored or
 * refused, 1 applied, 2 network request emitted. The ROM function returns void.
 * This does not implement scripts, sound, HUD, networking or entity allocation. */
int ssa_pickup_touch(SsaPickupGame *, const SsaPickupPlayer *, SsaPickup *,
                     uint32_t override_item);
/* PC variant acquisition: permit one already-owned weapon to queue again.
 * repeat_weapon=0 retains original acceptance. Other pickup behavior is shared. */
int ssa_pickup_touch_alternate(SsaPickupGame *,const SsaPickupPlayer *,SsaPickup *,
                             uint32_t override_item,unsigned repeat_weapon);
#endif
