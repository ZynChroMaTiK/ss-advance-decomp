#include "ssa/horizontal.h"
#include <limits.h>
static int32_t add(int32_t a,int32_t b) {
    uint32_t n=(uint32_t)a+(uint32_t)b;
    return n<=INT32_MAX ? (int32_t)n : -1-(int32_t)~n;
}
static int32_t clamp(int32_t v) {
    return v>30*65536 ? 30*65536 : v< -30*65536 ? -30*65536 : v;
}
int ssa_horizontal_step(SsaBody *b,uint8_t index,uint8_t ticks,uint8_t count,uint8_t local,const SsaHorizontalOps *ops) {
    int32_t vx,vy;
    unsigned i;
    if (!b) return -1;
    if (!b->velocity_x && !b->velocity_y) return 0;
    if ((b->flags&0x10) && count>1 && index!=local) return 0;
    b->velocity_x=clamp(b->velocity_x); b->velocity_y=clamp(b->velocity_y);
    vx=b->velocity_x; vy=b->velocity_y;
    if (ticks && (!ops || !ops->try_move || !ops->blocked_by_entity)) return -1;
    for (i=0;i<ticks;++i) {
        int32_t x=add(b->x,vx),y=add(b->y,vy);
        int result=ops->try_move(ops->context,b,x,y);
        if (result<0) return -1;
        if (result) continue;
        if (b->flags&8) {
            int blocked=ops->blocked_by_entity(ops->context);
            if (blocked<0) return -1;
            if (!blocked) {
                if (!ops->slide || ops->slide(ops->context,b)<0) return -1;
                continue;
            }
            result=ops->try_move(ops->context,b,b->x,y);
            if (result<0) return -1;
            if (result) { b->velocity_x=0; continue; }
            result=ops->try_move(ops->context,b,x,b->y);
            if (result<0) return -1;
            if (result) { b->velocity_y=0; continue; }
        }
        b->velocity_x=b->velocity_y=0;
    }
    return 0;
}
