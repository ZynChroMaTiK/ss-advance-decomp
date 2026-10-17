/* Matching candidate: original behavior only. Native jumping is separate. */
typedef union PhysicsFixed {
    s32 value;
    struct { u16 low; s16 high; } part;
} PhysicsFixed;
typedef struct PhysicsEntity {
    s32 x, y;
    PhysicsFixed z;
    s32 vx, vy, vz;
    u8 pad18[0x14];
    s32 height;
    u16 flags;
    u8 pad32[0x11];
    s8 hover;
    u8 pad44[0x10];
    u8 player_index;
    u8 pad55[0x1B];
    s16 ceiling, floor;
} PhysicsEntity;
typedef struct PhysicsPlayer {
    u8 pad0[0xBC];
    u16 flags;
    u8 padBE[0x12];
    s32 eye_offset, eye_velocity;
    u8 padD8[0x28];
} PhysicsPlayer;
typedef struct PhysicsGame { u8 pad0[7]; u8 ticks; } PhysicsGame;
extern PhysicsPlayer *D_03004648;
extern PhysicsGame *D_03004010;

s32 sub_08004CE0(PhysicsEntity *body)
{
    s32 airborne = 1;
    s32 old_velocity;
    u32 ticks;
    if (body->flags & 0x10) {
        PhysicsPlayer *player = &D_03004648[body->player_index];
        player->flags &= ~0x20;
        if (body->z.part.high < body->floor) {
            player->eye_offset -= body->floor - body->z.part.high;
            player->eye_velocity = (0x600000 - player->eye_offset) >> 3;
        } else if (body->z.part.high == body->floor) player->flags |= 0x20;
    }
    ticks = D_03004010->ticks;
    old_velocity = body->vz;
    body->z.value += ticks * old_velocity;
    if ((body->z.value >> 16) <= body->floor) {
        body->z.value = body->floor << 16;
        airborne = 0;
        if (old_velocity < 0) body->vz = 0;
    }
    if (!(body->flags & 0x100)) {
        if (body->hover) {
            if (body->z.value < ((body->floor + body->hover) << 16))
                body->vz += D_03004010->ticks << 13;
            else body->vz -= D_03004010->ticks << 13;
        } else if (!body->vz) body->vz = -(D_03004010->ticks << 15);
        else body->vz -= D_03004010->ticks << 14;
    }
    if (((body->z.value + body->height) >> 16) > body->ceiling) {
        if (body->vz > 0) body->vz = 0;
        body->z.value = (body->ceiling << 16) - body->height;
    }
    return airborne;
}

