typedef struct {
    s32 x, y, z, vx, vy, vz;
    u8 padding[0xa0];
    u16 heading;
} SyncPlayer;
typedef struct {
    s32 x, y, z, vx, vy, vz;
    u8 padding[0x27];
    u8 facing;
} SyncEntity;
void sub_08005E28(SyncEntity *entity, SyncPlayer *player) {
    player->x=entity->x;
    player->y=entity->y;
    player->z=entity->z;
    player->vx=entity->vx;
    player->vy=entity->vy;
    player->vz=entity->vz;
    player->heading=entity->facing<<8;
}
