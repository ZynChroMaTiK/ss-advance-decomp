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

M2C_UNK sub_08015708(s32, u16);                     /* extern */
M2C_UNK sub_08015798(s32, s32);                     /* extern */

void sub_08015868(s32 arg0, s32 arg1, u16 arg2) {
    sub_08015708(0xC0, arg2);
    sub_08015798(arg0, arg1);
}
