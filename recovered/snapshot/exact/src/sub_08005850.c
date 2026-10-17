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

void sub_08005850(u16 arg0, u16 arg1) {
    u16 temp_r2;
    void *temp_r1;
    void *temp_r1_2;

    temp_r1 = *(s32 *)0x03004648 + (M2C_FIELD(*(void **)0x03004010, u8 *, 0x68) << 8);
    temp_r2 = M2C_FIELD(temp_r1, u16 *, 0xF8);
    temp_r1_2 = temp_r1 + ((temp_r2 * 4) + 0xE8);
    if ((u32) temp_r2 <= 3U) {
        M2C_FIELD(temp_r1_2, u16 *, 0) = arg0;
        M2C_FIELD(temp_r1_2, s16 *, 2) = (s16) (0xFF & arg1);
        M2C_FIELD(temp_r1, u16 *, 0xF8) = (u16) (M2C_FIELD(temp_r1, u16 *, 0xF8) + 1);
    }
}
