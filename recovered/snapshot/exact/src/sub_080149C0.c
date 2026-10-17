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
s32 sub_08014518(void *, s32);                      /* extern */

void sub_080149C0(void *arg0) {
    s32 var_r1;

    var_r1 = sub_08014518(arg0, M2C_FIELD(*(void **)0x03004648, s32 *, 0xAC));
    if (var_r1 < 0) {
        var_r1 = 0 - var_r1;
    }
    if (var_r1 <= 0x3FF) {
        M2C_FIELD(arg0, s8 *, 0x3E) = 2;
        sub_08005394(arg0, 0x56, 0xA);
    }
}
