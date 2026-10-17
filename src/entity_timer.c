#include "ssa/entity_timer.h"

int ssa_entity_timer_step(uint16_t *flags, uint16_t *timer, unsigned ticks) {
    uint16_t remaining;
    if (!flags || !timer || flags == timer || ticks > 255u) return -1;
    remaining = (uint16_t)((unsigned)*timer - ticks);
    *timer = remaining;
    if (remaining != 0 && remaining < 0x8000u) return 0;
    if (*flags & 0x80u) {
        *flags = (uint16_t)(*flags & 0xff7fu);
        *timer = 30;
        return 0;
    }
    *timer = 0;
    return 1;
}
