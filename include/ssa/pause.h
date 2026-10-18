#ifndef SSA_PAUSE_H
#define SSA_PAUSE_H
#include <stdint.h>
/* Single-player, non-network branch of021E4. Input is original packed edges.
 * Events below256 are8EB0 commands;256+id requests E914(id,0,56,0).
 * The host owns frozen presentation and maps PC navigation to these edges. */
typedef struct SsaPause {
    uint16_t flags,timer,input_state;
    uint8_t selection;
    unsigned events[8],event_count;
} SsaPause;
int ssa_pause_step(SsaPause *,uint32_t input);
#endif
