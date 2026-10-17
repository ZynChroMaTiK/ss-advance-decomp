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

M2C_UNK sub_08018EE4(s32, s32);                     /* extern */

void sub_080193D8(void *arg0) {
    s32 temp_r1;
    s32 var_r1;
    s32 var_r2;
    u32 var_r5;
    void *var_r3;

    M2C_FIELD(arg0, s32 *, 0x10) = (s32) (M2C_FIELD(arg0, s32 *, 0x10) + M2C_FIELD(arg0, s32 *, 0x18));
    M2C_FIELD(arg0, s32 *, 0x14) = (s32) (M2C_FIELD(arg0, s32 *, 0x14) + M2C_FIELD(arg0, s32 *, 0x1C));
    var_r3 = M2C_FIELD(arg0, void **, 0x3C);
    var_r5 = 0;
    do {
        var_r2 = M2C_FIELD(arg0, s16 *, 0x12) + ((1 & var_r5) << 6);
        var_r1 = M2C_FIELD(arg0, s16 *, 0x16) + ((2 & var_r5) << 5);
        if (var_r2 > 0xF0) {
            var_r2 = 0xF0;
        } else if (var_r2 < -0x40) {
            var_r2 = -0x40;
        }
        if (var_r1 > 0xA0) {
            var_r1 = 0xA0;
        } else if (var_r1 < -0x40) {
            var_r1 = -0x40;
        }
        M2C_FIELD(var_r3, s16 *, 0) = (s16) var_r2;
        M2C_FIELD(var_r3, s16 *, 2) = (s16) var_r1;
        var_r5 += 1;
        var_r3 += 0x14;
    } while (var_r5 <= 3U);
    temp_r1 = M2C_FIELD(arg0, s32 *, 0x24);
    if (temp_r1 != M2C_FIELD(arg0, s32 *, 0x20)) {
        sub_08018EE4(M2C_FIELD(arg0, s32 *, 0x44) + (temp_r1 << 0xE), M2C_FIELD(arg0, s32 *, 0x40));
        M2C_FIELD(arg0, s32 *, 0x20) = (s32) M2C_FIELD(arg0, s32 *, 0x24);
    }
}
