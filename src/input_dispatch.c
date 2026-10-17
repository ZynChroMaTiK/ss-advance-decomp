#include "ssa/input_dispatch.h"

int ssa_input_dispatch_local(const SsaInputDispatch *d,uint32_t packed_keys) {
    unsigned i;
    if(!d) return SSA_INPUT_DISPATCH_INVALID;
    if(d->player_count>1 && !(d->game_flags&0x100u) && !d->exit_flag)
        return SSA_INPUT_DISPATCH_NETWORK_REQUIRED;
    for(i=0;i<4;++i)
        if(!d->players[i].flags || !d->players[i].keys)
            return SSA_INPUT_DISPATCH_INVALID;
    for(i=0;i<4;++i)
        if((*d->players[i].flags&4u) && i==d->motion_index)
            *d->players[i].keys=packed_keys;
    return SSA_INPUT_DISPATCH_OK;
}
