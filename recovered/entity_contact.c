/* Original callback behavior, including the X-as-height overlap quirk. */
typedef struct ContactEntity {
    s32 x, y, z;
    u8 pad0c[0x1c];
    u32 radius;
    s32 height;
    u16 flags;
    u8 pad32[10];
    u16 type;
    u8 pad3e[0x16];
    u8 player_index;
} ContactEntity;
extern ContactEntity *D_03004330, *D_03004328;
extern s32 D_03004334, D_03004438;
extern void sub_0801B99C(ContactEntity *, ContactEntity *, s32);

s32 sub_08003D7C(ContactEntity *other)
{
    ContactEntity *initial = D_03004330;
    ContactEntity *current;
    u32 radius;
    s32 distance;
    u32 flags;
    if (other == initial) return 1;
    flags = other->flags;
    if ((flags & 0x10) && (initial->flags & 0x10)) return 1;
    if (flags & 0x20) return 1;
    if ((flags & 1) && D_03004330->type == 0x67) return 1;
    if ((flags & 0x10) && (D_03004330->flags & 0x100)
        && D_03004330->player_index - 1 == other->player_index) return 1;
    if (other->type == 0xe1) {
        sub_0801B99C(D_03004330, other, 0);
        return 1;
    }
    current = D_03004330;
    radius = other->radius + current->radius;
    distance = other->x - D_03004334;
    if (distance < 0) distance = -distance;
    if ((u32)distance >= radius) return 1;
    distance = other->y - D_03004438;
    if (distance < 0) distance = -distance;
    if ((u32)distance >= radius) return 1;
    if (current->flags & 0x200) {
        if (current->x > other->z + other->height) return 1;
        if (current->x + current->height < other->z) return 1;
    }
    D_03004328 = other;
    return 0;
}


