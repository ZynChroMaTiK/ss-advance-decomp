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

M2C_UNK sub_080037E0(void *);                       /* extern */
M2C_UNK sub_08010A20();                             /* extern */

void sub_080111E0(void *arg0) {
    sub_08010A20();
    if (M2C_FIELD(arg0, u8 *, 0x5F) != 0) {
        sub_080037E0(arg0);
    }
}
