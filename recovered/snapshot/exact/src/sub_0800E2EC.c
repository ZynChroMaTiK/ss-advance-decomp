#include "common.h"

/* machine draft (m2c), tried by autopilot */
#ifndef M2C_DEFINES
#define M2C_DEFINES
typedef s32 M2C_UNK;
typedef s8 M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
#define M2C_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(expr))
#define M2C_ERROR(desc) (0)
#define M2C_CARRY 0
#define M2C_OVERFLOW(a) (0)
#endif

M2C_UNK sub_0800E1CC();                             /* extern */
M2C_UNK sub_0800E244(u16, u16);                     /* extern */
void *sub_0800E274(u16);                            /* extern */
M2C_UNK sub_0800E2B0();                             /* extern */

void sub_0800E2EC(u16 arg0) {
    u16 temp_r4;
    u16 temp_r5;
    void *temp_r0;

    temp_r5 = arg0;
    temp_r0 = sub_0800E274(temp_r5);
    if (temp_r0 != NULL) {
        temp_r4 = M2C_FIELD(temp_r0, u16 *, 2);
        sub_0800E2B0();
        *(u16 *)0x03005564 -= 1;
        sub_0800E244(temp_r5, temp_r4);
        sub_0800E1CC();
    }
}
