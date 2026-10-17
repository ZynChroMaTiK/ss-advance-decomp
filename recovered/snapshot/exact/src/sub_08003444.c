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

s32 sub_08003444(s32 arg0) {
    s32 temp_r0;
    s32 temp_r3;
    void *temp_r2;

    temp_r0 = ((u32) (arg0 + 3) >> 2) * 4;
    temp_r2 = *(void **)0x03004318;
    temp_r3 = M2C_FIELD(temp_r2, s32 *, 4) - temp_r0;
    M2C_FIELD(temp_r2, s32 *, 4) = temp_r3;
    M2C_FIELD(temp_r2, s32 *, 8) = (s32) (M2C_FIELD(temp_r2, s32 *, 8) - temp_r0);
    return temp_r3;
}
