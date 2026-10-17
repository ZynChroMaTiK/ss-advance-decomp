#ifndef SSA_MODE_CONTROL_H
#define SSA_MODE_CONTROL_H
#include <stdint.h>

/* Borrowed scalar projections used by original080021E4. Storage and bindings
 * must remain stable; callbacks may change scalars, including live player_count
 * and later inputs. No display-local/BC-active filtering occurs in this routine. */
typedef struct SsaModeControl {
    uint16_t *flags;          /* GAME+04 */
    uint16_t *input_state;    /* input030042F0+14 */
    uint16_t *menu_timer;     /* GAME+76 */
    uint8_t *player_count;    /* GAME+10 */
    uint8_t *selection;       /* GAME+72 */
    const uint32_t *inputs[4];/* player+A4, packed held/pressed */
    unsigned updating;
} SsaModeControl;

typedef enum SsaModeControlEvent {
    SSA_MODE_COMMAND = 0x08008EB0,
    SSA_MODE_CREATE_SPRITE = 0x0800E914
} SsaModeControlEvent;
typedef struct SsaModeControlOps {
    /* COMMAND: a=original command ID, other args0. CREATE_SPRITE: original
     * r0..r3. Does not invent semantics for the remaining UI dispatcher. */
    int (*event)(void *, SsaModeControl *, SsaModeControlEvent,
                 uint32_t a, uint32_t b, uint32_t c, uint32_t d);
    void *context;
} SsaModeControlOps;

/* Whole021E4 pause/menu state orchestration, with both original callees explicit.
 * No nested update. Return0 complete,-1 invalid/callback failure. Prefix effects
 * persist on failure and guard clears. Callbacks must not change bindings, ops,
 * or updating, and must retain all borrowed storage. No OS/menu rendering here. */
int ssa_mode_control_step(SsaModeControl *, const SsaModeControlOps *);
#endif
