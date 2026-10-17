#include "common.h"

/* Original09238 wrapper. See analysis/actor-assets.md for native pixel core. */
extern u8 *D_03004754;
extern void sub_08009258(void *,const s16 *);
void sub_08009238(u16 index,const s16 *clips)
{
    sub_08009258(D_03004754+36*index,clips);
}
