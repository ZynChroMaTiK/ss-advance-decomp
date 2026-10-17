typedef struct ProjectileRecord {
    s32 x, y, z;
    s32 vx, vy, vz;
    u8 pad18[12];
    s32 speed;
} ProjectileRecord;
extern ProjectileRecord *sub_08010830(s32 type, void *source, s32 forward,
                                     s32 side, s32 z, s32 heading);
extern s32 sub_0800036C(void *, s32, s32);
extern u8 D_030000F0[];

ProjectileRecord *sub_080109F0(s32 type, void *source, s32 forward,
                              s32 side, s32 z, s32 heading, s32 slope) {
    ProjectileRecord *projectile;
    projectile = sub_08010830(type, source, forward, side, z, heading);
    if (projectile != NULL) {
        projectile->vz = sub_0800036C(D_030000F0, projectile->speed, slope);
    }
    return projectile;
}

