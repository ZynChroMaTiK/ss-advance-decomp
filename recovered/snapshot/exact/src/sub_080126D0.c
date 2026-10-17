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

M2C_UNK sub_080093E8(s32, s32, s32 *);              /* extern */

s32 sub_080126D0(void *arg0) {
    s32 sp0;
    void *temp_r2;

    temp_r2 = M2C_FIELD(*(void **)0x03004648, void **, 0xAC);
    sub_080093E8((s32) (M2C_FIELD(temp_r2, s32 *, 0) - M2C_FIELD(arg0, s32 *, 0)) >> 0x10, (s32) (M2C_FIELD(temp_r2, s32 *, 4) - M2C_FIELD(arg0, s32 *, 4)) >> 0x10, &sp0);
    return sp0;
}
