#include "ssa/movement.h"
#include <limits.h>

/* Explicit ARM-width arithmetic avoids C signed-overflow and shift UB. */
static int32_t signed_bits(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)~v;
}
static int32_t add(int32_t a, int32_t b) {
    return signed_bits((uint32_t)a + (uint32_t)b);
}
static int32_t sub(int32_t a, int32_t b) {
    return signed_bits((uint32_t)a - (uint32_t)b);
}
static int32_t shift16(int32_t v) {
    uint32_t bits = (uint32_t)v;
    return signed_bits((bits >> 16) | (v < 0 ? UINT32_C(0xffff0000) : 0));
}
static int32_t multiply_fixed(int32_t a, int32_t b) {
    int64_t p = (int64_t)a * b;
    int64_t q = p / 65536;
    if (p < 0 && p % 65536) --q;
    return signed_bits((uint32_t)q);
}
static int32_t decay(int32_t value, int32_t step) {
    if (value > 0) {
        value = sub(value, step);
        return value < 0 ? 0 : value;
    }
    value = add(value, step);
    return value > 0 ? 0 : value;
}
static uint16_t read16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}
int ssa_math_tables_load(SsaMathTables *t, const uint8_t *rom, size_t size) {
    size_t i;
    if (!t || !rom || size < 0x34932) return 0;
    for (i = 0; i < 16384; ++i) t->sine_quarter[i] = read16(rom + 0x1c930 + 2*i);
    for (i = 0; i < 32769; ++i) t->direction[i] = read16(rom + 0x24930 + 2*i);
    return 1;
}
void ssa_sincos(const SsaMathTables *t, uint16_t heading, int32_t *sine, int32_t *cosine) {
    unsigned q = heading >> 14, r = heading & 0x3fff;
    int32_t s, c;
    if (!r) {
        s = q == 1 ? 65536 : q == 3 ? -65536 : 0;
        c = q == 0 ? 65536 : q == 2 ? -65536 : 0;
    } else {
        int32_t a = t->sine_quarter[r], b = t->sine_quarter[0x4000-r];
        s = q == 0 ? a : q == 1 ? b : q == 2 ? -a : -b;
        c = q == 0 ? b : q == 1 ? -a : q == 2 ? -b : a;
    }
    if (sine) *sine = s;
    if (cosine) *cosine = c;
}
uint16_t ssa_direction(const SsaMathTables *t, int16_t x, int16_t y) {
    int32_t ax = x < 0 ? -(int32_t)x : x, ay = y < 0 ? -(int32_t)y : y;
    int32_t ratio, angle;
    if (!(ax + ay)) return 0;
    ratio = (ax * 16384) / (ax + ay);
    angle = t->direction[x < 0 ? 16384-ratio : 16384+ratio];
    if (y > 0) angle = -angle;
    return (uint16_t)(angle + 16384);
}

void ssa_movement_controls(SsaMovement *s, uint8_t ticks) {
    uint32_t k = s->keys;
    int32_t step = (int32_t)ticks * 65536, v;
    if (!(s->flags & 0x10)) {
        if (k & 2) {
            s->modifier_ticks = (uint16_t)(s->modifier_ticks + ticks);
            if (k & 0x02000000) s->flags |= 0x80;
            else if (k & 0x01000000) s->flags |= 0x40;
        } else {
            if ((uint16_t)(s->modifier_ticks - 1) <= 14) s->flags |= 0x40;
            s->modifier_ticks = 0;
        }
    }
    s->heading = (uint16_t)(s->heading + shift16(signed_bits((uint32_t)s->turn_velocity * ticks)));
    if (!(s->weapon_flags & 0x100)) {
        int32_t acceleration = (s->modifier_ticks > 8 ? 0x80000 : 0x400000) * (int32_t)ticks;
        v = s->turn_velocity;
        if ((k & 0x20) && !(s->flags & 0x10)) {
            v = sub(v, acceleration);
            if (v < -0x1900000) v = -0x1900000;
        } else if ((k & 0x10) && !(s->flags & 0x10)) {
            v = add(v, acceleration);
            if (v > 0x1900000) v = 0x1900000;
        } else if (v > 0) {
            v = sub(v, (v > 0x640000 ? 0x300000 : 0x180000) * (int32_t)ticks);
            if (v < 0) v = 0;
        } else {
            v = add(v, (v < -0x640000 ? 0x300000 : 0x100000) * (int32_t)ticks);
            if (v > 0) v = 0;
        }
        s->turn_velocity = v;
    }
    if (k & 2) s->flags &= (uint16_t)~3;
    else if ((k & 0x300) != 0x300) {
        if (k & 0x02000000) s->flags = (uint16_t)((s->flags & ~2) | 1);
        else if (k & 0x01000000) s->flags = (uint16_t)((s->flags & ~1) | 2);
        else if (k & 0x200) s->flags = (uint16_t)((s->flags & ~2) | 1);
        else if (k & 0x100) s->flags = (uint16_t)((s->flags & ~1) | 2);
        else s->flags &= (uint16_t)~3;
    }
    v = s->forward_velocity;
    if ((k & 0xc0) && !(s->flags & 0x10)) {
        if (k & 0x40) {
            v = add(v, k & 2 ? 0x2000 : step);
            if (v > 0x80000) v = k & 2 ? 0x10000 : 0x80000;
        } else {
            v = sub(v, step);
            if (v < -0x80000) v = -0x80000;
        }
    } else v = decay(v, step);
    s->forward_velocity = v;
    v = s->strafe_velocity;
    if ((s->flags & 3) && !(s->flags & 0x10)) {
        if (s->flags & 1) {
            v = add(v, step);
            if (v > 0x80000) v = 0x80000;
        } else {
            v = sub(v, step);
            if (v < -0x80000) v = -0x80000;
        }
    } else v = decay(v, step);
    s->strafe_velocity = v;
    v = s->impulse;
    if (v > 0) v = sub(v, (int32_t)ticks * 4096);
    s->impulse = v < 0 ? 0 : v;
}

void ssa_movement_vector(SsaMovement *s, const SsaMathTables *t) {
    int32_t x = 0, y = 0, sx = 0, sy = 0, ix = 0, iy = 0;
    uint32_t ax, ay;
    if (s->forward_velocity) {
        ssa_sincos(t, s->heading, &x, &y);
        x = multiply_fixed(x, s->forward_velocity);
        y = multiply_fixed(y, s->forward_velocity);
    }
    if (s->strafe_velocity) {
        ssa_sincos(t, (uint16_t)(s->heading - 0x4000), &sx, &sy);
        sx = multiply_fixed(sx, s->strafe_velocity);
        sy = multiply_fixed(sy, s->strafe_velocity);
    }
    if (s->impulse) {
        ssa_sincos(t, s->impulse_heading, &ix, &iy);
        /* Original stores scaled impulse into the strafe temporaries, then
         * adds the unscaled direction. Preserve that observed behavior. */
        sx = multiply_fixed(ix, s->impulse);
        sy = multiply_fixed(iy, s->impulse);
    }
    s->velocity_x = add(x, add(sx, ix));
    s->velocity_y = add(y, add(sy, iy));
    ax = s->velocity_x < 0 ? 0u - (uint32_t)s->velocity_x : (uint32_t)s->velocity_x;
    ay = s->velocity_y < 0 ? 0u - (uint32_t)s->velocity_y : (uint32_t)s->velocity_y;
    if (ax + ay == 0)
        s->move_heading = s->heading;
    else s->move_heading = ssa_direction(t, (int16_t)shift16(s->velocity_x), (int16_t)shift16(s->velocity_y));
}
