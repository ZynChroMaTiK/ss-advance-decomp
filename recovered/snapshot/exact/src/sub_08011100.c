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

M2C_UNK sub_08005394(void *, s32, s32);             /* extern */
M2C_UNK sub_080110AC(void *);                       /* extern */

void sub_08011100(void *arg0) {
    sub_08005394(arg0, 0x87, 5);
    M2C_FIELD(arg0, s8 *, 0x3E) = 1;
    sub_080110AC(arg0);
}
