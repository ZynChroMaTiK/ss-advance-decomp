#ifndef SSA_SPRITE_DISPATCH_H
#define SSA_SPRITE_DISPATCH_H
#include "ssa/weapon_animation.h"

enum { SSA_SPRITE_CAPACITY = 84 };
typedef struct SsaSpriteDispatch {
    SsaWeaponObject *objects[SSA_SPRITE_CAPACITY];
    uint32_t active[SSA_SPRITE_CAPACITY];
    uint32_t active_count;        /*030055C4*/
    uint32_t counter;             /*030055B0*/
    uint32_t state;               /*030055A4: 1 dispatch, 2 submit, 3 outside*/
    uint32_t current_index;       /*030055A8, logical iteration, not cursor*/
    SsaWeaponObject *current;     /*030055A0, restored to objects[0]*/
    uint8_t submitted_count;      /*03005674, reset by101C4*/
    uint8_t submitted[SSA_SPRITE_CAPACITY]; /*03005620: low bytes of active_index*/
    unsigned updating;
} SsaSpriteDispatch;
typedef struct SsaSpriteDispatchOps {
    int (*handler)(void *, SsaSpriteDispatch *, uint32_t entry, unsigned slot);
    /* Optional observation AFTER native101D0 appends the low byte of the
     * object's active_index. Receives the object passed to101D0. No renderer
     * or presentation implementation is required at this boundary. */
    int (*submit)(void *, SsaSpriteDispatch *, SsaWeaponObject *);
    void *context;
} SsaSpriteDispatchOps;

uint32_t ssa_sprite_dispatch_handler(uint16_t type);
/* NativeF85C+101C4+101D0. All84 pointers must be nonnull and stable, including empty
 * slots. Callbacks may edit payloads, active entries/count and projected globals,
 * and redirect current to another bound object. Do not mutate objects[] bindings,
 * ops or updating, free referenced storage, or nest step. Count <=84 throughout.
 * submitted_count must remain <=84. An append at count84 returns an error rather
 * than overwriting adjacent GBA globals; only callback mutations can cause this
 * for a valid <=84 iteration frame. Queue bytes past the written prefix persist.
 * Loop index always increments; physical cursor advances only for post-handler
 * current.type!=0, after submit. Livecount rereads include appends/deletions.
 * Return0 complete/-1 invalid or callbackfailure. Earlier effects persist; after
 * entry succeeds, errors restore state3/currentobjects[0] and clearupdating.
 * No allocation, controller, animation or render implementation is implied. */
int ssa_sprite_dispatch_step(SsaSpriteDispatch *, const SsaSpriteDispatchOps *);
#endif
