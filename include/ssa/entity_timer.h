#ifndef SSA_ENTITY_TIMER_H
#define SSA_ENTITY_TIMER_H
#include <stdint.h>

/* Original 08012CFC, also called by the E1 update wrapper 0801B848.
 * flags is entity+30, timer is entity+4E; these are host field pointers.
 * ticks is the original unsigned byte timestep, 0..255.
 * Subtraction wraps to 16 bits before testing signed positivity. Expiration
 * clears flag 0x80 and resets the timer to 30 if that flag was set, returning 0.
 * Otherwise expiration zeros the timer and returns 1. A running timer returns 0.
 * No activation, visibility, AI or destruction is performed by this routine.
 * Returns -1 without mutation for null/aliased pointers or ticks > 255. */
int ssa_entity_timer_step(uint16_t *flags, uint16_t *timer, unsigned ticks);
#endif
