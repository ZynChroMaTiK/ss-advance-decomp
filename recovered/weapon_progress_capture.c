/* NONMATCHING: 86.8% through decomp.wtf; native behavior is tested separately. */
typedef struct CaptureWeaponRecord {
    u32 current;
    void *object;
    u32 flags;
    u16 owned;
    u16 ammo[10];
    u8 padding[0x2e];
    u16 requested;
} CaptureWeaponRecord;
typedef struct CaptureGame { u8 padding[0x90]; s32 weapons[10][4]; } CaptureGame;
typedef struct CaptureSettings { u8 padding[0x1b]; u8 setting; } CaptureSettings;
extern CaptureWeaponRecord D_03005680;
extern CaptureGame *D_03004010;
extern CaptureSettings *D_0300572C;
void sub_0801231C(CaptureWeaponRecord *weapon) {
    u16 *ammo;
    u16 *source;
    u16 *target;
    CaptureWeaponRecord *saved;
    s32 i;
    D_03005680.current = weapon->current;
    D_03005680.owned = weapon->owned;
    i = 0;
    saved = &D_03005680;
    ammo = weapon->ammo;
    source = ammo;
    target = saved->ammo;
    do {
        *target = *source;
        ++source;
        ++target;
        ++i;
    } while (i <= 9U);
    saved->requested = weapon->requested;
    i = 0;
    do {
        CaptureGame *game = D_03004010;
        game->weapons[i][0] = ((s32)weapon->owned >> i) & 1;
        game->weapons[i][1] = *(s16 *)((u8 *)weapon + 14 + i * 2);
        if (D_0300572C->setting) {
            game->weapons[i][2] = 0;
            game->weapons[i][3] = 0;
        }
        ++i;
    } while (i <= 9U);
}
