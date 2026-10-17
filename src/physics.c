#include "ssa/physics.h"
#include <limits.h>

static int32_t bits(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)~v;
}
static int32_t add(int32_t a, int32_t b) { return bits((uint32_t)a + (uint32_t)b); }
static int32_t sub(int32_t a, int32_t b) { return bits((uint32_t)a - (uint32_t)b); }
static int32_t asr(int32_t v, unsigned n) {
    return bits(((uint32_t)v >> n) | (v < 0 ? UINT32_MAX << (32-n) : 0));
}
static int32_t fixed_mul(int32_t a, int32_t b) {
    int64_t product = (int64_t)a * b;
    int64_t quotient = product / 65536;
    if (product < 0 && product % 65536) --quotient;
    return bits((uint32_t)quotient);
}

int ssa_camera_step(const SsaBody *b, SsaPlayerVertical *p, SsaCamera *c,
                     SsaCameraFrame *f, const SsaMathTables *tables) {
    int32_t bob = 0;
    int event = 0;
    if (!(p->flags & SSA_PLAYER_CONTROL_LOCK)) {
        int32_t sine;
        c->bob_amplitude = asr(add(fixed_mul(b->velocity_x,b->velocity_x),
                                   fixed_mul(b->velocity_y,b->velocity_y)),3);
        if (c->bob_amplitude > 0x100000) c->bob_amplitude = 0x100000;
        ssa_sincos(tables,(uint16_t)(f->counter * 1872u),&sine,0);
        bob = fixed_mul(c->bob_amplitude / 2,sine);
        if (f->is_local) {
            if (!f->footsteps_muted) {
                if (f->previous_bob < 0 && bob >= 0) event = 13;
                else if (f->previous_bob >= 0 && bob < 0) event = 14;
            }
            f->previous_bob = bob;
        }
    }
    if (p->flags & 4) {
        p->eye_offset = add(p->eye_offset,p->eye_velocity);
        if (p->eye_offset > 0x600000) {
            p->eye_offset = 0x600000;
            p->eye_velocity = 0;
        }
        if (p->eye_offset < 0x180000) {
            p->eye_offset = 0x180000;
            if (p->eye_velocity <= 0) p->eye_velocity = 1;
        }
        if (p->eye_velocity) {
            p->eye_velocity = add(p->eye_velocity,0x4000);
            if (!p->eye_velocity) p->eye_velocity = 1;
        }
    }
    if (p->lowered) bob = -0x140000;
    c->eye_z = add(add(b->z,p->eye_offset),bob);
    if (b->step_adjust && !(p->flags & SSA_PLAYER_CONTROL_LOCK) && b->z <= b->floor)
        c->eye_z = sub(c->eye_z,(int32_t)b->step_adjust * 65536);
    /* The original compares whole units but assigns these mixed-unit sums. */
    if (asr(c->eye_z,16) > b->ceiling - 4) c->eye_z = b->ceiling - 0x40000;
    if (asr(c->eye_z,16) < b->floor + 4) c->eye_z = b->floor + 0x40000;
    return event;
}

int ssa_vertical_step(SsaBody *b, SsaPlayerVertical *p, uint8_t ticks) {
    int airborne = 1;
    int32_t old_velocity, z;
    if ((b->flags & SSA_BODY_PLAYER) && p) {
        int32_t whole_z = asr(b->z, 16);
        p->flags &= (uint16_t)~SSA_PLAYER_FLOOR_CONTACT;
        if (whole_z < b->floor) {
            /* Original subtracts integer height here, without a <<16. */
            p->eye_offset = sub(p->eye_offset, b->floor - whole_z);
            p->eye_velocity = asr(sub(0x600000, p->eye_offset), 3);
        } else if (whole_z == b->floor) p->flags |= SSA_PLAYER_FLOOR_CONTACT;
    }
    old_velocity = b->velocity_z;
    b->z = add(b->z, bits((uint32_t)old_velocity * ticks));
    if (asr(b->z, 16) <= b->floor) {
        b->z = (int32_t)b->floor * 65536;
        airborne = 0;
        if (old_velocity < 0) b->velocity_z = 0;
    }
    z = b->z;
    if (!(b->flags & SSA_BODY_NO_GRAVITY)) {
        if (b->hover_offset) {
            int32_t target = bits((uint32_t)(b->floor + b->hover_offset) << 16);
            int32_t change = (int32_t)ticks * 8192;
            b->velocity_z = z < target ? add(b->velocity_z, change) : sub(b->velocity_z, change);
        } else if (!b->velocity_z) b->velocity_z = -(int32_t)ticks * 32768;
        else b->velocity_z = sub(b->velocity_z, (int32_t)ticks * 16384);
    }
    if (asr(add(z, b->height), 16) > b->ceiling) {
        if (b->velocity_z > 0) b->velocity_z = 0;
        b->z = sub((int32_t)b->ceiling * 65536, b->height);
    }
    return airborne;
}

void ssa_jump_init(SsaJump *j) {
    j->launch_velocity = SSA_JUMP_DEFAULT_VELOCITY;
    j->was_down = 0;
}

int ssa_player_vertical_step(SsaBody *b, SsaPlayerVertical *p, SsaJump *j,
                             uint8_t ticks, int jump_down, int controls_enabled) {
    unsigned tick;
    int began = 0, edge;
    int32_t floor_z = (int32_t)b->floor * 65536;
    int32_t ceiling_z = (int32_t)b->ceiling * 65536;
    if (!ticks) return 0;
    edge = jump_down && !j->was_down;
    j->was_down = (uint8_t)(jump_down != 0);
    if (edge && controls_enabled && p && (b->flags & SSA_BODY_PLAYER) &&
        !(p->flags & SSA_PLAYER_CONTROL_LOCK) &&
        !(b->flags & SSA_BODY_NO_GRAVITY) && !b->hover_offset &&
        b->z == floor_z && b->velocity_z <= 0 && b->height > 0 &&
        (int64_t)b->z + b->height < ceiling_z && j->launch_velocity > 0) {
        b->velocity_z = j->launch_velocity > SSA_JUMP_MAX_VELOCITY ?
                         SSA_JUMP_MAX_VELOCITY : j->launch_velocity;
        p->flags &= (uint16_t)~SSA_PLAYER_FLOOR_CONTACT;
        began = 1;
    }
    for (tick = 0; tick < ticks; ++tick) {
        ssa_vertical_step(b, p, 1);
        /* For the new jump path, clamp exact fixed-point head contact. The
         * original only compares integer heights and can overshoot fractionally.
         * Only apply when a body fits; invalid map clearance remains a caller
         * collision/map-validation concern, not permission to pass through floors. */
        if (b->height > 0 && (int64_t)ceiling_z - floor_z >= b->height &&
            (int64_t)b->z + b->height >= ceiling_z) {
            b->z = ceiling_z - b->height;
            if (b->velocity_z > 0) b->velocity_z = 0;
        }
        if ((b->flags & SSA_BODY_PLAYER) && p) {
            p->flags &= (uint16_t)~SSA_PLAYER_FLOOR_CONTACT;
            if (b->z == floor_z && b->velocity_z <= 0) p->flags |= SSA_PLAYER_FLOOR_CONTACT;
        }
    }
    return began;
}
