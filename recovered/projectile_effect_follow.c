typedef struct AttachedEffect {
    s32 x, y, z;
    u8 pad0c[0x14];
    struct AttachedEffect *target;
    s32 speed, radius, height;
    u16 flags;
} AttachedEffect;
extern void sub_0800A65C(AttachedEffect *);
extern void sub_0800A5D0(AttachedEffect *);
extern void sub_080037E0(AttachedEffect *);

void sub_080113E4(AttachedEffect *effect) {
    AttachedEffect *target = effect->target;
    if (!(0x20 & target->flags)) {
        sub_0800A65C(effect);
        effect->x = target->x;
        effect->y = target->y;
        effect->z = target->z + target->height + 0x300000;
        sub_0800A5D0(effect);
    } else {
        sub_080037E0(effect);
    }
}
