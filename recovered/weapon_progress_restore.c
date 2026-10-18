typedef struct WeaponProgressRecord {
    u32 current;
    void *object;
    u32 flags;
    u16 owned;
    u16 ammo[10];
    u8 padding[0x2e];
    u16 requested;
} WeaponProgressRecord;
extern WeaponProgressRecord D_03005680;
extern void sub_0800FC04(void *, u16, u16);
void sub_080123C4(WeaponProgressRecord *weapon) {
    u16 *source;
    u16 *target;
    u32 i;
    weapon->current = D_03005680.current;
    weapon->owned = D_03005680.owned;
    i = 0;
    source = D_03005680.ammo;
    target = weapon->ammo;
    do {
        *target = *source;
        ++source;
        ++target;
        ++i;
    } while (i <= 9);
    weapon->requested = weapon->current;
    sub_0800FC04(weapon->object, (u16)(weapon->current - 1), 3);
}
